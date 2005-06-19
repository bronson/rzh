/* zscan.c
 * Scott Bronson
 * 13 June 2005
 *
 * Uses a pseudo coroutine to scan a buffer for the zmodem start sequence.
 *
 * TODO: Make the zscanner not know about fifos.  It should just
 * scan and start.  The echo scanner should worry about fifos.
 */


#include "fifo.h"
#include "log.h"
#include "zscan.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static void zscanstate_init(zscanstate *conn)
{
	conn->parse_state = 0;
	conn->gotrz = RZNONE;
	conn->starcnt = 0;
}


zscanstate* zscan_create(zstart_proc proc, void *refcon)
{
    zscanstate *zscan;

    zscan = malloc(sizeof(zscanstate));
    if(zscan == NULL) {
        perror("allocating zscanstate");
        bail(55);
    }

    zscanstate_init(zscan);
    zscan->start_proc = proc;
    zscan->start_refcon = refcon;

    return zscan;
}


void zscan_destroy(zscanstate *state)
{
	free(state);
}


static void fifo_append_rz(struct fifo *f, int gotrz)
{
	switch(gotrz) {
		case RZNL:
			fifo_unsafe_append(f, "rz\n", 3);
			break;
		case RZCR:
			fifo_unsafe_append(f, "rz\r", 3);
			break;
		case RZCRNL:
			fifo_unsafe_append(f, "rz\r\n", 4);
			break;
		default:
			; // do nothing
	}
}


static void fifo_append_stars(struct fifo *f, int starcnt)
{
	int i;

	for(i=0; i<starcnt; i++) {
		fifo_unsafe_addchar(f, '*');
	}
}


/** we can't switch over until the fifo is empty
 * (else the data would go to the wrong place)
 *
 * We'll try to write for 10 seconds, then assume that we've deadlocked
 * and terminate the program.  In practice, the fifo is almost always
 * empty and this loop is never entered anyway.
 * 
 * TODO
 *
 * Frankly, this routine sucks ass.  There's a vanishingly small but
 * nonzero chance of deadlock
 * since we're not handling any other I/O while this is happening.
 *
 * Instead of doing this, we should mark the fifo with the new task_spec at
 * the start of the rz data.  Then, when it has drained to that point normally,
 * the task_spec would be installed automatically.  Scanning would then be
 * just marking, not copying (except that it must be able to remove the
 * rz\r somehow).
 */

static void fifo_drain_completely(struct fifo *f, int fd)
{
	int i = 0;

	// Try to write.  We sleep to try to give the system enough
	// time to free up whatever it's doing in the foreground.
	while(fifo_count(f) > 0) {
		fifo_write(f, fd);
		sleep(1);
		i += 1;
		if(i >= 5) {
			fprintf(stderr, "Could not empty zscan fifo in 5 seconds (had %d bytes at end).  Bailing!\n", fifo_count(f));
			bail(33);
		}
	}
}

static void fifo_drain_avail(struct fifo *f, int fd, int min_needed)
{
	int i = 0;

	// Try to write.  We sleep to try to give the system enough
	// time to free up whatever it's doing in the foreground.
	while(fifo_avail(f) < min_needed) {
		fifo_write(f, fd);
		sleep(1);
		i += 1;
		if(i >= 5) {
			fprintf(stderr, "Could not free up %d bytes in zscan fifo in 5 seconds (had %d bytes at end).  Bailing!\n",
					min_needed, fifo_avail(f));
			bail(33);
		}
	}
}


/** This routine is SUCH AN AWFUL HACK!!!  It doesn't update
 *  pipe->bytes_written, it does i/o without ever crossing the main
 *  event loop, bleah!!  OTOH, it works now and it would take a lot
 *  of time to fix properly.  I can live with it for now.
 */

static void zscan_start(zscanstate *conn, struct fifo *f, const char *cp, const char *ce, int fd)
{
	int remaining = ce - cp;

	fifo_drain_completely(f, fd);
	fifo_unsafe_append_str(f, "**\030B00");

	// start the subtask
	(*conn->start_proc)(conn->start_refcon);

	// There is probably a different filter proc on the fifo now.
	// We'll call it with the remaining data.
	if(remaining > 0) {
		// Because we just appended the start string, there might
		// not be enough room to store the remaining fifo data.
		// (maybe pipe is drastically write-bound and the few extra
		// bytes of the start string overflowed it)

		if(remaining > fifo_avail(f)) {
			fifo_drain_avail(f, fd, remaining);
		}
		if(f->proc) {
			(*f->proc)(f, cp, remaining, fd);
		} else {
			// no proc, just append the rest
			fifo_unsafe_append(f, cp, remaining);
		}
	}

	// Now the next task can continue on its merry way.
}


#define crBegin()                           \
	switch(conn->parse_state) {             \
		case 0:

#define crEnd()                             \
	}

#define crNext(st)							\
	do {                    				\
		conn->parse_state=st;				\
		cp += 1;                            \
		if(cp >= ce) return;          		\
		case st:;							\
	} while(0)



