/*********************************************************************/
/*                                                                   */
/*  This Program Written by Paul Edwards                             */
/*  Released to the Public Domain                                    */
/*                                                                   */
/*********************************************************************/
/*********************************************************************/
/*                                                                   */
/*  zmextm - extModem routines                                       */
/*                                                                   */
/*********************************************************************/

#include <stddef.h>

#include "zmcore.h"

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

