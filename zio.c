/* zio.c
 * Scott Bronson
 * 14 Jan 2004
 *
 * This file is way too complex because I originally wrote it for
 * lrzsz.  Problem is, lrzsz is hopelessly complex so I salvaged
 * this for pdpzm.  Now it is in dire need of a rewrite...
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <setjmp.h>
#include <time.h>

#include "zio.h"

fifo inma;
fifo insi;
fifo outso;		
fifo outma;


/* private globals */
static jmp_buf *exit_jmp;
static struct timeval current_timeout;
static bcfn backchannel_fn;


void zio_bail(fifo *f, char *msg)
{
	if(f) {
		errno = f->error; 
		fprintf(stderr, "%s ", f->name);
	}

	perror(msg);

	if(exit_jmp) {
		longjmp(*exit_jmp, JMP_ERR);
	}

	/* only safe thing to do here is quit unexpectedly */
	for(;;) {}
	exit(1);
}


void default_backchannel(void)
{
	/* no action by default */
}


bcfn set_backchannel_fn(bcfn new)
{
	bcfn old = backchannel_fn;
	fifo_clear(&insi);
	backchannel_fn = new;
	return old;
}


/* Both output streams (outso and outma) are marked nonblocking.
 * There's no need to do this for input because select will
 * prevent us from ever calling a read that would block.
 */

void zio_setup(int ima, int oso, int ini, int oma, jmp_buf *bail)
{
	int stdsize = BUFSIZ;

	exit_jmp = bail;
	backchannel_fn = &default_backchannel;

	if(!fifo_init("inma", &inma, stdsize)) {
		zio_bail(&inma, "fifo init");
	}
	inma.fd = ima;

	if(oso >= 0) {
		if(!fifo_init("outso", &outso, stdsize)) {
			zio_bail(&outso, "fifo init");
		}
		outso.fd = oso;
		fcntl(outso.fd, F_SETFL, fcntl(outso.fd, F_GETFL) | O_NONBLOCK);
	} else {
		outso.buf = NULL;
	}

	if(ini >= 0) {
		if(!fifo_init("insi", &insi, stdsize)) {
			zio_bail(&insi, "fifo init");
		}
		insi.fd = ini;
	} else {
		insi.buf = NULL;
	}

	if(!fifo_init("outma", &outma, stdsize)) {
		zio_bail(&outma, "fifo init");
	}
	outma.fd = oma;
	fcntl(outma.fd, F_SETFL, fcntl(outma.fd, F_GETFL) | O_NONBLOCK);
}


/* pass 0 for both to disable the timeout */
void set_timeout(int sec, int usec)
{
	current_timeout.tv_sec = sec;
	current_timeout.tv_usec = usec;
}


/* returns true if a timeout happened */

static int doio(int shouldblock, struct timeval *timeout)
{
	fd_set rfds, wfds;
	struct timeval tvs, *tvp;
	int cnt;
	int nfds;
	int maxfd = 0;

#define FD_ADD(fd,set) do { \
	FD_SET((fd), (set)); \
	if((fd) > maxfd) \
	maxfd = (fd); \
} while(0)

	FD_ZERO(&rfds);
	FD_ADD(inma.fd, &rfds);
	if(insi.buf) FD_ADD(insi.fd, &rfds);

	FD_ZERO(&wfds);
	if(outso.buf && fifo_count(&outso)) FD_ADD(outso.fd, &wfds);
	if(fifo_count(&outma)) FD_ADD(outma.fd, &wfds);

	if(shouldblock == 0) {
		/* don't block, so set timeout to 0 */
		tvs.tv_sec = 0;
		tvs.tv_usec = 0;
		tvp = &tvs;
	} else if(timeout->tv_sec || timeout->tv_usec) {
		/* going to block so use the timeout */
		tvs = *timeout;
		tvp = &tvs;
	} else {
		/* no timeout */
		tvp = NULL;
	}

	nfds = select(maxfd+1, &rfds, &wfds, NULL, tvp);
	if(nfds < 0) {
		zio_bail(NULL, "select error");
	}

	if(insi.buf && FD_ISSET(insi.fd, &rfds)) {
		cnt = fifo_read(&insi, insi.fd);
		if(cnt == 0) {
			insi.state = FIFO_CLOSED;
		}
	}
	if(FD_ISSET(inma.fd, &rfds)) {
		cnt = fifo_read(&inma, inma.fd);
		if(cnt == 0) {
			inma.state = FIFO_CLOSED;
		}
	}

	(*backchannel_fn)();

	/* because we set the output to nonblocking, we just unconditionally
	 * write without checking the select results first. we still need to
	 * check the write fds in select, though, so that we'll wake up if
	 * one of them becomes available */
	if(outso.buf && (cnt=fifo_count(&outso))) {
		fifo_write(&outso, outso.fd);
	}
	if((cnt=fifo_count(&outma))) {
		fifo_write(&outma, outma.fd);
	}

	if(shouldblock) {
		if(nfds == 0) {
			return 1;
		}
	}

	return 0;	/* no timeout if we didn't block */
}


