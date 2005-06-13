/* r2read-fp.c
 * Scott Bronson
 * 28 Dec 2004
 */

#include <stdio.h>

#include "scan-dyn.h"
#include "read-fp.h"


static int readfp_read(scanstate *ss)
{
    int n, avail;

    avail = read_shiftbuf(ss);
    n = fread((void*)ss->limit, avail, 1, ss->readref);
    ss->limit += n;

    if(n <= 0) {
        if(feof(ss->readref)) {
            // too few bytes to complete the current token.
            ss->at_eof = 1;
            return 0;
        }
        if(ferror(ss->readref)) {
            // error while reading stream.
            // too bad stdlib doesn't allow us to
            // determine what the error is.
            ss->at_eof = 2;
            return -1;
        }
       
        assert(!"Not eof or error. I have no idea what happened!");
        return -3;
    }

    return n;
}


scanstate* readfp_attach(scanstate *ss, FILE *fp)
{
    if(!ss || !fp) {
        return 0;
    }

    ss->readref = fp;
    ss->read = readfp_read;
    return ss;
}


/** Creates a scanstate object that can read from the given file.
 * Returns NULL and prints to STDERR if an error ocurrs.
 * Ensure that you call readfp_close() when you're finished.
 * Uses the given buffer size, or BUFSIZ if bufsiz is 0.
 * Ensure that the buffer size will fit into a signed
 * int on the current machine architecture.
 */

scanstate* readfp_open(const char *path, int bufsiz)
{
    scanstate *ss;
    FILE *fp;

    // open the file
    fp = fopen(path, "r");
    if(!fp) {
        return NULL;
    }

    // create the dynamic scanstate
    ss = dynscan_create(bufsiz);
    if(!ss) {
        fclose(fp);
        return NULL;
    }

    return readfp_attach(ss, fp);
}


/** Releases the resources allocated by readfp_open()
 */

void readfp_close(scanstate *ss)
{
    fclose(ss->readref);
    dynscan_free(ss);
}


