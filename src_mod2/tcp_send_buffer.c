#include <string.h>

#include "memory_mgt.h"
#include "debug.h"
#include "tcp_send_buffer.h"
#include "tcp_sb_queue.h"
#include <math.h>
#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

/*----------------------------------------------------------------------------*/
struct sb_manager
{
	size_t chunk_size;
	uint32_t cur_num;
	uint32_t cnum;
	mem_pool_t mp;
	sb_queue_t freeq;

} sb_manager;
/*----------------------------------------------------------------------------*/
uint32_t 
SBGetCurnum(sb_manager_t sbm)
{
	return sbm->cur_num;
}
/*----------------------------------------------------------------------------*/
sb_manager_t 
SBManagerCreate(size_t chunk_size, uint32_t cnum)
{
	sb_manager_t sbm = (sb_manager_t)calloc(1, sizeof(sb_manager));
	if (!sbm) {
		TRACE_ERROR("SBManagerCreate() failed. %s\n", strerror(errno));
		return NULL;
	}

	sbm->chunk_size = chunk_size;
	sbm->cnum = cnum;
	sbm->mp = (mem_pool_t)MPCreate(chunk_size, (uint64_t)chunk_size * cnum, 0);
	if (!sbm->mp) {
		TRACE_ERROR("Failed to create mem pool for sb.\n");
		free(sbm);
		return NULL;
	}

	sbm->freeq = CreateSBQueue(cnum);
	if (!sbm->freeq) {
		TRACE_ERROR("Failed to create free buffer queue.\n");
		return NULL;
	}

	return sbm;
}
/*----------------------------------------------------------------------------*/
struct tcp_send_buffer *
SBInit(sb_manager_t sbm, uint32_t init_seq)
{
	struct tcp_send_buffer *buf;

	/* first try dequeue from free buffer queue */
	buf = SBDequeue(sbm->freeq);
	if (!buf) {
		buf = (struct tcp_send_buffer *)malloc(sizeof(struct tcp_send_buffer));
		if (!buf) {
			perror("calloc() for buf");
			return NULL;
		}
		buf->data = MPAllocateChunk(sbm->mp);
		if (!buf->data) {
			TRACE_ERROR("Failed to fetch memory chunk for data.\n");
			return NULL;
		}
		sbm->cur_num++;
	}

	buf->head = buf->data;

	buf->head_off = buf->tail_off = 0;
	buf->len = buf->cum_len = 0;
	buf->size = sbm->chunk_size;

	buf->init_seq = buf->head_seq = init_seq;
	
	return buf;
}
/*----------------------------------------------------------------------------*/
#if 0
static void 
SBFreeInternal(sb_manager_t sbm, struct tcp_send_buffer *buf)
{
	if (!buf)
		return;

	if (buf->data) {
		MPFreeChunk(sbm->mp, buf->data);
		buf->data = NULL;
	}

	sbm->cur_num--;
	free(buf);
}
#endif
/*----------------------------------------------------------------------------*/
void 
SBFree(sb_manager_t sbm, struct tcp_send_buffer *buf)
{
	if (!buf)
		return;

	SBEnqueue(sbm->freeq, buf);
}
/*----------------------------------------------------------------------------*/
size_t 
SBPut(sb_manager_t sbm, struct tcp_send_buffer *buf, void *data, size_t len)
{
	size_t to_put=0;
	//ckf mod
	size_t n =0;
	uint32_t num_in_4 = 0;
	uint32_t npkt = 0;
	uint32_t i,j,data_index=0;
	uint8_t* _data = (uint8_t*)data;
	uint8_t redundancy[PKT_SIZE];
	uint32_t fraction_part;
	uint8_t zeros[PKT_SIZE];

	memset(zeros,0,PKT_SIZE);
	//compute to_put
	n = len;
	to_put = ceil(ceil(n/PKT_SIZE)/3) * 4 * PKT_SIZE;
	
	//if the remaining space is not enough
	if(to_put > buf->size - buf->len)
		return -2;

	/*	if (len <= 0)
		return 0;*/

	/* if no space, return -2 */
	

	if (buf->tail_off + to_put > buf->size) {
		
		/* if buffer overflows, move the existing payload and merge */
		memmove(buf->data, buf->head, buf->len);
		buf->head = buf->data;
		buf->head_off = 0;
		buf->tail_off = buf->len;
	}
	
	
	
	//copy the data, and redundancy if necessary
	n = len;
	npkt = floor(n/PKT_SIZE);
	num_in_4 = 0;
	data_index = 0;
	memset(redundancy,0,PKT_SIZE);
	
	for( i = 0; i<npkt;i++)
	{
		memcpy(buf->data + buf->tail_off,data+data_index,PKT_SIZE);
		buf->tail_off += PKT_SIZE;
		
		//compute redundancy
		for(j = 0;j<PKT_SIZE;j++,data_index++)
			redundancy[i] = redundancy[i] ^ _data[data_index];

		num_in_4 ++;

		if(num_in_4 == 3)//time to insert the redundancy
		{
			num_in_4 = 0;
			memcpy(buf->data + buf->tail_off,redundancy,PKT_SIZE);
			buf->tail_off += PKT_SIZE;
			memset(redundancy,0,PKT_SIZE);
		}
		
	}
	
	if(n - npkt * PKT_SIZE != 0)//need to compelete an fraction part
	{
		fraction_part = n - npkt * PKT_SIZE;

		memcpy(buf->data + buf->tail_off,data+data_index,fraction_part);
		buf->tail_off += fraction_part;
		
		for(j = 0; j< PKT_SIZE; j++,data_index++)
			redundancy[i] = redundancy[i] ^ _data[data_index];
		
		//compelete an pkt
		memcpy(buf->data + buf->tail_off,zeros,PKT_SIZE-fraction_part);
		buf->tail_off += PKT_SIZE - fraction_part;
		num_in_4 ++;
		if(num_in_4 == 3)//time to insert the redundancy
		{
			num_in_4 = 0;
			memcpy(buf->data + buf->tail_off,redundancy,PKT_SIZE);
			buf->tail_off += PKT_SIZE;
			memset(redundancy,0,PKT_SIZE);
		}

	}

	//fill int the last 4 pkt
	if(num_in_4 != 0)
	{	
		while(num_in_4 < 3)
		{
			memcpy(buf->data + buf->tail_off,zeros,PKT_SIZE);
			buf->tail_off += PKT_SIZE;
			num_in_4 ++;
		}
		memcpy(buf->data + buf->tail_off,redundancy,PKT_SIZE);
		buf->tail_off += PKT_SIZE;
	}
	
	buf->len += to_put;
	buf->cum_len += to_put;

	return len;
}
/*----------------------------------------------------------------------------*/
size_t 
SBRemove(sb_manager_t sbm, struct tcp_send_buffer *buf, size_t len)
{
	size_t to_remove;

	if (len <= 0)
		return 0;

	to_remove = MIN(len, buf->len);
	if (to_remove <= 0) {
		return -2;
	}

	buf->head_off += to_remove;
	buf->head = buf->data + buf->head_off;
	buf->head_seq += to_remove;
	buf->len -= to_remove;

	/* if buffer is empty, move the head to 0 */
	if (buf->len == 0 && buf->head_off > 0) {
		buf->head = buf->data;
		buf->head_off = buf->tail_off = 0;
	}

	return to_remove;
}
/*---------------------------------------------------------------------------*/
