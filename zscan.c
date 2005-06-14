/* zscan.c
 * Scott Bronson
 * 13 June 2005
 *
 * Uses a pseudo coroutine to scan a buffer for the zmodem start sequence.
 */


#include "fifo.h"
#include "log.h"
#include "zscan.h"

#include <stdio.h>


void zscanstate_init(zscanstate *conn)
{
	conn->parse_state = 0;
	conn->gotrz = RZNONE;
	conn->starcnt = 0;
}


static void fifo_append_rz(fifo *f, int gotrz)
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


static void fifo_append_stars(fifo *f, int starcnt)
{
	int i;

	for(i=0; i<starcnt; i++) {
		fifo_unsafe_addchar(f, '*');
	}
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


void zscan(zscanstate *conn, const char *cp, const char *ce, fifo *f)
{
	const char *cb;

	crBegin();

	for(;;) {
		if(cp >= ce) {
			return;
		}

		// skip as much garbage as we can
		cb = cp;
		while(*cp != 'r' && *cp != '*' && cp < ce) {
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
					fifo_unsafe_append(f, "rz", 2);
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
								(*conn->start_proc)(cp, ce-cp, conn->start_refcon);
								return;
							}
						}
					}
				}
			}

			fifo_append_rz(f, conn->gotrz);
			fifo_append_stars(f, conn->starcnt);
			if(conn->parse_state >= 1) fifo_unsafe_addchar(f, '\030');
			if(conn->parse_state >= 2) fifo_unsafe_addchar(f, 'B');
			if(conn->parse_state >= 3) fifo_unsafe_addchar(f, '0');
		}

		zscanstate_init(conn);
	}

	crEnd();
}

