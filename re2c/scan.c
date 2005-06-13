/* re2c.c
 * Scott Bronson
 * 28 Dec 2004
 */

#include <string.h>
#include <assert.h>
#include "scan.h"


/** Initializes the given scanstate to use the provided buffer.
 * Note that you must attach it to a reader after calling this routine.
 */

void scanstate_init(scanstate *ss, const char *bufptr, int bufsiz)
{
    ss->cursor = bufptr;
    ss->limit = bufptr;
    ss->marker = NULL;
    ss->token = bufptr;
    ss->line = 0;
    ss->at_eof = 0;
    ss->bufptr = bufptr;
    ss->bufsiz = bufsiz;
    ss->readref = NULL;
    ss->read = NULL;
    ss->scanref = NULL;
    ss->state = NULL;
    ss->userref = NULL;
    ss->userproc = NULL;
}


/** Resets the given scanstate to start from the beginning.
 *
 * Doesn't modify:
 * - the reader or the readref.
 * - the scanner or the scanref.
 * - the buffer or buffer size
 *
 * But sets everything else to the default values.
 * You may still need to reattach to the reader if it needs to
 * reset some part of its internal state.  This is true of the
 * scanner too.
 */

void scanstate_reset(scanstate *ss)
{
    ss->cursor = ss->bufptr;
    ss->limit = ss->bufptr;
    ss->marker = NULL;
    ss->token = ss->bufptr;
    ss->line = 0;
    ss->at_eof = 0;
}

