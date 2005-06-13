/* re2c.c
 * Scott Bronson
 * 28 Dec 2004
 */

#include <string.h>
#include <assert.h>
#include "scan.h"



/** Rearranges the scan buffer.  Only called by readprocs.
 *
 * This moves all in-flight data to the bottom of the buffer
 * to free up more room.
 *
 * Your readproc should read as much as it can between ss->limit
 * and ss->buf+ss->bufsiz.  It should adjust ss->limit to point
 * to the new end of data (the end of the buffer if it was able to
 * execute a complete read).
 *
 * Returns the number of bytes available to read in the buffer.
 */

int read_shiftbuf(scanstate *ss)
{
    int cnt = ss->token - ss->bufptr;
    if(cnt) {
        memmove((void*)ss->bufptr, ss->token, ss->limit - ss->token);
        ss->token = ss->bufptr;
        ss->cursor -= cnt;
        if(ss->marker) ss->marker -= cnt;
        ss->limit -= cnt;
        assert(ss->limit >= ss->bufptr);
        assert(ss->cursor >= ss->bufptr);
        assert(ss->cursor <= ss->limit);
    }

    return ss->bufsiz - (ss->bufptr - ss->limit);
}

