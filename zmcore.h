/*********************************************************************/
/*                                                                   */
/*  This Program Written by Paul Edwards, 3:711/934@fidonet.         */
/*  Released to the Public Domain                                    */
/*                                                                   */
/*********************************************************************/
/*********************************************************************/
/*                                                                   */
/*  zmcore - core zmodem routines                                    */
/*                                                                   */
/*********************************************************************/

#ifndef ZMCORE_INCLUDED
#define ZMCORE_INCLUDED

#include <stdio.h>
#include <time.h>

#include "zmext.h"

#define ZMODEM_INIT "ZMD001 Remote didn't accept my ZRINIT\n"
#define ZMODEM_POS "ZMD002 Remote failed to respond to my ZRPOS\n"
#define ZMODEM_ZDATA "ZMD003 Remote failed to sync to correct position\n"
#define ZMODEM_CRCXM "ZMD004 CRCXM error, ours = %x, theirs = %x\n"
#define ZMODEM_LONGSP "ZMD005 Error - subpacket too long\n"
#define ZMODEM_CRC32 "ZMD006 CRC32 error - ours = %lx, theirs = %lx\n"
#define ZMODEM_FILEDATA "ZMD007 File Data is out of spec\n"            
#define ZMODEM_BADHEX "ZMD008 Bad hex character %x received\n"
#define ZMODEM_TIMEOUT "ZMD009 Timeout\n"
#define ZMODEM_GOTZCAN "ZMD010 Received cancel signal from remote\n"

#define ZMCORE_MAXBUF 18000
#define ZMCORE_MAXTX 1024 /*1024*/

typedef struct {
    int wait;
    unsigned char filename[FILENAME_MAX];
    unsigned char *fileinfo;
    size_t bytes;
    size_t bufsize;
    size_t maxTx;
    long goodOffset;
    long filesize;
    time_t filetime;
    int filemode;
    int gotSpecial;
    int gotHeader;
    int moreData;
    int skip;
    unsigned char frameType;
    unsigned char headerType;
    unsigned char headerData[4];
    unsigned char *mainBuf;
    unsigned char *bufPos;
    unsigned char *bufTop;
    int ch;
    ZMEXT *zmext;
} ZMCORE;

void zmcoreDefaults(ZMCORE *zmcore);
void zmcoreInit(ZMCORE *zmcore, ZMEXT *zmext);
void zmcoreReceive(ZMCORE *zmcore);
void zmcoreSend(ZMCORE *zmcore);
void zmcoreTerm(ZMCORE *zmcore);

#endif
