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
#include <errno.h>

#include <error.h>
#include <trav.h>
#include <unused.h>

#include "zmfr.h"
#include "pdcomm.h"



/** This can be called on files opened by either extFileReceiveStart
 * or extFileSendStart.
 */
void extFileSetPos(ZMCORE *zmcore, ZMEXT *zmext, long offset)
{
    long curpos;

    unused(zmcore);
    printf("seeking to offset %lu\n", offset);
    curpos = fseek(zmext->fq, offset, SEEK_SET);
    if(curpos != offset) {
        errorSet("Couldn't seek to %lu: %s\n", offset, strerror(errno));
    }
    return;
}


/** Opens a file to receive the incoming data.
 *
 *  The following zmcore parameters are available as input:
 *  - filename: the name of the file
 *  - fileinfo: the zmodem info string for the file
 *  - filemode: the mode as read from fileinfo
 *  - filesize: the supposed file size as read from fileinfo
 *  - filetime: the file modification time as read from fileinfo
 *
 *  and you may set the following zmcore parameters to affect the transfer:
 *  - goodOffset: ?? (defaults to 0)
 *  - skip: ?? (defaults to 0)
 */
void extFileReceiveStart(ZMCORE *zmcore, ZMEXT *zmext)
{
    unused(zmcore);
    printf("opening file %s\n", zmcore->filename);
    zmext->fq = fopen(zmcore->filename, "wb");
    if (zmext->fq == NULL)
    {
        errorSet("failed to open file %s: %s\n", zmcore->filename, strerror(errno));
    }
}

/** Called one or more times to write data to the file.
 * The file was opened using extFileReceiveStart.
 * Bytes will never be 0.
 */
void extFileReceiveData(ZMCORE *zmcore, ZMEXT *zmext, void *buf, size_t bytes)
{
    long cnt;

    unused(zmcore);
    cnt = fwrite(buf, 1, bytes, zmext->fq);
    if(cnt != bytes) {
        errorSet("fwrite failed, wrote only %ld bytes: %s\n", cnt, strerror(errno));
    }
}

/** Closes the file opened by extFileReceiveStart.
 */
void extFileReceiveFinish(ZMCORE *zmcore, ZMEXT *zmext)
{
    int err;

    unused(zmcore);
    printf("closing write file\n");
    err = fclose(zmext->fq);
    if(err != 0) {
        errorSet("Error closing file: %s", strerror(errno));
    }
}

/** Opens a file containing the data to send and reads it attributes.
 *
 *  You must set the following parameters in zmcore:
 *  - filename: the name of the file
 *
 *  you should set the following parameters:
 *  - filesize: the advertised file size (defaults to 0).
 *
 *  you may set the following parameters:
 *  - filemode: the mode (defaults to 0644).
 *  - filetime: the file modification time (defaults to the current time).
 */
void extFileSendStart(ZMCORE *zmcore, ZMEXT *zmext)
{
    strcpy(zmcore->filename, zmext->fileList[zmext->fileUpto++]);
    printf("opening file %s\n", zmcore->filename);
    zmext->fq = fopen(zmcore->filename, "rb");
    if (zmext->fq == NULL)
    {
        errorSet("failed to open file %s: %s\n", zmcore->filename, strerror(errno));
    }
    else
    {
        fseek(zmext->fq, 0, SEEK_END);
        zmcore->filesize = ftell(zmext->fq);
        rewind(zmext->fq);
    }
}


/** Obtains data from the file we're sending.
 * Return the number of bytes read in "bytes" or 0 at EOF.
 */
void extFileSendData(ZMCORE *zmcore,
                   ZMEXT *zmext,
                   void *buf, 
                   size_t max, 
                   size_t *bytes)
{
    unused(zmcore);
    *bytes = fread(buf, 1, max, zmext->fq);
    if(ferror(zmext->fq)) {
        errorSet("Read error: %s\n", strerror(errno));
    }
    printf("read %d bytes\n", *bytes);
}

/** Closes the file opened by extFileSendStart.
 */
void extFileSendFinish(ZMCORE *zmcore, ZMEXT *zmext)
{
    int err;

    unused(zmcore);
    printf("closing read file\n");
    err = fclose(zmext->fq);
    if(err != 0) {
        errorSet("Error closing file: %s", strerror(errno));
    }
}

