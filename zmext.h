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

#include <stddef.h>
#include <stdio.h>

#include <pdcomm.h>

typedef struct
{
    PDCOMM *pdcomm;
    int fd;
    char **argv;
} ZMEXT;

#endif
