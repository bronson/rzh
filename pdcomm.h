/* pdcomm.h
 * Scott Bronson
 *
 * Shim to attach to the pdpzm library
 * This file is in the public domain.
 */


#ifndef PDCOMM_INCLUDED
#define PDCOMM_INCLUDED

typedef struct {
} PDCOMM;

size_t pdcommWriteBuf(PDCOMM *pdcomm, void *buf, size_t num);
size_t pdcommReadBuf(PDCOMM *pdcomm, void *buf, size_t num);

void DosSleep(int ct);

#endif
