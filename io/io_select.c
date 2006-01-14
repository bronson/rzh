// select.c
// Scott Bronson
// 4 October 2003
//
// Uses select to satisfy gatekeeper's network I/O
// Because of select's internal limitations, MAXFDS is 1024.
//
// This code is licensed under the same terms as Parrot itself.


// TODO: should probably make thread-safe by passing a global
// context everywhere?  This is pretty far in the future...


#include <stdio.h>
#include <errno.h>
#include <values.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "io.h"


static io_atom* connections[FD_SETSIZE];
static fd_set fd_read, fd_write, fd_except;
static fd_set gfd_read, gfd_write, gfd_except;
static int max_fd;	// the highest-numbered filedescriptor in connections.


// Pass the file descriptor that you'll be listening and accepting on.

void io_init()
{
	FD_ZERO(&fd_read);
	FD_ZERO(&fd_write);
	FD_ZERO(&fd_except);
}


void io_exit()
{
	// nothing to do
}


int io_exit_check()
{
	int cnt = 0;
	int i;

	// Check that we haven't leaked any atoms.
	for(i=0; i<FD_SETSIZE; i++) {
		if(connections[i]) {
			fprintf(stderr, "Leaked atom fd=%d proc=%08lX!\n", i, (long)connections[i]);
			cnt += 1;
		}
	}

	return cnt;
}


static void install(int fd, int flags)
{
	if(flags & IO_READ) {
		FD_SET(fd, &fd_read);
	} else {
		FD_CLR(fd, &fd_read);
	}

	if(flags & IO_WRITE) {
		FD_SET(fd, &fd_write);
	} else {
		FD_CLR(fd, &fd_write);
	}

	if(flags & IO_EXCEPT) {
		FD_SET(fd, &fd_except);
	} else {
		FD_CLR(fd, &fd_except);
	}
}


int io_add(io_atom *atom, int flags)
{
	int fd = atom->fd;

	if(fd < 0 || fd > FD_SETSIZE) {
		return -ERANGE;
	}
	if(connections[fd]) {
		return -EALREADY;
	}

	connections[fd] = atom;
	install(fd, flags);
	if(fd > max_fd) max_fd = fd;
	
	return 0;
}


int io_set(io_atom *atom, int flags)
{
	int fd = atom->fd;

	if(fd < 0 || fd > FD_SETSIZE) {
		return -ERANGE;
	}
	if(!connections[fd]) {
		return -EALREADY;
	}

	install(fd, flags);

	return 0;
}


int io_enable(io_atom *atom, int flags)
{
	if(atom->fd < 0 || atom->fd > FD_SETSIZE) {
		return -ERANGE;
	}
	if(!connections[atom->fd]) {
		return -EALREADY;
	}

	if(flags & IO_READ) {
		FD_SET(atom->fd, &fd_read);
	}

	if(flags & IO_WRITE) {
		FD_SET(atom->fd, &fd_write);
	}

	if(flags & IO_EXCEPT) {
		FD_SET(atom->fd, &fd_except);
	}
	
	return 0;
}


int io_disable(io_atom *atom, int flags)
{
	if(atom->fd < 0 || atom->fd > FD_SETSIZE) {
		return -ERANGE;
	}
	if(!connections[atom->fd]) {
		return -EALREADY;
	}

	if(flags & IO_READ) {
		FD_CLR(atom->fd, &fd_read);
	}

	if(flags & IO_WRITE) {
		FD_CLR(atom->fd, &fd_write);
	}

	if(flags & IO_EXCEPT) {
		FD_CLR(atom->fd, &fd_except);
	}

	return 0;
}


int io_del(io_atom *atom)
{
	int fd = atom->fd;

	if(fd < 0 || fd > FD_SETSIZE) {
		return -ERANGE;
	}
	if(!connections[fd]) {
		return -EALREADY;
	}

	install(fd, 0);
	connections[fd] = NULL;

	while((max_fd >= 0) && (connections[max_fd] == NULL))  {
		max_fd -= 1;
	}

	return 0;
}


/** Waits for events.  See io_dispatch to dispatch the events.
 * 
 * @param timeout The maximum amount of time we should wait in
 * milliseconds.  MAXINT is special-cased to mean forever.
 *
 * @returns the number of events to be dispatched or a negative
 * number if there was an error.
 */

int io_wait(unsigned int timeout)
{
	struct timeval tv;
	struct timeval *tvp = &tv;
	int ret;

	if(timeout == MAXINT) {
		tvp = NULL;
	} else {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
	}

	gfd_read = fd_read;
	gfd_write = fd_write;
	gfd_except = fd_except;

	do {
		ret = select(1+max_fd, &gfd_read, &gfd_write, &gfd_except, tvp);
	} while(ret < 0 && errno == EINTR);

	return ret;
}


void io_dispatch()
{
	int i, max, flags;

    // Note that max_fd might change in the middle of this loop.
    // For instance, if an acceptor proc opens a new connection
    // and calls io_add, max_fd will take on the new value.  Therefore,
    // we need to loop on the value set at the start of the loop.

	max = max_fd;
	for(i=0; i <= max; i++) {
		flags = 0;
		if(FD_ISSET(i, &gfd_read)) flags |= IO_READ;
		if(FD_ISSET(i, &gfd_write)) flags |= IO_WRITE;
		if(FD_ISSET(i, &gfd_except)) flags |= IO_EXCEPT;
		if(flags) {
			if(connections[i]) {
				(*connections[i]->proc)(connections[i], flags);
			} else {
				// what do we do -- event on an unknown connection?
				fprintf(stderr, "io_dispatch: got an event on an uknown connection %d!?\n", i);
			}
		}
	}
}

