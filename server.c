#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>
#include "protocol.h"
#define ERR(source) (perror(source),\
		     fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
		     exit(EXIT_FAILURE))

typedef struct
{
	struct sockaddr_in *addr;
	pthread_mutex_t *mutex;
	char file[NMMAX + 1];
} thread_arg;


struct connections{
	int free;
	int32_t chunkNo;
	struct sockaddr_in addr;
};

int sethandler( void (*f)(int), int sigNo) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = f;
	if (-1==sigaction(sigNo, &act, NULL))
		return -1;
	return 0;
}

int make_socket(int domain, int type){
	int sock;
	sock = socket(domain,type,0);
	if(sock < 0) ERR("socket");
	return sock;
}

int bind_inet_socket(uint16_t port,int type){
	struct sockaddr_in addr;
	int socketfd,t=1;
	socketfd = make_socket(PF_INET,type);
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR,&t, sizeof(t))) ERR("setsockopt");
	if(bind(socketfd,(struct sockaddr*) &addr,sizeof(addr)) < 0)  ERR("bind");
	if(SOCK_STREAM==type)
		if(listen(socketfd, BACKLOG) < 0) ERR("listen");
	return socketfd;
}

ssize_t bulk_write(int fd, char *buf, size_t count){
	int c;
	size_t len=0;
	do{
		c=TEMP_FAILURE_RETRY(write(fd,buf,count));
		if(c<0) return c;
		buf+=c;
		len+=c;
		count-=c;
	}while(count>0);
	return len ;
}


void usage(char * name){
	fprintf(stderr,"USAGE: %s port\n",name);
}

void init(pthread_t *thread, thread_arg *targ, sem_t *semaphore, pthread_cond_t *cond, pthread_mutex_t *mutex, int *idlethreads, int *socket, int *condition)
{
	int i;

	if (sem_init(semaphore, 0, FS_NUM) != 0)
		ERR("sem_init");

	for (i = 0; i < THREAD_NUM; i++)
	{
		targ[i].id = i + 1;
		targ[i].cond = cond;
		//targ[i].mutex = mutex;
		targ[i].semaphore = semaphore;
		targ[i].idlethreads = idlethreads;
		targ[i].socket = socket;
		targ[i].condition = condition;
		if (pthread_create(&thread[i], NULL, threadfunc, (void *) &targ[i]) != 0)
			ERR("pthread_create");

	}
}


int main(int argc, char** argv) {
	int fd;
	if(argc!=2) {
		usage(argv[0]);
		return EXIT_FAILURE;
	}
	if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
	fd=bind_inet_socket(atoi(argv[1]),SOCK_DGRAM);
	doServer(fd);
	if(TEMP_FAILURE_RETRY(close(fd))<0)ERR("close");
	fprintf(stderr,"Server has terminated.\n");
	return EXIT_SUCCESS;
}

