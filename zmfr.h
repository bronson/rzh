/*********************************************************************/
/*                                                                   */
/*  This Program Written by Paul Edwards, 3:711/934@fidonet.         */
/*  Released to the Public Domain                                    */
/*                                                                   */
/*********************************************************************/
/*********************************************************************/
/*                                                                   */
/*  zmfr - routines that operate on zmcore + zmext                   */
/*                                                                   */
/*********************************************************************/

#include <stddef.h>

#include "zmcore.h"
#include "zmext.h"

void extModemClearInbound(ZMCORE *zmcore, ZMEXT *zmext);
void extModemResponseSent(ZMCORE *zmcore, ZMEXT *zmext);
void extModemGetBlock(ZMCORE *zmcore, 
                      ZMEXT *zmext, 
                      void *buf, 
                      size_t max, 
                      size_t *actual);
void extModemRegisterBad(ZMCORE *zmcore, ZMEXT *zmext);
void extModemRegisterGood(ZMCORE *zmcore, ZMEXT *zmext);
void extModemGetBlockImm(ZMCORE *zmcore,
                         ZMEXT *zmext,
                         void *buf, 
                         size_t max, 
                         size_t *actual);
void extModemWriteBlock(ZMCORE *zmcore, ZMEXT *zmext, void *buf, size_t max);
void extFileSetInfo(ZMCORE *zmcore,
                    ZMEXT *zmext,
                    unsigned char *filename, 
                    unsigned char *fileinfo,
                    long *offset,
                    int *skip);
void extFileWriteData(ZMCORE *zmcore, ZMEXT *zmext, void *buf, size_t bytes);
void extFileFinish(ZMCORE *zmcore, ZMEXT *zmext);
int extFileGetFile(ZMCORE *zmcore, 
                   ZMEXT *zmext,
                   unsigned char *buf, 
                   long *filesize);
void extFileSetPos(ZMCORE *zmcore, ZMEXT *zmext, long offset);
int extFileGetData(ZMCORE *zmcore, 
                   ZMEXT *zmext,
                   void *buf, 
                   size_t max, 
                   size_t *bytes);
