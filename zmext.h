/*********************************************************************/
/*                                                                   */
/*  This Program Written by Paul Edwards, 3:711/934@fidonet.         */
/*  Released to the Public Domain                                    */
/*                                                                   */
/*********************************************************************/
/*********************************************************************/
/*                                                                   */
/*  zmext - external routines used to support zmcore                 */
/*                                                                   */
/*********************************************************************/

#ifndef ZMEXT_INCLUDED
#define ZMEXT_INCLUDED

#include <pdcomm.h>

#define ZMEXT_MAXFILES 40

typedef struct
{
    int fd;
    PDCOMM *pdcomm;
    char fileList[ZMEXT_MAXFILES][FILENAME_MAX];
    int fileUpto;
} ZMEXT;

void zmextDefaults(ZMEXT *zmext);
void zmextInit(ZMEXT *zmext, PDCOMM *pdcomm);
void zmextTerm(ZMEXT *zmext);
void zmextFileSetSpec(ZMEXT *zmext, char *spec);

#endif
