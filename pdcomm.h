/* pdcomm.h
 * 1 Nov 2004
 * Scott Bronson
 *
 * Communication between rzh and pdpzm.
 *
 * This file is released under the MIT license.  This is basically the
 * same as public domain, but absolves the author of liability.
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
