/* pdcomm.h
 * Scott Bronson
 *
 * Shim to attach to the pdpzm library
 * This file is in the public domain.
 */


#ifndef PDCOMM_INCLUDED
#define PDCOMM_INCLUDED

struct PDCOMM;

typedef size_t (*pdcomm_proc)(void *buf, size_t num);

typedef struct PDCOMM {
	pdcomm_proc read;
	pdcomm_proc write;
} PDCOMM;

size_t pdcommWriteStdout(void *buf, size_t num);
size_t pdcommReadStdin(void *buf, size_t num);

#define pdcommReadBuf(pd,buf,num) (*(pd)->read)(buf,num)
#define pdcommWriteBuf(pd,buf,num) (*(pd)->write)(buf,num)

void DosSleep(int ct);

#endif
