/* r2read-fd.c
 * Scott Bronson
 * 28 Dec 2004
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils.h"
#include "scan-dyn.h"
#include "read-fd.h"



/* Creates a new scanner and opens the given file.
 *
 * You need to call readfd_close() to close the file and
 * deallocate the scanner.
 *
 * Bufsiz tells how big the scan buffer will be.  No single
 * token may be larger than bufsiz (it will be broken up
 * and the scanner may return strange things if it is).
 */

scanstate* readfd_open(const char *path, int bufsiz)
{
    scanstate *ss;
    int fd;

    fd = open(path, O_RDONLY);
    if(fd < 0) {
        return NULL;
    }

    ss = dynscan_create(bufsiz);
    if(!ss) {
        close(fd);
        return NULL;
    }

    return readfd_attach(ss, fd);
}


void readfd_close(scanstate *ss)
{
    close((int)ss->readref);
    dynscan_free(ss);
}

