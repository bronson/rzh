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

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>

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
    off_t curpos;

    unused(zmcore);
    curpos = lseek(zmext->fd, offset, SEEK_SET);
    if(curpos != offset) {
        errorSet("Couldn't seek to %lu on %s: %s\n",
                offset, zmcore->filename, strerror(errno));
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
 *  - filetime: the file modification time as read from fileinfo ((time_t)(-1) if unspecified)
 *
 *  and you may set the following zmcore parameters to affect the transfer:
 *  - goodOffset: ?? (defaults to 0)
 *  - skip: ?? (defaults to 0)
 */
void extFileReceiveStart(ZMCORE *zmcore, ZMEXT *zmext)
{
    zmext->fd = open(zmcore->filename, O_CREAT|O_WRONLY|O_TRUNC);
    if (zmext->fd < 0)
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
    cnt = write(zmext->fd, buf, bytes);
    if(cnt != bytes) {
        errorSet("fwrite failed on %s, wrote only %ld bytes: %s\n",
                zmcore->filename, cnt, strerror(errno));
    }
}

/** Closes the file opened by extFileReceiveStart.
 */
void extFileReceiveFinish(ZMCORE *zmcore, ZMEXT *zmext)
{
    struct utimbuf tv;
    int err;

    err = close(zmext->fd);
    if(err != 0) {
        errorSet("Error closing receive file %s: %s", zmcore->filename, strerror(errno));
        return;
    }

    // Set file mtime.
    if(zmcore->filetime != (time_t)(-1)) {
        tv.actime = tv.modtime = zmcore->filetime;
        err = utime(zmcore->filename, &tv);
        if(err) {
            fprintf(stderr, "Error setting mtime for %s: %s\n",
                    zmcore->filename, strerror(errno));
        }
    }

    // Set file mode.  Ensure that we don't set exec or special permissions
    // if user is running as root.
    err = chmod(zmcore->filename, zmcore->filemode &
            (geteuid() ? 07777 : 00666));
    if(err) {
        fprintf(stderr, "Error setting permissions for %s to 0%o: %s\n",
                zmcore->filename, (int)zmcore->filemode, strerror(errno));
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
 *  - filetime: the file modification time (defaults to (time_t)(-1) -- unspecified).
 */
void extFileSendStart(ZMCORE *zmcore, ZMEXT *zmext)
{
    struct stat st;
    int err;

    if(!*zmext->argv) {
        // no more files
        return;
    }
    strncpy(zmcore->filename, *zmext->argv, sizeof(zmcore->filename));
    zmcore->filename[sizeof(zmcore->filename)-1] = '\0';
    zmext->argv++;

    // open it
    zmext->fd = open(zmcore->filename, O_RDONLY);
    if (zmext->fd == -1) {
        errorSet("failed to open file %s: %s\n", zmcore->filename, strerror(errno));
    } else {
        // and read its attrs
        err = fstat(zmext->fd, &st);
        if(err) {
            errorSet("failed to stat file %s: %s\n", zmcore->filename, strerror(errno));
        } else {
            // both times are expressed in seconds since the epoch.
            zmcore->filetime = st.st_mtime;
            zmcore->filesize = st.st_size;
            zmcore->filemode = st.st_mode;
        }
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
    do {
        *bytes = read(zmext->fd, buf, max);
        if(*bytes < 0) {
            errorSet("Read error on %s: %s\n", zmcore->filename, strerror(errno));
            break;
        }
        buf += *bytes;
        max -= *bytes;
    } while(max > 0);
}

/** Closes the file opened by extFileSendStart.
*/
void extFileSendFinish(ZMCORE *zmcore, ZMEXT *zmext)
{
    int err;

    err = close(zmext->fd);
    if(err != 0) {
        errorSet("Error closing send file %s: %s", zmcore->filename, strerror(errno));
        return;
    }
}

