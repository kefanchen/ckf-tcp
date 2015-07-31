#include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <stdint.h>
 #include <time.h>
 #include <sys/time.h>
 #include <sys/types.h>
 #include <sys/stat.h>
 #include <fcntl.h>
 #include <pthread.h>
 #include <signal.h>
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <arpa/inet.h>
 #include <sys/queue.h>
 #include <assert.h>

 #include <mtcp_api.h>
 #include <mtcp_epoll.h>
 #include "cpu.h"
 #include "rss.h"
 #include "http_parsing.h"
 #include "debug.h"

#define PORT_NO 12345
#define SIZE 4096
int main (int argc,char*argv[])
{
	int sock,len;
	struct sockaddr_in name;
	unsigned char buf[SIZE];
	int rlen;
	int i;
	int core;
	
	
	if(mtcp_init("epwget.conf"))
	{	
		printf("failed to init .conf\n");
	}
	mtcp_core_affinitize(0);
	mctx_t mctx = mtcp_create_context(0);
	if(sock<0)
	{
		perror("sock<0\n");
		exit(1);
	}
/*	sock =mtcp_socket(mctx,AF_INET,SOCK_STREAM,0);
	
	if(mtcp_setsock_nonblock(mctx,sock) < 0)
	{
		printf("ckf failed to set nonblock sock\n");
	}
*/

	name.sin_family = AF_INET;
	name.sin_addr.s_addr = htonl(inet_network("127.0.0.1"));
	name.sin_port = htons(PORT_NO);

	if(mtcp_connect(mctx,sock,&name,sizeof(name))<0)
	{
		perror("failed to connect to server\n");
		exit(1);
	}

	for(;;)
	{
		rlen = mtcp_read (mctx,sock,buf,sizeof(buf));
		
		if(rlen < 0)
		{
			perror("read error\n");
			exit(1);
		}

		if(rlen ==0)
		{
			printf("******Disconeted\n");
			exit(0);
		}

		printf("read from server :");

		for(i=0;i<rlen;i++)printf("%c",buf[i]);
	}

	mtcp_destory_context(mctx);
}




