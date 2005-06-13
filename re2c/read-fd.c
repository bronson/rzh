/* read-fd.c
 * Scott Bronson
 * 28 Dec 2004
 */

#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include "scan-dyn.h"
#include "read-fd.h"


static int readfd_read(scanstate *ss)
{
    int n, avail;

	if(ss->at_eof) {
		// on some platforms, hammering on the eof can have bad consequences.
		return 0;
	}

    avail = read_shiftbuf(ss);

    // ensure we get a full read
    do {
        n = read((int)ss->readref, (void*)ss->limit, avail);
    } while(n < 0 && errno == EINTR);
    ss->limit += n;

    if(n == 0) {
        ss->at_eof = 1;
    }

    return n;
}


/** Attaches the existing fd to the existing scanstate object.
 * Note that this routine checks the fd and if it's less than 0
 * (indicating an error) it returns null.
 *
 * If you pass it a valid fd, there's no way it will possibly fail.
 */

scanstate* readfd_attach(scanstate *ss, int fd)
{
    if(!ss || fd < 0) {
        return 0;
    }

    ss->readref = (void*)fd;
    ss->read = readfd_read;
    return ss;
}

