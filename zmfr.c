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
    unused(fileinfo);
    unused(zmcore);
    printf("opening file %s\n", filename);
    zmext->fq = fopen((char *)filename, "wb");
    if (zmext->fq == NULL)
    {
        printf("failed to open file %s\n", filename);
    }
    *offset = 0;
    *skip = 0;
    return;
}

void extFileWriteData(ZMCORE *zmcore, ZMEXT *zmext, void *buf, size_t bytes)
{
    unused(zmcore);
    fwrite(buf, bytes, 1, zmext->fq);
    return;
}

void extFileFinish(ZMCORE *zmcore, ZMEXT *zmext)
{
    unused(zmcore);
    printf("closing file\n");
    fclose(zmext->fq);
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
    printf("opening file %s\n", buf);
    zmext->fq = fopen((char *)buf, "rb");
    if (zmext->fq == NULL)
    {
        printf("failed to open file %s\n", buf);
    }
    else
    {
        fseek(zmext->fq, 0, SEEK_END);
        *filesize = ftell(zmext->fq);
        rewind(zmext->fq);
        ret = 1;
    }
    return (ret);
}

void extFileSetPos(ZMCORE *zmcore, ZMEXT *zmext, long offset)
{
    unused(zmcore);
    fseek(zmext->fq, offset, SEEK_SET);
    printf("seeking to offset %lu\n", offset);
    return;
}

int extFileGetData(ZMCORE *zmcore,
                   ZMEXT *zmext,
                   void *buf, 
                   size_t max, 
                   size_t *bytes)
{
    unused(zmcore);
    *bytes = fread(buf, 1, max, zmext->fq);
    printf("read %d bytes\n", *bytes);
    return (*bytes != 0);
}
