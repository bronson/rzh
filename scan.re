/* scan.c
 * 15 Jan 2004
 * Scott Bronson
 *
 * Watches for a zmodem start request.
 *
 * This file is released under the MIT license.  This is basically the
 * same as public domain, but absolves the author of liability.
 */

#include "re2c/scan.h"

#define CMDNO 0x10

enum {
	STARTIT = 1,
	GARBAGE = 2,
	CMDSTR1 = CMDNO|1,
	CMDSTR2 = CMDNO|2,
	CMDSTR3 = CMDNO|3,
};


int zrqscan(scanstate *ss)
{
	ss->token = ss->cursor;

/*!re2c

	"rz\n"			{ return CMDSTR1; }
	"rz\r"			{ return CMDSTR2; }
	"rz\r\n"		{ return CMDSTR3; }

	"*"+"\030B00"	{ return STARTIT; }
	[\000-\377]+	{ return GARBAGE; }

*/
}


void zrqparse(scanstate *ss)
{
	int tok;
	int cmdno = 0;

	do {
		tok = zrqscan(ss);

		// fire this puppy up
		if(tok == STARTIT) {
			break;
		}

		// try to suppress the start str
		if(tok & CMDNO) {
			if(cmdno) {
				putcmdno
			}
			cmdno = tok;
			continue;
		}

		// gotta be garbage
		putgarbage
	}
}

