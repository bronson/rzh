/*********************************************************************/
/*                                                                   */
/*  This Program Written by Paul Edwards, 3:711/934@fidonet.         */
/*  Released to the Public Domain                                    */
/*  Written Dec 1993                                                 */
/*                                                                   */
/*********************************************************************/
/*********************************************************************/
/*                                                                   */
/*  error.c - error handling routines.                               */
/*                                                                   */
/*  Refer to error.h for documentation.                              */
/*                                                                   */
/*********************************************************************/

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "error.h"

ERROR error;

void errorSet(char *fmt, ...)
{
    va_list va;

    if (ALLOK)
    {    
        strcpy(error.fmt, fmt);
        va_start(va, fmt);
        vsprintf(error.buf, fmt, va);
        va_end(va);
        error.errorOccurred = 1;
    }
    return;
}

void errorFlush(void)
{
    if (error.errorOccurred)
    {
        printf("%s", error.buf);
        errorClear();
    }
    return;
}
