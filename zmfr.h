/*********************************************************************/
/*                                                                   */
/*  This Program Written by Paul Edwards                             */
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

void extFileSetPos(ZMCORE *zmcore, ZMEXT *zmext, long offset);

void extFileReceiveStart(ZMCORE *zmcore, ZMEXT *zmext);
void extFileReceiveData(ZMCORE *zmcore, ZMEXT *zmext, void *buf, size_t bytes);
void extFileReceiveFinish(ZMCORE *zmcore, ZMEXT *zmext);
void extFileSendStart(ZMCORE *zmcore, ZMEXT *zmext);
void extFileSendData(ZMCORE *zmcore, 
                   ZMEXT *zmext,
                   void *buf, 
                   size_t max, 
                   size_t *bytes);
void extFileSendFinish(ZMCORE *zmcore, ZMEXT *zmext);
