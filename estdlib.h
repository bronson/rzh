/* Shim to implement emalloc() and efree() required by the pdpzm code.
 * This file is in the public domain.
 */

#ifndef ESTDLIB_INCLUDED
#define ESTDLIB_INCLUDED

#include <stddef.h>

#if 0
#define ERROR_MEMORY "ESL001 Insufficient memory allocating %lu bytes\n"
void *emalloc(size_t sz);
void efree(void *buf);
#else
#define emalloc(sz) malloc(sz)
#define efree(ptr) free(ptr)
#endif

#endif

