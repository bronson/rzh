/* scan.c
 * 15 Jan 2004
 * Scott Bronson
 *
 * Watches for a zmodem start request.
 *
 * This file is released under the MIT license.  This is basically the
 * same as public domain, but absolves the author of liability.
 */

#include <string.h>
#include <sys/types.h>
#include <setjmp.h>

#include "zmcore.h"
#include "scan.h"
#include "zio.h"


static const char *startstr = "**\030B00"; 


static void cancel_transfer()
{
	static char stopstr[] =
	{
		24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
		 8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
		 0
	};

	fifo_clear(&inma);
	fifo_clear(&outma);

	write_master(stopstr, strlen(stopstr));
}


static void do_transfer()
{
	if(fifo_avail(&inma) < strlen(startstr)) {
		/* this, i think, is effectively impossible */
		fprintf(stderr, "Not enough buffer space to start transfer -- please try again.\n");
		cancel_transfer();
		return;
	}

	// prepend the stock startstr so that the zmodem engine will fire up.
	fifo_unsafe_prepend(&inma, startstr, strlen(startstr));
	receive_file();
}


// This function is slower than it should be.
// It can be optimized since most of the characters it'll
// be looking at won't be 'r' or '*'.

int scan_for_zrqinit()
{
	int c;
	int gotrz;
	int gotfin;
	int starcnt;

	goto toploop;

	while(1) {
		put_stdout(c);

toploop:
		gotrz=0;
		gotfin=0;
		starcnt=0;

		/* Need to be careful or we won't print 'r's.
		 * presumably the remote machine will be automated and burst
		 * "rz\r" into one packet.  If not, no big deal, the user just
		 * sees the rz\r onscreen. */
		c = get_master();
		if(c == 'r' && !fifo_empty(&inma)) {
			c = get_master();
			if(c == 'z') {
				c = get_master();
				if(c == '\r') {
					gotrz = 1;
					c = get_master();
					if(c == '\n') {
						gotrz = 2;
						c = get_master();
					}
				} else {
					put_stdout('r');
					put_stdout('z');
					continue;
				}
			} else {
				put_stdout('r');
				continue;
			}
		}

		if(c == '*') {
			do {
				starcnt += 1;
				if(fifo_empty(&inma) && !gotrz) {
					/* if fifo empties, it's probably just the user typing stars.
					 * Worst case, he just gets some garbage onscreen.  */
					for(;starcnt;starcnt--) put_stdout('*');
				}
				c = get_master();
			} while(c == '*');

			if(c == '\030') {
				gotfin = 1;
				c = get_master();
				if(c == 'B') {
					gotfin = 2;
					c = get_master();
					if(c == '0') {
						gotfin = 3;
						c = get_master();
						if(c == '0') {
							do_transfer();
							goto toploop;
						}
					}
				}
			}
		}

		/* no session could be started so write data back out */
		if(gotrz) {
			put_stdout('r');
			put_stdout('z');
			put_stdout('\r');
			if(gotrz >= 2) {
				put_stdout('\n');
			}
		}

		for(;starcnt;starcnt--) put_stdout('*');

		if(gotfin >= 1) put_stdout('\030');
		if(gotfin >= 2) put_stdout('B');
		if(gotfin >= 3) put_stdout('0');
	}
}

