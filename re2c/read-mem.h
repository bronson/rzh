/* re2c-mem.c
 * Scott Bronson
 * 30 Dec 2004
 */

#include "read.h"

scanstate* readmem_init(scanstate *ss, const char *data, int len);
scanstate* readmem_attach(scanstate *ss, const char *data, int len);