/* reads/writes all buffers, but won't return until it has some data
 * for the specified buffer or it hits eof on it */
/* bufstats is one of DO_ONCE HAS_ROOM HAS_DATA IS_EMPTY
 * if DO_ONCE is passed then buf can be NULL & we'll never block. */
/* if error reading the buffer, returns -1, else returns the
 * number of bytes available in the buffer */
/* note that if there's an error it only returns if it's THIS stream
 * that caused the error. otherwise it keeps chugging. eventually
 * someone will work with the errored stream and THEN it'll bail. */
/* This call is guaranteed not to block if you pass DO_ONCE.  Otherwise
 * it will block until the condition is fulfilled.  If the condition is
 * fulfilled upon entry, it will read/write as much as it can, but it
 * will not block. */

int tick(int bufstatus, fifo *f, int *toflagp)
{
	int ret = 0;
	time_t starttime, nowtime;
	struct timeval timeout;
	int toflag;

	static int seq=0;
	seq ++;

	starttime = time(NULL);

	if(toflagp) *toflagp = 0;

	if(f) {
		if(f->error) {
			ret = -1;
			goto done;
		}
		if(f->state == FIFO_CLOSED) {
			ret = 0;
			goto done;
		}
	}

	for(;;) {
		int should_block = 0;

		if(f) {
			switch(bufstatus) {
				case DO_ONCE:
					/* never block */
					break;
				case HAS_ROOM:
					/* block if fifo has no room */
					if(fifo_avail(f) == 0) should_block = 1;
					break;
				case HAS_DATA:
					/* block if fifo has no data */
					if(fifo_empty(f)) should_block = 1;
					break;
				case IS_EMPTY:
					if(!fifo_empty(f)) should_block = 1;
					break;
				default:
					ret = -2;
					goto done;
			}

			/* if we're going to block, we need to ensure that all
			 * write buffers are as flushed as possible so data doesn't
			 * just sit there while we sleep in select. */

			if(should_block) {
				if(outso.buf && fifo_count(&outso)) {
					fifo_write(&outso, outso.fd);
				}
				if(fifo_count(&outma)) {
					fifo_write(&outma, outma.fd);
				}

				/* now there's a chance that the previous writes
				 * fulfilled the exit condition.  If so, then exit */

				switch(bufstatus) {
					case HAS_ROOM:
						if(fifo_avail(f)) goto done;
						break;
					case IS_EMPTY:
						if(fifo_empty(f)) goto done;
						break;
				}
			}
		}

		/* need to watch timeout ourselves so that the user banging
		 * away on the keyboard won't prevent a read timeout from
		 * triggering */
		timeout = current_timeout;
		nowtime = time(NULL);
		timeout.tv_sec -= difftime(starttime, time(NULL));
		if(timeout.tv_sec < 0) {
			toflag = 1;
			goto done;
		}

		toflag = doio(should_block, &timeout);
		if(toflag) {
			goto done;
		}

		if(!f) {
			goto done;
		}

		if(f->error) {
			ret = -1;
			goto done;
		}
		if(f->state == FIFO_CLOSED) {
			ret = 0;
			goto done;
		}

		switch(bufstatus) {
			case DO_ONCE:
				goto done;
			case HAS_ROOM:
				if(fifo_avail(f) > 0) goto done;
				break;
			case HAS_DATA:
				if(!fifo_empty(f)) goto done;
				break;
			case IS_EMPTY:
				if(fifo_empty(f)) goto done;
				break;
		}
	}

done:
	if(toflagp) *toflagp = toflag;
	if(!toflag && f && (f->state == FIFO_IN_USE) && ret == 0) {
		/* fill in count if we performed an error-free transaction */
		ret = fifo_count(f);
	}

	return ret;
}


