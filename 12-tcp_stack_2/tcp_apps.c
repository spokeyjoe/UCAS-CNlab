#include "tcp_sock.h"

#include "log.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
// tcp server application, listens to port (specified by arg) and serves only one
// connection request
void *tcp_server(void *arg)
{
	u16 port = *(u16 *)arg;
	struct tcp_sock *tsk = alloc_tcp_sock();

	struct sock_addr addr;
	addr.ip = htonl(0);
	addr.port = port;
	if (tcp_sock_bind(tsk, &addr) < 0) {
		log(ERROR, "tcp_sock bind to port %hu failed", ntohs(port));
		exit(1);
	}

	if (tcp_sock_listen(tsk, 3) < 0) {
		log(ERROR, "tcp_sock listen failed");
		exit(1);
	}

	log(DEBUG, "listen to port %hu.", ntohs(port));
	log(DEBUG, "going to accept a connection.");
	struct tcp_sock *csk = tcp_sock_accept(tsk);

	

	char rbuf[1001];
	char wbuf[1024];
	int rlen = 0;
	FILE *fptr;
	fptr = fopen("server-output.dat","wb");
	if(fptr == NULL)
	{
		printf("Error!");   
		exit(1);             
	}
	while (1) {
		rlen = tcp_sock_read(csk, rbuf, 1000);
		// log(DEBUG,"read data from socket, length is %d",rlen);
		if (rlen == 0) {
			log(DEBUG, "tcp_sock_read return 0, finish transmission.");
			break;
		} 
		else if (rlen > 0) {
			// data from receive buffer, data size is rlen, write 1 instance, to fptr.
			fwrite(rbuf,rlen,1,fptr); 
		}
		else {
			log(DEBUG, "tcp_sock_read return negative value, something goes wrong.");
			exit(1);
		}
	}
	fclose(fptr);
	log(DEBUG, "close this connection.");

	tcp_sock_close(csk);
	
	return NULL;
}

// tcp client application, connects to server (ip:port specified by arg), each
// time sends one bulk of data and receives one bulk of data 
void *tcp_client(void *arg)
{
	struct sock_addr *skaddr = arg;

	struct tcp_sock *tsk = alloc_tcp_sock();

	if (tcp_sock_connect(tsk, skaddr) < 0) {
		log(ERROR, "tcp_sock connect to server ("IP_FMT":%hu)failed.", \
				NET_IP_FMT_STR(skaddr->ip), ntohs(skaddr->port));
		exit(1);
	}
	
	FILE *fptr;
	fptr = fopen("client-input.dat","rb");
	if(fptr==NULL){
		log(ERROR,"open file error");
		return;
	}
	
	int buffer_size = 5000;
	char *wbuf = malloc(buffer_size); 
	int read_byte;
	// 1 是单位大小， buffer_size 是单位数量。以字节为单位，读取 buffer_size 个字节。
	while((read_byte=fread(wbuf, 1, buffer_size, fptr))>0){
		if (tcp_sock_write(tsk, wbuf, read_byte) < 0){
			log(ERROR,"read error");
		}
		usleep(1000);
	}


	fclose(fptr);
	free(wbuf);
	tcp_sock_close(tsk);

	return NULL;
}
