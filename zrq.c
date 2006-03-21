/* zrq.c
 * Scott Bronson
 * 13 June 2005
 *
 * Uses a pseudo coroutine to scan a buffer for the ZRQINIT
 * zmodem start sequence.
 */


#include "fifo.h"
#include "log.h"
#include "zrq.h"
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


zscanstate* zrq_create(zstart_proc proc, void *refcon)
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


void zrq_destroy(zscanstate *state)
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

	log_info("Draining %d bytes to %d before starting.", fifo_count(f), fd);
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

void zrq_scan(zscanstate *conn, const char *cp, const char *ce, struct fifo *f, int fd)
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
			while(cp < ce && *cp == '*') {
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
								cp += 1;	// don't use crNext because
									// we don't care if we're out of data --
									// the zmodem receive will start anyway.
								log_info("zrq on %d found!", fd);
								/*
								extern void logio(char *gr1, char* gr2, int fd, const char *buf, int cnt, int act);
								logio("There are", "remaining on", fd, cp, ce-cp, ce-cp);
								*/
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

