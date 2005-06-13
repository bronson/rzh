// epoll.c
// Scott Bronson
//
// Uses epoll to satisfy gatekeeper's network I/O

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

