// io_kqueue.c
// Scott Bronson

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/event.h>


// TODO I'm not sure how to do except notification.
// Even if simultaneous read and write events happen on an fd,
// this code handles them separately.  I think this is just a
// kqueue limitation.


int kqfd;

// initial size for the change list
#define CHANGE_INITIAL 128
// change list grows by this amount when it runs out of room
#define CHANGE_INCREMENT 128
// the maximum number of events that can be handled on each
// call to io_wait().
#define MAX_EVENTS_HANDLED 512

kevent *changes;
int num_changes = 0;
int max_changes = 128;



void io_init()
{
	kqfd = kqueue();
	if(kqfd < 0) {
		perror("kqueue");
		exit(1);
	}

	changes = malloc(sizeof(struct kevent) * max_changes);
}


void io_exit()
{
	free(changes);
	close(kqfd);
}


static void newchange(int filter, int flags, io_atom *atom)
{
	if(num_changes >= max_changes) {
		max_changes += CHANGE_INCREMENT;
		changes = (struct kevent*)realloc(changes,
				sizeof(struct kevent) * max_changes);
	}

	struct kevent *kp = &changes[num_changes];
	memset(kp, 0, sizeof(struct kevent));
	kp->ident = atom->fd;
	kp->filter = filter;
	kp->udata = atom;

	num_changes += 1;
}



int io_add(io_atom *atom, int flags)
{
	if(flags & IO_READ) {
		newchange(EVFILT_READ, EV_ADD | EV_ENABLE, atom);
	}
	if(flags & IO_WRITE) {
		newchange(EVFILT_WRITE, EV_ADD | EV_ENABLE, atom);
	}
}


void io_set(io_atom *atom, int flags)
{
	if(flags & IO_READ) {
		newchange(EVFILT_READ, EV_ADD | EV_ENABLE, atom);
	} else {
		newchange(EVFILT_READ, EV_DELETE, atom);
	}
	if(flags & IO_WRITE) {
		newchange(EVFILT_WRITE, EV_ADD | EV_ENABLE, atom);
	} else {
		newchange(EVFILT_WRITE, EV_DELETE, atom);
	}
}


int io_del(io_atom *atom)
{
	if(flags & IO_READ) {
		newchange(EVFILT_READ, EV_DELETE, atom);
	}
	if(flags & IO_WRITE) {
		newchange(EVFILT_WRITE, EV_DELETE, atom);
	}
}


int io_wait(int timeout)
{
	struct kevent ev[MAX_EVENTS_HANDLED];
	struct timeval tv;
	struct timeval *tvp = &tv;
	int num;

	if(timeout == MAXINT) {
		tvp = NULL;
	} else {
		tv.tv_sec = timeout / 1000;
		tv.tv_usec = (timeout % 1000) * 1000;
	}

	num = kevent(kqfd, changes, num_changes, events,
			sizeof(ev)/sizeof(ev[0]), tvp);
	if(num < 0) {
		perror("kevent");
		return num;
	}

	for(i=0; i<num; i++) {
		struct kevent *kp = &ev[i];
		io_atom *atom = (io_atom*)kp->udata;
		int flags = 0;

		if(kp->filter == EVFILT_READ) {
			flags = IO_READ;
		} else if(kp->filter == EVFILT_WRITE) {
			flags = IO_WRITE;
		} else {
			// wtf is this?
			continue;
		}

		(*atom->proc)(atom, flags);
	}

	return num;
}

