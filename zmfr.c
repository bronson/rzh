/*********************************************************************/
/*                                                                   */
/*  This Program Written by Paul Edwards, 3:711/934@fidonet.         */
/*  Released to the Public Domain                                    */
/*                                                                   */
/*********************************************************************/
/*********************************************************************/
/*                                                                   */
/*  zmfr - friend functions that operate on zmcore + zmext           */
/*                                                                   */
/*********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <utime.h>

#include <error.h>
#include <trav.h>
#include <unused.h>

#include "zmext.h"
#include "zmcore.h"
#include "zmfr.h"
#include "pdcomm.h"

void extModemClearInbound(ZMCORE *zmcore, ZMEXT *zmext)
{
    unsigned char buf[200];

    unused(zmcore);    
    while (pdcommReadBuf(zmext->pdcomm, buf, sizeof buf) == sizeof buf) ;
    return;
}

void extModemResponseSent(ZMCORE *zmcore, ZMEXT *zmext)
{
    unused(zmcore);
    unused(zmext);
    return;
}

void extModemGetBlock(ZMCORE *zmcore, 
                      ZMEXT *zmext, 
                      void *buf, 
                      size_t max, 
                      size_t *actual)
{
    int x;
    int cnt;
    
    unused(zmcore);
    x = pdcommReadBuf(zmext->pdcomm, buf, max);
    if (x == 0)
    {
        /* wait for 2 seconds for response */
        for (cnt = 0; cnt < 200; cnt++)
        {
#ifndef MSDOS    
            DosSleep(10);
#else
            delay(10);
#endif            
            x = pdcommReadBuf(zmext->pdcomm, buf, max);
            if (x != 0) break;
        }
    }
    if (x == 0)
    {
        errorSet(ZMODEM_TIMEOUT);
    }
    *actual = x;
    return;
}

void extModemRegisterBad(ZMCORE *zmcore, ZMEXT *zmext)
{
    unused(zmcore);
    unused(zmext);
    return;
}

void extModemRegisterGood(ZMCORE *zmcore, ZMEXT *zmext)
{
    unused(zmcore);
    unused(zmext);
    return;
}

void extModemGetBlockImm(ZMCORE *zmcore, 
                         ZMEXT *zmext,
                         void *buf, 
                         size_t max, 
                         size_t *actual)
{
    int x;

    unused(zmcore);
    x = pdcommReadBuf(zmext->pdcomm, buf, max);
    *actual = x;
    if (x == 0)
    {
        errorSet(ZMODEM_TIMEOUT);
    }
    return;
}

void extModemWriteBlock(ZMCORE *zmcore, ZMEXT *zmext, void *buf, size_t max)
{
/*    FILE *fq; */

    unused(zmcore);    
    pdcommWriteBuf(zmext->pdcomm, buf, max);
    
/*    fq = fopen("debug.log", "ab");
    fwrite(buf, max, 1, fq);
    fclose(fq); */
    return;
}
   
void extFileSetInfo(ZMCORE *zmcore,
                    ZMEXT *zmext,
                    unsigned char *filename, 
                    unsigned char *fileinfo,
                    long *offset,
                    int *skip)
{
    // Won't mark anything executable if we're running with root privs.
    int mask = geteuid() ? 07777 : 06666;
    zmcore->filesize = 0;
	zmcore->actualsize = 0;
    zmcore->filetime = time(NULL);
    long mode = 0644;

    unused(fileinfo);
    unused(zmcore);

    sscanf(fileinfo, "%ld %lo %lo", &zmcore->filesize, &zmcore->filetime, &mode);
    zmext->fd = creat((char *)filename, mode & mask);
    if(zmext->fd == -1)
    {
        fprintf(stderr, "failed to open file %s: %s\n", filename, strerror(errno));
    }
    *offset = 0;
    *skip = 0;
    return;
}

void extFileWriteData(ZMCORE *zmcore, ZMEXT *zmext, void *buf, size_t bytes)
{
    ssize_t cnt;
    unused(zmcore);
	zmcore->actualsize += bytes;
    cnt = write(zmext->fd, buf, bytes);
    if(cnt != bytes) {
        fprintf(stderr, "write failed: didn't write all data!\n");
    }
    return;
}

void extFileFinish(ZMCORE *zmcore, ZMEXT *zmext)
{
    struct utimbuf tv;

    unused(zmcore);
    unused(zmext);

    close(zmext->fd);

    if(zmcore->filetime) {
        tv.actime = tv.modtime = zmcore->filetime;
        utime(zmcore->filename, &tv);
    }

	zmcore->total_bytes_transferred += zmcore->actualsize;
	zmcore->total_files_transferred += 1;

    return;
}

int extFileGetFile(ZMCORE *zmcore, 
                   ZMEXT *zmext, 
                   unsigned char *buf, 
                   long *filesize)
{
    int ret = 0;

    unused(zmcore);    
    strcpy((char *)buf, zmext->fileList[zmext->fileUpto++]);
    zmcore->filetime = 0;
    zmext->fd = open((char *)buf, O_RDONLY);
    if (zmext->fd == -1)
    {
        fprintf(stderr, "failed to open file %s: %s\n", buf, strerror(errno));
    }
    else
    {
        zmcore->actualsize = *filesize = (long)lseek(zmext->fd, 0, SEEK_END);
        lseek(zmext->fd, 0, SEEK_SET);
        ret = 1;
    }
    return (ret);
}

void extFileSetPos(ZMCORE *zmcore, ZMEXT *zmext, long offset)
{
    unused(zmcore);
    lseek(zmext->fd, offset, SEEK_SET);
    return;
}

int extFileGetData(ZMCORE *zmcore,
                   ZMEXT *zmext,
                   void *buf, 
                   size_t max, 
                   size_t *bytes)
{
    unused(zmcore);
    *bytes = read(zmext->fd, buf, max);
    return (*bytes != 0);
}
