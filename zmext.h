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

typedef struct
{
    int fd;
    PDCOMM *pdcomm;
	char **argv;
} ZMEXT;

#endif
