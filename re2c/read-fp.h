/* r2read-fp.h
 * Scott Bronson
 * 28 Dec 2004
 *
 * This allows you to feed an re2c scanner directly from a
 * std C fileptr.
 */


#include <stdio.h>
#include "read.h"


scanstate* readfp_attach(scanstate *ss, FILE *fp);
scanstate* readfp_open(const char *filename, int bufsiz);
void readfp_close(scanstate *ss);

