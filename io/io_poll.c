// poll.c
// Scott Bronson

#include <sys/poll.h>


#define MAXFDS 

struct pollfd ufds[MAXFDS];


void io_init()
{
}


void io_exit()
{
	close(epfd);
}


void io_wait()
{
	int nfds;

	nfds = epoll_wait(epfd, events, maxevent
}