int get_master()
{
	int cnt;

	if(fifo_empty(&inma)) {
		cnt = tick(HAS_DATA, &inma, NULL);
		if(cnt < 0) zio_bail(&inma, "could not read from remote");
		if(cnt == 0) longjmp(*exit_jmp, JMP_EOF);
	}

	return fifo_unsafe_getchar(&inma);
}


// Blocks until there's data to return.
// This is OK because the zmodem lib only reads when it is expecting data.
// If it's deadlocking then try changing HAS_DATA to DO_ONCE.

size_t read_master(void *buf, size_t num)
{
	int cnt = fifo_count(&inma);
	if(!cnt) {
		cnt = tick(HAS_DATA, &inma, NULL);
	}
	if(cnt) {
		if(cnt > num) cnt = num;
		fifo_unsafe_unpend(&inma, buf, cnt);
	}
	return cnt;
}

void put_stdout(int c)
{
	int cnt;

	/* ensure there's room for this character */
	cnt = fifo_avail(&outso);
	if(cnt == 0) {
		int timeout;
		cnt = tick(HAS_ROOM, &outso, &timeout);
		if(cnt < 0) zio_bail(&outso, "could not write to stdout");
		if(timeout) zio_bail(&outso, "write_stdout timed out");
		if(cnt == 0) zio_bail(&outso, "unknown write_stdout error");
	}

	fifo_unsafe_addchar(&outso, c);
}

#if 0
   unused?
void put_outma(int c)
{
	int cnt;

	/* ensure there's room for this character */
	cnt = fifo_avail(&outma);
	if(cnt == 0) {
		int timeout;
		cnt = tick(HAS_ROOM, &outma, &timeout);
		if(cnt < 0) zio_bail(&outma, "could not write to master");
		if(timeout) zio_bail(&outso, "put_outma timed out");
		if(cnt == 0) zio_bail(&outso, "unknown put_outma error");
	}

	fifo_unsafe_addchar(&outma, c);
}
#endif


size_t write_master(void *buf, size_t size)
{
	int cnt;

	// write_outma will either block until it writes all pending
	// characters or longjmp out.  There's no way for it to fail.

	/* loop until we've transferred the entire buffer */
	while(size) {
		cnt = fifo_avail(&outma);
		if(cnt > size) cnt = size;
		fifo_unsafe_append(&outma, buf, cnt);

		buf += cnt;
		size -= cnt;

		if(size) {
			int timeout;
			cnt = tick(HAS_ROOM, &outma, &timeout);
			if(cnt < 0) zio_bail(&outma, "could not write to master");
			/* the original code didn't support timeouts on the write
			 * so I don't know what to do in this situation. */
			if(timeout) zio_bail(&outso, "write_outma timed out");
		} else {
			/* for good measure */
			cnt = tick(DO_ONCE, NULL, NULL);
			if(cnt < 0) zio_bail(&outma, "could not write to master");
		}
	}

	return size;
}


void flush_outma()
{
	if(!fifo_empty(&outma)) {
		tick(IS_EMPTY, &outma, NULL);
	}
	fifo_flush(&outma);
}

