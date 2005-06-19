/* zfin.c
 * Scott Bronson
 * 13 June 2005
 *
 * Implements a fifo proc that scans for the ZFIN end packet.
 */


/** @file zfin.c
 *
 *  Scans for the ZFIN packet and tries to assure an orderly shutdown.
 *
 *  Terminating the connection without sending garbage onto the user's
 *  terminal is made somewhat complex due to zmodem weirdness.
 *  Here's how we do it now:
 *  
 *  1) Sender sends ZFIN
 *      Child receives the ZFIN
 *  2) Child sends ZFIN in response
 *      Sender receives it
 *          Sender might send "OO" (over and out) or not.  Stupid protocol.
 *      We shut the child down immediately after it sends the zfin
 *          This prevents further packets from hitting the sender
 *          and being interpreted as shell commands.
 *  
 *  So, when we recognize a ZFIN from the child:
 *      We turn off that fifo.  No more data will be read from the child.
 *      	zfin_scan -> zfin_term -> zfin_drop
 *      We kill the child.  We know the sender has already sent a ZFIN.
 *  
 *  When we recognize a ZFIN from the master:
 *      We supporess a further optional "OO"
 *      We ensure that all further data gets written to stdout.
 *          So, what we do, we store all data read from the master AFTER the ZFIN/OO
 *          Then, when the pipes are restored, the data is written back into the pipe
 * 		So the master actually cycles through 3 procs while scanning:
 * 			zfin_scan -> zfin_nooo -> zfin_save
 * 		Then, in the destructor, we copy the saved data back into the output pipe.
 */


#include "log.h"
#include "fifo.h"
#include "io/io.h"
#include "pipe.h"
#include "task.h"
#include "zfin.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


zfinscanstate* zfin_create(master_pipe *mp,
		void (*proc)(struct fifo *f, const char *buf, int size, int fd))
{
    zfinscanstate *state;

    state = malloc(sizeof(zfinscanstate));
    if(state == NULL) {
        perror("allocating zfinscanstate");
        bail(56);
    }
	memset(state, 0, sizeof(zfinscanstate));

	state->master = mp;
	state->found = proc;

    return state;

}


void zfin_destroy(zfinscanstate *state)
{
	if(state->savebuf) {
		free(state->savebuf);
	}
	free(state);
}


void zfin_scan(struct fifo *f, const char *buf, int size, int fd)
{
	static const char zfin[] = "**\030B0800000000022d\015\212";

	zfinscanstate *state = (zfinscanstate*)f->refcon;
	const char *ref = state->ref;
	const char *cp, *ce;

	if(size <= 0) {
		return;
	}
	
	for(;;) {
		if(ref) {
			cp = buf;
		} else {
			cp = memchr(buf, '*', size);
			if(!cp) {
				// couldn't find the start char in the entire buffer.
				fifo_unsafe_append(f, buf, size);
				return;
			}

			// found the start char.  flush all the data up to the start char.
			fifo_unsafe_append(f, buf, cp-buf);
			size -= cp-buf;
			buf = cp;
			// and set up the state var
			ref = zfin;
		}
		ce = buf + size;

		while(cp < ce && *cp == *ref && ref < zfin+sizeof(zfin)-1) {
			cp += 1;
			ref += 1;
		}

		if(*ref == 0) {
			log_info("ZFIN on fd %d!", fd);
			fifo_unsafe_append(f, zfin, sizeof(zfin)-1);
			size -= cp-buf;
			buf = cp;
			f->proc = state->found;
			(*f->proc)(f, buf, size, fd);
			return;
		} else if(cp >= ce) {
			// out of data, so we'll return and get called with more
			break;
		} else if(*cp != *ref) {
			// couldn't match.  append what we have so far.
			fifo_unsafe_append(f, zfin, ref-zfin);
			size -= ref-zfin;
			buf = cp;
			//  unset the state var
			ref = NULL;
		} else {
			assert(!"This should be unpossible!");
		}
	}

	f->refcon = (void*)ref;
}


/** No OO: drops an optional OO, then passes the rest to zfinsave */

void zfin_nooo(struct fifo *f, const char *buf, int size, int fd)
{
	zfinscanstate *state = (zfinscanstate*)f->refcon;

	while(size > 0) {
		// move to the next routine if we've suppressed 2 Os
		// or there are no more Os to be found.
		if(*buf != 'O' || state->oocount >= 2) {
			f->proc = zfin_save;
			(*f->proc)(f, buf, size, fd);
			return;
		}

		log_info("Dropped an O after ZFIN on %d", fd);
		buf += 1;
		size -= 1;
		state->oocount += 1;
	}
}


/** Saves all text in a dynamic buffer.  When the destructor is
 *  called, the saved text will be inserted into the pipe (now
 *  reconnected to the terminal instead of to the receive process).
 */

void zfin_save(struct fifo *f, const char *buf, int size, int fd)
{
	zfinscanstate *state = (zfinscanstate*)f->refcon;

	if(size <= 0) {
		return;
	}

	if(size + state->savecnt > state->savemax) {
		state->savemax += size + 512;
		log_info("Reallocing save buffer to %d bytes.", (int)state->savemax);
		state->savebuf = realloc(state->savebuf, state->savemax);
		if(state->savebuf == NULL) {
			perror("reallocing savebuf");
			bail(57);
		}
	}

	log_info("SAVING %d bytes from %d: %s", size, fd, sanitize(buf, size));
	memcpy(state->savebuf + state->savecnt, buf, size);
	state->savecnt += size;
}


void zfin_term(struct fifo *f, const char *buf, int size, int fd)
{
	zfinscanstate *state = (zfinscanstate*)f->refcon;

	task_terminate(state->master);

	f->proc = zfin_drop;
	(*f->proc)(f, buf, size, fd);
}


void zfin_drop(struct fifo *f, const char *buf, int size, int fd)
{
	if(size <= 0) {
		return;
	}

	log_info("IGNORE from %d: %s", fd, sanitize(buf, size));
}


/*
   // This wrapper verifies that zfin_scan doesn't modify its data in any way.
void zfin_scan(struct fifo *f, const char *buf, int size, int fd)
{
	log_warn("enter: size=%d refcon=%08lX", size, (long)f->refcon);
	int ofe = f->end;
	azfin_scan(f, buf, size, fd);
	int nfe = f->end;

	if(nfe >= ofe) {
		if(nfe - ofe != size) {
			log_warn("sizes differ!");
		} else if(memcmp(f->buf+ofe, buf, size) != 0) {
			log_warn("contents differ!");
		}
	} else {
		// skip this for now
		log_warn("wrap!");
	}
}
*/