/** Scan for the start of a ZRQINIT header.  zmodem transfers often
 * 	begin with a "rz\n" character sequence.  We try to suppress that.
 *
 * 	We need to be a bit tricky with two sequences.  "rz\n" must all
 * 	appear in a single packet.  Otherwise, the user may have typed it
 * 	and will be wondering why the "r" he just typed is being suppressed!
 * 	Same with the "**\030" sequence -- the user might be wondering where
 * 	the asterixes he typed went!  Other than those two sequences, the
 * 	packet boundaries may fall wherever.
 *
 * 	This behavior may cause us to miss some start packets, but I think
 * 	that's unlikely.  Far better that than try to explain to the user
 * 	why "r"s and "*"s don't appear until you type more characters...
 */

void zscan(zscanstate *conn, const char *cp, const char *ce, struct fifo *f, int fd)
{
	const char *cb;

	crBegin();

	for(;;) {
		if(cp >= ce) {
			return;
		}

		// skip as much garbage as we can
		cb = cp;
		while(cp < ce && *cp != 'r' && *cp != '*') {
			cp++;
		}

		if(cp > cb) {
			fifo_unsafe_append(f, cb, cp-cb);
			if(cp >= ce) {
				return;
			}
			cb = cp;
		}

		if(cp[0] == 'r') {
			if(ce - cp < 3) {
				// We require the "rz\n" to appear entirely within a single
				// packet.  This prevents eating r/z's that the user types.
				fifo_unsafe_append(f, cp, ce-cp);
				return;
			}

			if(cp[1] == 'z') {
				if(cp[2] == '\r') {
					if(ce - cp >= 4 && cp[3] == '\n') {
						conn->gotrz = RZCRNL;
						cp += 4;
					} else {
						conn->gotrz = RZCR;
						cp += 3;
					}
				} else if(cp[2] == '\n') {
					conn->gotrz = RZNL;
					cp += 3;
				} else {
					fifo_unsafe_append_str(f, "rz");
					cp += 2;
					continue;
				}
			} else {
				fifo_unsafe_addchar(f, 'r');
				cp += 1;
				continue;
			}
		}

		conn->parse_state = -1;
		if(cp >= ce) return;
		case -1:;

		if(cp[0] == '*') {

			// there may be any number of stars
			while(*cp == '*' && cp < ce) {
				conn->starcnt += 1;
				cp++;
			}

			if(cp < ce) {
				if(*cp == '\030') {
					crNext(1);
					if(*cp == 'B') {
						crNext(2);
						if(*cp == '0') {
							crNext(3);
							if(*cp == '0') {
								zscanstate_init(conn);
								zscan_start(conn, f, cp, ce, fd);
								return;
							}
						}
					}
				}
			}

			fifo_append_rz(f, conn->gotrz);
			conn->gotrz = 0;
			fifo_append_stars(f, conn->starcnt);
			if(conn->parse_state >= 1) fifo_unsafe_addchar(f, '\030');
			if(conn->parse_state >= 2) fifo_unsafe_addchar(f, 'B');
			if(conn->parse_state >= 3) fifo_unsafe_addchar(f, '0');
		}

		if(conn->gotrz) {
			fifo_append_rz(f, conn->gotrz);
		}
		zscanstate_init(conn);
	}

	crEnd();
}


zfinscanstate* zfinscan_create(void (*proc)(struct fifo *f, const char *buf, int size, int fd))
{
    zfinscanstate *state;

    state = malloc(sizeof(zfinscanstate));
    if(state == NULL) {
        perror("allocating zfinscanstate");
        bail(56);
    }
	memset(state, 0, sizeof(zfinscanstate));

	state->found = proc;

    return state;

}


void zfinscan_destroy(zfinscanstate *state)
{
	if(state->savebuf) {
		free(state->savebuf);
	}
	free(state);
}


/** Scans for the ZFIN packet.  This one doesn't need to be a coroutine.
 *
 *  Terminating the connection without sending garbage onto the user's
 *  terminal is made somewhat complex due to zmodem weirdness.
 *  Here's how it works:
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
 *      We kill the child.  We know the sender has already sent a ZFIN.
 *  
 *  When we recognize a ZFIN from the master:
 *      We supporess a further optional "OO"
 *      We ensure that all further data gets written to stdout.
 *          So, what we do, we store all data read from the master AFTER the ZFIN/OO
 *          Then, when the pipes are restored, the data is written back into the pipe
 */

void zfinscan(struct fifo *f, const char *buf, int size, int fd)
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
			if(size > 0) {
				(*f->proc)(f, buf, size, fd);
			}
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

void zfinnooo(struct fifo *f, const char *buf, int size, int fd)
{
	zfinscanstate *state = (zfinscanstate*)f->refcon;

	while(size > 0) {
		// move to the next routine if we've suppressed 2 Os
		// or there are no more Os to be found.
		if(*buf != 'O' || state->oocount >= 2) {
			f->proc = zfinsave;
			if(size > 0) {
				(*f->proc)(f, buf, size, fd);
			}
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

void zfinsave(struct fifo *f, const char *buf, int size, int fd)
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


void zfindrop(struct fifo *f, const char *buf, int size, int fd)
{
	log_info("IGNORE from %d: %s", fd, sanitize(buf, size));
}


/*
   // This wrapper verifies that zfinscan doesn't modify its data in any way.
void zfinscan(struct fifo *f, const char *buf, int size, int fd)
{
	log_warn("enter: size=%d refcon=%08lX", size, (long)f->refcon);
	int ofe = f->end;
	azfinscan(f, buf, size, fd);
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

