/*********************************************************************/
/*                                                                   */
/*  This Program Written by Paul Edwards                             */
/*  Released to the Public Domain                                    */
/*                                                                   */
/*********************************************************************/
/*********************************************************************/
/*                                                                   */
/*  zmextm.c - extModem routines                                     */
/*                                                                   */
/*********************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include <error.h>
#include <trav.h>
#include <unused.h>

#include "zmextm.h"
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

