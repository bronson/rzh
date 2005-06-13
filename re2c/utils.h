/* utils.h
 * Scott Bronson
 * 30 Dec 2004
 *
 * Various routines intended to make your life easier.
 */


#include "scan.h"


scanstate* readfd_open(const char *path, int bufsiz);
void readfd_close(scanstate *ss);

