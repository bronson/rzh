/*********************************************************************/
/*                                                                   */
/*  This Program Written by Paul Edwards, 3:711/934@fidonet          */
/*  Released to the Public Domain                                    */
/*  Written Dec 1993                                                 */
/*                                                                   */
/*********************************************************************/
/*********************************************************************/
/*                                                                   */
/*  error.h - error handling routines.                               */
/*                                                                   */
/*  This error subsystem is a "minimal" implementation of            */
/*  PDS0001-1994.  Refer to this document (available for FREQ from   */
/*  3:711/934 as PDS0001.*) for a description of all the routines.   */
/*                                                                   */
/*  This minimal implementation is meant to be used for most of      */
/*  your "hack" routines, rather than your full-blown                */
/*  client-server with language translation.                         */
/*                                                                   */
/*  Basically, this means you get to keep your error messages as     */
/*  part of the executable, and you get to format them on the spot.  */
/*                                                                   */
/*  Messages are delivered via printf, and when a message is         */
/*  "flushed", it is simply delivered straight away (via printf)     */
/*  rather than stacking them up waiting for errorTerm(), since it   */
/*  has the same effect anyway.                                      */
/*                                                                   */
/*********************************************************************/

#ifndef ERROR_INCLUDED
#define ERROR_INCLUDED

#include <string.h>

#define ERROR_MAXLEN 300

#define ERROR_INTERNAL "ERR001 Internal error file %s line %d\n"

typedef struct {
    char fmt[ERROR_MAXLEN];
    char buf[ERROR_MAXLEN];
    int  errorOccurred;
} ERROR;

extern ERROR error;

#define errorDefaults()
#define errorInit() errorReset()
#define errorReset() (error.errorOccurred = 0)
#define errorTerm() errorFlush()
#define errorYes() (error.errorOccurred != 0)
#define errorNo() (error.errorOccurred == 0)
#define errorClear() (error.errorOccurred = 0)
#define errorCompare(x) (strcmp(x, error.fmt) == 0)
void errorSet(char *fmt, ...);
void errorFlush(void);
#define ALLOK (errorNo())
#define CC if (!ALLOK) break;
#define INTERNAL_ERROR() (errorSet(ERROR_INTERNAL, __FILE__, __LINE__))
#define TRY(x) x; if (!ALLOK) break
#define SUPPRESS(x) (errorCompare(x) ? errorClear() : 0)

#endif
