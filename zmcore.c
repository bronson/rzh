/*********************************************************************/
/*                                                                   */
/*  This Program Written by Paul Edwards, 3:711/934@fidonet.         */
/*  Released to the Public Domain                                    */
/*                                                                   */
/*********************************************************************/
/*********************************************************************/
/*                                                                   */
/*  zmcore - core zmodem routines                                    */
/*                                                                   */
/*********************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include <crcxm.h>
#include <crc32.h>
#include <error.h>
#include <estdlib.h>

#include "zmext.h"
#include "zmcore.h"
#include "zmfr.h"
#include "unused.h"

#define ZPAD 0x2a
#define ZDLE 0x18
#define ZBIN 0x41
#define ZHEX 0x42
#define ZBIN32 0x43
#define ZRQINIT 0x00
#define ZRINIT 0x01
#define CANFDX 0x01
#define CANOVIO 0x02
#define CANFC32 0x20
#define ZACK  0x03
#define ZFILE 0x04
#define ZSKIP 0x05
#define ZCRCW 0x6b
#define ZRPOS 0x09
#define ZDATA 0x0a
#define ZCRCG 0x69
#define ZEOF 0x0b
#define ZFIN 0x08
#define ZRUB0 0x6C /* 'l' - escape for 0x7f */
#define ZRUB1 0x6D /* 'm' - escape for 0xff */

#define ZCBIN 0x01
#define ZCRCE 0x68
#define ZCAN 0xfe /* not implemented properly yet */

static unsigned char hexdigit[] = 
    "\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x61\x62\x63\x64\x65\x66";

static void receiveFile(ZMCORE *zmcore);
static void receiveData(ZMCORE *zmcore);
static int posMatch(ZMCORE *zmcore);
static void getData(ZMCORE *zmcore);
static void getData16(ZMCORE *zmcore);
static void getData32(ZMCORE *zmcore);
static void getFileData(ZMCORE *zmcore);
static void getZMHeaderImmediate(ZMCORE *zmcore);
static void getZMHeader(ZMCORE *zmcore);
static void getHexHeader(ZMCORE *zmcore);
static void getBinaryHeader(ZMCORE *zmcore);
static void getBin32Header(ZMCORE *zmcore);
static void getNextHexCh(ZMCORE *zmcore);
static void getNextDLECh(ZMCORE *zmcore);
static void getNextCh(ZMCORE *zmcore);
static void sendHeader(ZMCORE *zmcore);
static void sendHexHeader(ZMCORE *zmcore);
static void sendBinHeader(ZMCORE *zmcore);
static void sendBin32Header(ZMCORE *zmcore);
static void sendChar(ZMCORE *zmcore);
static void sendDLEChar(ZMCORE *zmcore);
static void sendHexChar(ZMCORE *zmcore);
static void sendZRINIT(ZMCORE *zmcore);
static void sendZSKIP(ZMCORE *zmcore);
static void sendZRPOS(ZMCORE *zmcore);
static void sendZFIN(ZMCORE *zmcore);
static void getOO(ZMCORE *zmcore);

static void sendrz(ZMCORE *zmcore);
static void sendZRQINIT(ZMCORE *zmcore);
static void sendZFILE(ZMCORE *zmcore);
static void sendFILEINFO(ZMCORE *zmcore);
static void sendZDATA(ZMCORE *zmcore);
static void sendDATA(ZMCORE *zmcore);
static void sendZEOF(ZMCORE *zmcore);
static void sendOO(ZMCORE *zmcore);
static void sendFiles(ZMCORE *zmcore);
static void sendFile(ZMCORE *zmcore);

#define ZMODEM_INIT "ZMD001 Remote didn't accept my ZRINIT\n"
#define ZMODEM_POS "ZMD002 Remote failed to respond to my ZRPOS\n"
#define ZMODEM_ZDATA "ZMD003 Remote failed to sync to correct position\n"
#define ZMODEM_CRCXM "ZMD004 CRCXM error, ours = %x, theirs = %x\n"
#define ZMODEM_LONGSP "ZMD005 Error - subpacket too long\n"
#define ZMODEM_CRC32 "ZMD006 CRC32 error - ours = %lx, theirs = %lx\n"
#define ZMODEM_FILEDATA "ZMD007 File Data is out of spec\n"            
#define ZMODEM_BADHEX "ZMD008 Bad hex character %x received\n"
#define ZMODEM_TIMEOUT "ZMD009 Timeout\n"

/* strictly we don't need to escape all of these characters.
   If the environment can send 0x7f we don't need to DLE
   escape it.  If the environment is 8-bit clean we don't
   need all the 8-bit escapes.  But we escape everything
   here by default to be on the safe side. */

#ifdef DONT_ESCAPE_7F
#define needsDLE2(x) (((x) == ZDLE) || ((x) == 0x10) || ((x) == 0x11) \
                     || ((x) == 0x13) )
#else
#define needsDLE2(x) (((x) == ZDLE) || ((x) == 0x10) || ((x) == 0x11) \
                     || ((x) == 0x7f) \
                     || ((x) == 0x13) )
#endif

#ifdef IS_8BIT_CLEAN
#define needsDLE(x) needsDLE2((x))
#else
#define needsDLE(x) needsDLE2((x) & ~0x80)
#endif

void zmcoreDefaults(ZMCORE *zmcore)
{
    zmcore->bufsize = ZMCORE_MAXBUF;
    zmcore->maxTx = ZMCORE_MAXTX;
    return;
}

void zmcoreInit(ZMCORE *zmcore, ZMEXT *zmext)
{
    zmcore->wait = 1;
    zmcore->mainBuf = emalloc(zmcore->bufsize);
    zmcore->zmext = zmext;
    return;
}

void zmcoreTerm(ZMCORE *zmcore)
{
    efree(zmcore->mainBuf);
    return;
}

void zmcoreReceive(ZMCORE *zmcore)
{
    int fin, quit, tries;

    zmcore->bufTop = zmcore->mainBuf + zmcore->bufsize;
    extModemClearInbound(zmcore, zmcore->zmext);
    zmcore->skip = 0;
    fin = 0;
    while (ALLOK && !fin)
    {
        sendZRINIT(zmcore);
        quit = 0;
        tries = 0;
        while (ALLOK && !quit)
        {
            getZMHeader(zmcore);
            if (ALLOK)
            {
                if (zmcore->headerType == ZFIN)
                {
                    sendZFIN(zmcore);
                    extModemRegisterGood(zmcore, zmcore->zmext);
                    getOO(zmcore);
                    quit = 1;
                    fin = 1;
                }
                else if (zmcore->headerType == ZRQINIT)
                {    
                    if (tries < 10)
                    {
                        sendZRINIT(zmcore);
                        tries++;
                    }
                    else
                    {
                        errorSet(ZMODEM_INIT);
                    }
                }
                else if (zmcore->headerType == ZFILE)
                {
                    zmcore->skip = 0;
                    extModemRegisterGood(zmcore, zmcore->zmext);
                    getFileData(zmcore);
                    if (ALLOK)
                    {
                        receiveFile(zmcore);
                        quit = 1;
                    }
                    else if (errorCompare(ZMODEM_CRC32))
                    {
                        if (tries < 10)
                        {
                            errorFlush();
                            sendZRINIT(zmcore);
                            tries++;
                        }
                    }
                }
            }
            else
            {
                if (errorCompare(ZMODEM_TIMEOUT)
                    || errorCompare(ZMODEM_CRCXM)
                    || errorCompare(ZMODEM_CRC32)
                    || errorCompare(ZMODEM_BADHEX))
                {
                    if (tries < 10)
                    {
                        errorFlush();
                        sendZRINIT(zmcore);
                        tries++;
                    }
                }
            }
        }
    }
    return;
}

static void receiveFile(ZMCORE *zmcore)
{
    int quit;
    int tries;

#ifdef DEBUG
    printf("in receiveFile\n");
#endif
    zmcore->goodOffset = 0;
    extFileSetInfo(zmcore, 
                   zmcore->zmext,
                   zmcore->filename, 
                   zmcore->fileinfo, 
                   &zmcore->goodOffset, 
                   &zmcore->skip);
    if (zmcore->skip)
    {
        sendZSKIP(zmcore);
    }    
    else
    {
        sendZRPOS(zmcore);
        quit = 0;
        tries = 0;
        while (ALLOK && !quit)
        {
            getZMHeader(zmcore);
            if (ALLOK)
            {
                if (zmcore->headerType == ZFILE)
                {
                    if (tries < 10)
                    {
                        sendZRPOS(zmcore);
                        tries++;
                    }
                    else
                    {
                        errorSet(ZMODEM_POS);
                    }
                }
                else if (zmcore->headerType == ZDATA)
                {
                    extModemRegisterGood(zmcore, zmcore->zmext);
                    receiveData(zmcore);
                    quit = 1;
                }
                else if (zmcore->headerType == ZEOF)
                {
                    if (posMatch(zmcore))
                    {
                        extModemRegisterGood(zmcore, zmcore->zmext);
                        extFileFinish(zmcore, zmcore->zmext);
                        quit = 1;
                    }
                    else
                    {
                        if (tries < 10)
                        {
                            sendZRPOS(zmcore);
                            tries++;
                        }
                        else
                        {
                            errorSet(ZMODEM_POS);
                        }
                    }
                }
            }
            else
            {
                if (errorCompare(ZMODEM_TIMEOUT) 
                    || errorCompare(ZMODEM_CRCXM)
                    || errorCompare(ZMODEM_CRC32))
                {
                    if (tries < 10)
                    {
                        errorFlush();
                        sendZRPOS(zmcore);
                        tries++;
                    }
                }
            } 
        }
    }
    return;
}
    
static void receiveData(ZMCORE *zmcore)
{
    int quit;
    int tries;
    
    quit = 0;
    tries = 0;
    zmcore->moreData = 1;
    while (ALLOK && !quit)
    {
        if (zmcore->moreData)
        {
            getData(zmcore);
            if (ALLOK)
            {
                extFileWriteData(zmcore, 
                                 zmcore->zmext,
                                 zmcore->mainBuf, 
                                 (size_t)(zmcore->bufPos - zmcore->mainBuf));
                tries = 0;
            }
        }
        else
        {
#ifdef DEBUG
            printf("getting header\n");
#endif
            getZMHeader(zmcore);
#ifdef DEBUG
            printf("finished getting header\n");
#endif
            if (ALLOK)
            {
                if (zmcore->headerType == ZDATA)
                {
                    if (posMatch(zmcore))
                    {
                        extModemRegisterGood(zmcore, zmcore->zmext);
                        zmcore->moreData = 1;
#ifdef DEBUG
                        printf("got ZDATA for position %ld\n", 
                               zmcore->goodOffset);
#endif
                    }
                }
                else if (zmcore->headerType == ZEOF)
                {
                    if (posMatch(zmcore))
                    {
                        extModemRegisterGood(zmcore, zmcore->zmext);
                        extFileFinish(zmcore, zmcore->zmext);
                        quit = 1;
                    }
                }
            }
        }
        if (!ALLOK)
        {
            if (errorCompare(ZMODEM_TIMEOUT)
                || errorCompare(ZMODEM_LONGSP)
                || errorCompare(ZMODEM_CRCXM)
                || errorCompare(ZMODEM_CRC32))
            {
                if (tries < 10)
                {
                    errorFlush();
                    zmcore->moreData = 0;
                    sendZRPOS(zmcore);
                    tries++;
                }
            }
        }
    }
    return;
}

static int posMatch(ZMCORE *zmcore)
{
    long templ;
    int ret;

    templ = zmcore->headerData[3];
    templ = (templ << 8) | zmcore->headerData[2];
    templ = (templ << 8) | zmcore->headerData[1];
    templ = (templ << 8) | zmcore->headerData[0];
    if (templ == zmcore->goodOffset)
    {
        ret = 1;
    }
    else
    {
#ifdef DEBUG
        printf("failed to match their offset %ld with ours %ld\n",
               templ, zmcore->goodOffset);
#endif
        ret = 0;
    }
    return (ret);
}

static void getData(ZMCORE *zmcore)
{
    if (zmcore->frameType == ZBIN32)
    {
        getData32(zmcore);
    }
    else
    {
        getData16(zmcore);
    }
    if (!ALLOK)
    {
        zmcore->moreData = 0;
    }
    else
    {
        extModemRegisterGood(zmcore, zmcore->zmext);
    }
    return;
}

static void getData16(ZMCORE *zmcore)
{
    int quit;
    CRCXM crc;
    unsigned int theirCRC;
    
    zmcore->bufPos = zmcore->mainBuf;
    quit = 0;
    crcxmInit(&crc);
    while (ALLOK && (zmcore->bufPos < zmcore->bufTop) && !quit)
    {
        getNextDLECh(zmcore);
        if (ALLOK)
        {
            if (zmcore->gotSpecial)
            {
                if (zmcore->ch != ZCRCG)
                {
                    zmcore->moreData = 0;
                }
                crcxmUpdate(&crc, zmcore->ch);
                getNextDLECh(zmcore);
                if (ALLOK)
                {
                    theirCRC = zmcore->ch;
                    getNextDLECh(zmcore);
                }
                if (ALLOK)
                {
                    theirCRC = (theirCRC << 8) | zmcore->ch;
                    if (crcxmValue(&crc) != theirCRC)
                    {
                        errorSet(ZMODEM_CRCXM, 
                                 crcxmValue(&crc), 
                                 theirCRC);
                    }
                    else
                    {
                        zmcore->goodOffset += (zmcore->bufPos - zmcore->mainBuf);
                        quit = 1;
                    }
                }
            }
            else
            {
                crcxmUpdate(&crc, zmcore->ch);
                *zmcore->bufPos = (unsigned char)zmcore->ch;
                zmcore->bufPos++;
            }
        }
    }
    if (zmcore->bufPos == zmcore->bufTop)
    {
        errorSet(ZMODEM_LONGSP);
    }
    return;
}

static void getData32(ZMCORE *zmcore)
{
    int quit;
    CRC32 crc;
    unsigned long theirCRC;
    
    zmcore->bufPos = zmcore->mainBuf;
    quit = 0;
    crc32Init(&crc);
    while (ALLOK && (zmcore->bufPos < zmcore->bufTop) && !quit)
    {
        getNextDLECh(zmcore);
        if (ALLOK)
        {
            if (zmcore->gotSpecial)
            {
                if (zmcore->ch != ZCRCG)
                {
                    zmcore->moreData = 0;
                }
                crc32Update(&crc, zmcore->ch);
                getNextDLECh(zmcore);
                if (ALLOK)
                {
                    theirCRC = zmcore->ch;
                    getNextDLECh(zmcore);
                }
                if (ALLOK)
                {
                    theirCRC = theirCRC | ((unsigned long)zmcore->ch << 8);
                    getNextDLECh(zmcore);
                }
                if (ALLOK)
                {
                    theirCRC = theirCRC | ((unsigned long)zmcore->ch << 16);
                    getNextDLECh(zmcore);
                }
                if (ALLOK)
                {
                    theirCRC = theirCRC | ((unsigned long)zmcore->ch << 24);
                    if (~crc32Value(&crc) != theirCRC)
                    {
                        errorSet(ZMODEM_CRC32,
                                 ~crc32Value(&crc), 
                                 theirCRC);
#ifdef DEBUG
                        printf("got CRC32 error, last good offset %ld\n",
                               zmcore->goodOffset);
#endif
                    }
                    else
                    {
                        zmcore->goodOffset += (zmcore->bufPos - zmcore->mainBuf);
                        quit = 1;
                    }
                }
            }
            else
            {
                crc32Update(&crc, zmcore->ch);
                *zmcore->bufPos = (unsigned char)zmcore->ch;
                zmcore->bufPos++;
            }
        }
    }
    if (zmcore->bufPos == zmcore->bufTop)
    {
        errorSet(ZMODEM_LONGSP);
    }
    return;
}

static void getFileData(ZMCORE *zmcore)
{
    unsigned char *pos;
    int problem;
    
    getData(zmcore);
    if (ALLOK)
    {
        problem = 1;
        strncpy((char *)zmcore->filename, 
                (char *)zmcore->mainBuf,
                sizeof zmcore->filename);
        zmcore->filename[FILENAME_MAX - 1] = '\0';
        pos = memchr(zmcore->mainBuf, '\0', zmcore->bufsize - 1);
        if (pos != NULL)
        {
            pos++;
            zmcore->fileinfo = pos;
            pos = memchr(pos, 
                         '\0',
                         (size_t)(zmcore->bufPos - pos));
            if (pos != NULL)
            {
                if ((pos - zmcore->mainBuf) < 1024)
                {
                    problem = 0;
                }
            }        
        }
        if (problem)
        {
#ifdef DEBUG
            printf("setting error\n");
#endif
            errorSet(ZMODEM_FILEDATA);
        }
    }
    return;
}

static void getZMHeaderImmediate(ZMCORE *zmcore)
{
    zmcore->wait = 0;
    getZMHeader(zmcore);
    zmcore->wait = 1;
    return;
}

static void getZMHeader(ZMCORE *zmcore)
{
    int count;
    
    zmcore->gotHeader = 0;
    getNextCh(zmcore);
    while (ALLOK && !zmcore->gotHeader)
    {
        while (ALLOK && (zmcore->ch != ZPAD))
        {
            extModemRegisterBad(zmcore, zmcore->zmext);
            getNextCh(zmcore);
        }
        if (ALLOK)
        {
            count = 1;
            getNextCh(zmcore);
            while (ALLOK && (zmcore->ch == ZPAD))
            {
                count++;
                if (count > 2)
                {
                    extModemRegisterBad(zmcore, zmcore->zmext);
                }
                getNextCh(zmcore); 
            }
            if (ALLOK && (zmcore->ch == ZDLE))
            {
                getNextCh(zmcore);
                if (ALLOK)
                {
                    if (zmcore->ch == ZBIN)
                    {
                        zmcore->frameType = ZBIN;
                        getBinaryHeader(zmcore);
                    }
                    else if (zmcore->ch == ZBIN32)
                    {
                        zmcore->frameType = ZBIN32;
                        getBin32Header(zmcore);
                    }
                    else if ((zmcore->ch == ZHEX) && (count >= 2))
                    {
                        zmcore->frameType = ZHEX;
                        getHexHeader(zmcore);
                    }
                }
            }
        }
    }
    if (zmcore->gotHeader)
    {
#ifdef DEBUG
        printf("received headerType %x\n", zmcore->headerType);
#endif
    }
    return;
}

static void getHexHeader(ZMCORE *zmcore)
{
    CRCXM crc;
    unsigned int theirCRC;
    
    getNextHexCh(zmcore);
    while (ALLOK && !zmcore->gotHeader)
    {
        crcxmInit(&crc);
        zmcore->headerType = (unsigned char)zmcore->ch;
        crcxmUpdate(&crc, zmcore->ch);
        getNextHexCh(zmcore);
        if (ALLOK)
        {
            zmcore->headerData[0] = (unsigned char)zmcore->ch;
            crcxmUpdate(&crc, zmcore->ch);
            getNextHexCh(zmcore);
        }
        if (ALLOK)
        {
            zmcore->headerData[1] = (unsigned char)zmcore->ch;
            crcxmUpdate(&crc, zmcore->ch);
            getNextHexCh(zmcore);
        }
        if (ALLOK)
        {
            zmcore->headerData[2] = (unsigned char)zmcore->ch;
            crcxmUpdate(&crc, zmcore->ch);
            getNextHexCh(zmcore);
        }
        if (ALLOK)
        {
            zmcore->headerData[3] = (unsigned char)zmcore->ch;
            crcxmUpdate(&crc, zmcore->ch);
            getNextHexCh(zmcore);
        }
        if (ALLOK)
        {
            theirCRC = zmcore->ch;
            getNextHexCh(zmcore);
        }
        if (ALLOK)
        {
            theirCRC = (theirCRC << 8) | zmcore->ch;
            if (crcxmValue(&crc) != theirCRC)
            {
                errorSet(ZMODEM_CRCXM, crcxmValue(&crc), theirCRC);
            }
            else
            {
                /*
                   check for CR (optional) & LF
                 */
                 
                getNextCh(zmcore);
                if (ALLOK && (zmcore->ch == 0x0d))
                {
                    getNextCh(zmcore);
                }
                if (ALLOK)
                {
                    zmcore->gotHeader = 1;
                }
            }
        }
    }
    return;
}

static void getBinaryHeader(ZMCORE *zmcore)
{
    CRCXM crc;
    unsigned int theirCRC;

    getNextDLECh(zmcore);
    while (ALLOK && !zmcore->gotHeader)
    {
        crcxmInit(&crc);
        zmcore->headerType = (unsigned char)zmcore->ch;
        crcxmUpdate(&crc, zmcore->ch);
        getNextDLECh(zmcore);
        if (ALLOK)
        {
            zmcore->headerData[0] = (unsigned char)zmcore->ch;
            crcxmUpdate(&crc, zmcore->ch);
            getNextDLECh(zmcore);
        }
        if (ALLOK)
        {
            zmcore->headerData[1] = (unsigned char)zmcore->ch;
            crcxmUpdate(&crc, zmcore->ch);
            getNextDLECh(zmcore);
        }
        if (ALLOK)
        {
            zmcore->headerData[2] = (unsigned char)zmcore->ch;
            crcxmUpdate(&crc, zmcore->ch);
            getNextDLECh(zmcore);
        }
        if (ALLOK)
        {
            zmcore->headerData[3] = (unsigned char)zmcore->ch;
            crcxmUpdate(&crc, zmcore->ch);
            getNextDLECh(zmcore);
        }
        if (ALLOK)
        {
            theirCRC = zmcore->ch;
            getNextDLECh(zmcore);
        }
        if (ALLOK)
        {
            theirCRC = (theirCRC << 8) | zmcore->ch;
            if (crcxmValue(&crc) != theirCRC)
            {
                errorSet(ZMODEM_CRCXM, crcxmValue(&crc), theirCRC);
            }
            else
            {
                zmcore->gotHeader = 1;
            }
        }
    }
    return;
}

static void getBin32Header(ZMCORE *zmcore)
{
    CRC32 crc;
    unsigned long theirCRC;
    
    getNextDLECh(zmcore);
    while (ALLOK && !zmcore->gotHeader)
    {
        crc32Init(&crc);
        zmcore->headerType = (unsigned char)zmcore->ch;
        crc32Update(&crc, zmcore->ch);
        getNextDLECh(zmcore);
        if (ALLOK)
        {
            zmcore->headerData[0] = (unsigned char)zmcore->ch;
            crc32Update(&crc, zmcore->ch);
            getNextDLECh(zmcore);
        }
        if (ALLOK)
        {
            zmcore->headerData[1] = (unsigned char)zmcore->ch;
            crc32Update(&crc, zmcore->ch);
            getNextDLECh(zmcore);
        }
        if (ALLOK)
        {
            zmcore->headerData[2] = (unsigned char)zmcore->ch;
            crc32Update(&crc, zmcore->ch);
            getNextDLECh(zmcore);
        }
        if (ALLOK)
        {
            zmcore->headerData[3] = (unsigned char)zmcore->ch;
            crc32Update(&crc, zmcore->ch);
            getNextDLECh(zmcore);
        }
        if (ALLOK)
        {
            theirCRC = (unsigned long)zmcore->ch;
            getNextDLECh(zmcore);
        }
        if (ALLOK)
        {
            theirCRC = theirCRC | ((unsigned long)zmcore->ch << 8);
            getNextDLECh(zmcore);
        }
        if (ALLOK)
        {
            theirCRC = theirCRC | ((unsigned long)zmcore->ch << 16);
            getNextDLECh(zmcore);
        }
        if (ALLOK)
        {
            theirCRC = theirCRC | ((unsigned long)zmcore->ch << 24);
            if (~crc32Value(&crc) != theirCRC)
            {
                errorSet(ZMODEM_CRC32, ~crc32Value(&crc), theirCRC);
            }
            else
            {
                zmcore->gotHeader = 1;
            }
        }
    }
    return;
}

static void getNextHexCh(ZMCORE *zmcore)
{
    int tempCh;
    
    getNextCh(zmcore);
    if (ALLOK)
    { 
        if ((zmcore->ch <= 0x39) && (zmcore->ch >= 0x30))
        {
            tempCh = (zmcore->ch - 0x30);
        }
        else if ((zmcore->ch >= 0x61) && (zmcore->ch <= 0x66))
        {
            tempCh = (zmcore->ch - 0x61) + 0x0a;
        }
        else 
        {
            errorSet(ZMODEM_BADHEX, zmcore->ch);
        }
        if (ALLOK)
        {
            getNextCh(zmcore);
        }
        if (ALLOK)
        {
            tempCh = tempCh << 4;
            if ((zmcore->ch <= 0x39) && (zmcore->ch >= 0x30))
            {
                zmcore->ch = (zmcore->ch - 0x30);
            }
            else if ((zmcore->ch >= 0x61) && (zmcore->ch <= 0x66))
            {
                zmcore->ch = (zmcore->ch - 0x61) + 0x0a;
            }
            else
            {
                errorSet(ZMODEM_BADHEX, zmcore->ch);
            }
        }
        if (ALLOK)
        {
            zmcore->ch = zmcore->ch | tempCh;
        }
    }
    return;
}

static void getNextDLECh(ZMCORE *zmcore)
{
    zmcore->gotSpecial = 0;
    getNextCh(zmcore);
    if (ALLOK)
    {
        if (zmcore->ch == ZDLE)
        {
            getNextCh(zmcore);
            if (ALLOK)
            {
                zmcore->gotSpecial = 0;
                if (zmcore->ch == ZRUB0)
                {
                    zmcore->ch = 0x7f;
                }
                else if (zmcore->ch == ZRUB1)
                {
                    zmcore->ch = 0xff;
                }
                else if (((zmcore->ch & 0x40) != 0) 
                         && ((zmcore->ch & 0x20) == 0))
                {
                    zmcore->ch &= 0xbf;
                }
                else
                {
                    zmcore->gotSpecial = 1;
                }
            }
        }
    }
    return;
}

static void getNextCh(ZMCORE *zmcore)
{
    unsigned char buf[1];
    size_t actual;
    
    if (zmcore->wait)
    {
        extModemGetBlock(zmcore, zmcore->zmext, buf, 1, &actual);
    }
    else
    {
        extModemGetBlockImm(zmcore, zmcore->zmext, buf, 1, &actual);
    }
    if (ALLOK && (actual == 1))
    {
        zmcore->ch = buf[0];
    }
    return;
}

static void sendHeader(ZMCORE *zmcore)
{
#ifdef DEBUG
    printf("sending header %x\n", zmcore->headerType);
#endif
    if (zmcore->frameType == ZHEX)
    {
        sendHexHeader(zmcore);
    }
    else if (zmcore->frameType == ZBIN)
    {
        sendBinHeader(zmcore);
    }
    else if (zmcore->frameType == ZBIN32)
    {
        sendBin32Header(zmcore);
    }
    if (ALLOK)
    {
        extModemResponseSent(zmcore, zmcore->zmext);
    }
    return;
}

static void sendHexHeader(ZMCORE *zmcore)
{
    CRCXM crc;

    crcxmInit(&crc);
    
    /* send ZPAD, ZPAD, ZDLE ... */
    zmcore->ch = ZPAD;
    sendChar(zmcore);
    if (ALLOK)
    {
        sendChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = ZDLE;
        sendChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->frameType;
        sendChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerType;
        crcxmUpdate(&crc, zmcore->ch);
        sendHexChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerData[0];
        crcxmUpdate(&crc, zmcore->ch);
        sendHexChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerData[1];
        crcxmUpdate(&crc, zmcore->ch);
        sendHexChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerData[2];
        crcxmUpdate(&crc, zmcore->ch);
        sendHexChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerData[3];
        crcxmUpdate(&crc, zmcore->ch);
        sendHexChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = crcxmHighbyte(&crc);
        sendHexChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = crcxmLowbyte(&crc);
        sendHexChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = 0x0d;
        sendChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = 0x0a;
        sendChar(zmcore);
    }
    if (ALLOK)
    {
        if ((zmcore->headerType != ZFIN)
            && (zmcore->headerType != ZACK))
        {
            zmcore->ch = 0x11;
            sendChar(zmcore);
        }
    }
    return;
}    

static void sendBinHeader(ZMCORE *zmcore)
{
    CRCXM crc;

    crcxmInit(&crc);    
    zmcore->ch = ZPAD;
    sendChar(zmcore);
    if (ALLOK)
    {
        zmcore->ch = ZDLE;
        sendChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->frameType;
        sendChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerType;
        crcxmUpdate(&crc, zmcore->ch);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerData[0];
        crcxmUpdate(&crc, zmcore->ch);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerData[1];
        crcxmUpdate(&crc, zmcore->ch);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerData[2];
        crcxmUpdate(&crc, zmcore->ch);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerData[3];
        crcxmUpdate(&crc, zmcore->ch);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = crcxmHighbyte(&crc);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = crcxmLowbyte(&crc);
        sendDLEChar(zmcore);
    }
    return;
}    

static void sendBin32Header(ZMCORE *zmcore)
{
    CRC32 crc;

    crc32Init(&crc);    
    zmcore->ch = ZPAD;
    sendChar(zmcore);
    if (ALLOK)
    {
        zmcore->ch = ZDLE;
        sendChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->frameType;
        sendChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerType;
        crc32Update(&crc, zmcore->ch);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerData[0];
        crc32Update(&crc, zmcore->ch);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerData[1];
        crc32Update(&crc, zmcore->ch);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerData[2];
        crc32Update(&crc, zmcore->ch);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = zmcore->headerData[3];
        crc32Update(&crc, zmcore->ch);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = (unsigned char)crc32Byte4(&crc);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = (unsigned char)crc32Byte3(&crc);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = (unsigned char)crc32Byte2(&crc);
        sendDLEChar(zmcore);
    }
    if (ALLOK)
    {
        zmcore->ch = (unsigned char)crc32Byte1(&crc);
        sendDLEChar(zmcore);
    }
    return;
}    

static void sendChar(ZMCORE *zmcore)
{
    unsigned char buf[1];

    buf[0] = (unsigned char)zmcore->ch;
    extModemWriteBlock(zmcore, zmcore->zmext, buf, 1);
    return;
}

static void sendDLEChar(ZMCORE *zmcore)
{
    int tempCh;
    
    if (needsDLE(zmcore->ch))
    {
        tempCh = zmcore->ch;
        zmcore->ch = ZDLE;
        sendChar(zmcore);
        if (ALLOK)
        {
            if (tempCh == 0x7f) zmcore->ch = ZRUB0;
            else if (tempCh == 0xff) zmcore->ch = ZRUB1;
            else zmcore->ch = tempCh | 0x40;
            sendChar(zmcore);
        }
    }
    else
    {
        sendChar(zmcore);
    }
    return;
}

static void sendHexChar(ZMCORE *zmcore)
{
    int tempCh;
    
    tempCh = zmcore->ch;
    zmcore->ch = hexdigit[(tempCh >> 4) & 0x0f];
    sendChar(zmcore);
    if (ALLOK)
    {
        zmcore->ch = hexdigit[tempCh & 0x0f];
        sendChar(zmcore);
    }
    return;
}
                         
static void sendZRINIT(ZMCORE *zmcore)
{
    zmcore->frameType = ZHEX;
    zmcore->headerType = ZRINIT;
    zmcore->headerData[0] = 0x00;
    zmcore->headerData[1] = 0x00;
    zmcore->headerData[2] = 0x00;
    zmcore->headerData[3] = CANOVIO | CANFDX | CANFC32; 
    sendHeader(zmcore);
    return;
}

static void sendZSKIP(ZMCORE *zmcore)
{
    zmcore->frameType = ZHEX;
    zmcore->headerType = ZSKIP;
    zmcore->headerData[0] = 0x00;
    zmcore->headerData[1] = 0x00;
    zmcore->headerData[2] = 0x00;
    zmcore->headerData[3] = 0x00;
    sendHeader(zmcore);
    return;
}

static void sendZRPOS(ZMCORE *zmcore)
{
    long templ;
    
    zmcore->frameType = ZHEX;
    zmcore->headerType = ZRPOS;
    templ = zmcore->goodOffset;
    zmcore->headerData[0] = (unsigned char)(templ & 0xff);
    templ = templ >> 8;
    zmcore->headerData[1] = (unsigned char)(templ & 0xff);
    templ = templ >> 8;
    zmcore->headerData[2] = (unsigned char)(templ & 0xff);
    templ = templ >> 8;
    zmcore->headerData[3] = (unsigned char)(templ & 0xff);
    extModemClearInbound(zmcore, zmcore->zmext);
    sendHeader(zmcore);
    return;
}

static void sendZFIN(ZMCORE *zmcore)
{
    zmcore->frameType = ZHEX;
    zmcore->headerType = ZFIN;
    zmcore->headerData[0] = 0x00;
    zmcore->headerData[1] = 0x00;
    zmcore->headerData[2] = 0x00;
    zmcore->headerData[3] = 0x00;
    sendHeader(zmcore);
    return;
}

static void sendZEOF(ZMCORE *zmcore)
{
    zmcore->frameType = ZHEX;
    zmcore->headerType = ZEOF;
    zmcore->headerData[0] = (unsigned char)(zmcore->goodOffset & 0xffUL);
    zmcore->headerData[1] = (unsigned char)((zmcore->goodOffset >> 8) & 0xffUL);
    zmcore->headerData[2] = (unsigned char)((zmcore->goodOffset >> 16) & 0xffUL);
    zmcore->headerData[3] = (unsigned char)((zmcore->goodOffset >> 24) & 0xffUL);
    sendHeader(zmcore);
    return;
}

static void getOO(ZMCORE *zmcore)
{
    getNextCh(zmcore);
    if (ALLOK)
    {
        getNextCh(zmcore);
    }
    if (!ALLOK && errorCompare(ZMODEM_TIMEOUT))
    {
        errorClear();
    }
    return;
}

void zmcoreSend(ZMCORE *zmcore)
{
    int fin, quit, tries;

    zmcore->bufTop = zmcore->mainBuf + zmcore->bufsize;
    extModemClearInbound(zmcore, zmcore->zmext);
    sendrz(zmcore);
    fin = 0;
    tries = 0;
    while (ALLOK && !fin && (tries < 10))
    {
#ifdef DEBUG
        printf("sending ZRQINIT\n");
#endif
        sendZRQINIT(zmcore);           
        quit = 0;
#ifdef DEBUG        
        printf("getting ZMHeader\n");
#endif
        getZMHeader(zmcore);
        if (ALLOK)
        {
            if (zmcore->headerType == ZCAN)
            {
                errorSet(ZMODEM_GOTZCAN);
                quit = 1;
                fin = 1;
            }
            else if (zmcore->headerType == ZRINIT)
            {
                extModemRegisterGood(zmcore, zmcore->zmext);
                sendFiles(zmcore);
                fin = 1;
            }
            else
            {
#ifdef DEBUG
                printf("Unexpected header %d\n", zmcore->headerType);
#endif
                tries++;
            }
        }
        else
        {
            if (errorCompare(ZMODEM_TIMEOUT)
                || errorCompare(ZMODEM_CRCXM)
                || errorCompare(ZMODEM_CRC32)
                || errorCompare(ZMODEM_BADHEX))
            {
                if (tries < 10)
                {
                    errorFlush();
                    tries++;
                }
            }
        }
    }
    if (ALLOK)
    {
        quit = 0;
        tries = 0;
#ifdef DEBUG
        printf("sending ZFIN\n");
#endif
        sendZFIN(zmcore);
        while (ALLOK && !quit)
        {
            getZMHeader(zmcore);
            if (ALLOK && (zmcore->headerType == ZFIN))
            {
                quit = 1;
            }
            else
            {
                if (tries < 10)
                {
                    errorFlush();
                    sendZFIN(zmcore);
                    tries++;
                }
            }
        }
        if (ALLOK)
        {
            sendOO(zmcore);
        }
    }
    return;
}

static void sendFiles(ZMCORE *zmcore)
{
    int gotfile;
    int tries;
    int sent;
    unsigned long offset;
    
    gotfile = extFileGetFile(zmcore, 
                             zmcore->zmext, 
                             zmcore->filename, 
                             &zmcore->filesize);
    while (ALLOK && gotfile)
    {
        zmcore->goodOffset = 0;
        tries = 0;
        sent = 0;
        while (ALLOK && (tries < 10) && !sent)
        {
#ifdef DEBUG
            printf("sending ZFILE\n");
#endif
            sendZFILE(zmcore);
            sendFILEINFO(zmcore);
            getZMHeader(zmcore);
            if (ALLOK)
            {
                if (zmcore->headerType == ZRINIT)
                {
                    /* must have missed ZFILE - try again */
                    tries++;
                }
                else if (zmcore->headerType == ZRPOS)
                {
                    offset = ((unsigned long)zmcore->headerData[3] << 24)
                             | ((unsigned long)zmcore->headerData[2] << 16)
                             | ((unsigned long)zmcore->headerData[1] << 8)
                             | zmcore->headerData[0];
                    extFileSetPos(zmcore, zmcore->zmext, offset);
                    zmcore->goodOffset = offset;
                    sendFile(zmcore);
                    sent = 1;
                }
                else if (zmcore->headerType == ZSKIP)
                {
                    sent = 1;
                }
            }
            else
            {
                if (errorCompare(ZMODEM_TIMEOUT))
                {
                    errorFlush();
                }                
                tries++;
            }
        }
        if (ALLOK)
        {
            gotfile = extFileGetFile(zmcore,
                                     zmcore->zmext,
                                     zmcore->filename, 
                                     &zmcore->filesize);
        }
    }
    return;
}

#define SM_SENDZDATA 0
#define SM_SENDZEOF 1
#define SM_SENDDATA 2
#define SM_ACTIONRPOS 3
#define SM_WAITZRINIT 4

static void sendFile(ZMCORE *zmcore)
{
    int tries = 0;
    unsigned long newpos;
    int state;
    int sentfile = 0;

    zmcore->moreData = 1;
#ifdef DEBUG
    printf("in state machine\n");
#endif
    state = SM_SENDZDATA;
    while (ALLOK && !sentfile)
    {
        switch (state)
        {
        case SM_SENDZDATA:
#ifdef DEBUG
            printf("in SM_SENDZDATA\n");
#endif
            if (!extFileGetData(zmcore, 
                                zmcore->zmext,
                                zmcore->mainBuf, 
                                zmcore->maxTx, 
                                &zmcore->bytes))
            {
                state = SM_SENDZEOF;
            }
            else
            {
                sendZDATA(zmcore);
                state = SM_SENDDATA;
            }
            break;
        case SM_SENDDATA:
#ifdef DEBUG
            printf("in SM_SENDDATA\n");
#endif
            while (ALLOK && (state == SM_SENDDATA))
            {
#ifdef DEBUG
                printf("calling sendDATA\n");
#endif
                sendDATA(zmcore);
#ifdef DEBUG
                printf("immediate\n");
#endif
                getZMHeaderImmediate(zmcore);
#ifdef DEBUG
                printf("fin\n");
#endif
                if (ALLOK && (zmcore->headerType == ZRPOS))
                {
                    state = SM_ACTIONRPOS;
                }
                else
                {
                    if (errorCompare(ZMODEM_TIMEOUT))
                    {
                        errorClear();
                    }
                    if (ALLOK)
                    {
                        if (!zmcore->moreData)
                        {
                            state = SM_SENDZEOF;
                            break;
                        }
                        else 
                        {
                            extFileGetData(zmcore,
                                           zmcore->zmext,
                                           zmcore->mainBuf, 
                                           zmcore->maxTx,
                                           &zmcore->bytes);
                            if (zmcore->bytes < zmcore->maxTx)
                            {
                                zmcore->moreData = 0;
                            }
                        }
                    }
                }
            }
            break;
        case SM_ACTIONRPOS:
#ifdef DEBUG
            printf("in SM_ACTIONRPOS\n");
#endif
            newpos = zmcore->headerData[0]
                 | ((unsigned long)zmcore->headerData[1] << 8)
                 | ((unsigned long)zmcore->headerData[2] << 16)
                 | ((unsigned long)zmcore->headerData[3] << 24);
            if (newpos <= zmcore->goodOffset)
            {
                zmcore->moreData = 1;
                tries++;
            }
            else
            {
                tries = 0;
            }
            extFileSetPos(zmcore, zmcore->zmext, newpos);
            zmcore->goodOffset = newpos;
            state = SM_SENDZDATA;
            break;
        case SM_SENDZEOF:
#ifdef DEBUG
            printf("in SM_SENDZEOF\n");
#endif
            sendZEOF(zmcore);
            tries = 0;
            state = SM_WAITZRINIT;
            break;
        case SM_WAITZRINIT:
#ifdef DEBUG
            printf("in SM_WAITZRINIT\n");
#endif
            getZMHeader(zmcore);
            if (ALLOK && (zmcore->headerType == ZRINIT))
            {
                sentfile = 1;
            }
            else if (ALLOK && (zmcore->headerType == ZRPOS))
            {
                state = SM_ACTIONRPOS;
            }
            else
            {
                tries++;
                if (tries < 10)
                {
                    errorClear();
                }
            }
            break;
        }
    }
}


static void sendrz(ZMCORE *zmcore)
{
    extModemWriteBlock(zmcore, zmcore->zmext, "\x72\x7a\x0d", 3);
    return;
}

#define ZRQRINIT_STR "\x2a\x2a\x18\x42" \
                 "\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30" \
                 "\x0d\x0a\x11"
                 
static void sendZRQINIT(ZMCORE *zmcore)
{
    extModemWriteBlock(zmcore, 
                       zmcore->zmext, 
                       (unsigned char *)ZRQRINIT_STR, 
                       sizeof ZRQRINIT_STR - 1);
    return;
}

static void sendZFILE(ZMCORE *zmcore)
{
    zmcore->frameType = ZBIN;
    zmcore->headerType = ZFILE;
    zmcore->headerData[0] = 0;
    zmcore->headerData[1] = 0;
    zmcore->headerData[2] = 0;
    zmcore->headerData[3] = ZCBIN;
    sendHeader(zmcore);
    return;
}

static void sendFILEINFO(ZMCORE *zmcore)
{
    unsigned char *buf;
    int cnt = 0;
    int x;
    CRCXM crc;

    buf = zmcore->mainBuf;
    strcpy((char *)buf, (char *)zmcore->filename);
    cnt = strlen((char *)buf) + 1;
    sprintf((char *)buf + cnt, "%ld 0 0 0", zmcore->filesize); 
    cnt = cnt + strlen((char *)buf + cnt) + 1;
    crcxmInit(&crc);
    for (x = 0; x < cnt; x++)
    {
        crcxmUpdate(&crc, buf[x]);
    }
    buf[cnt++] = ZDLE;
    buf[cnt++] = ZCRCW;
    crcxmUpdate(&crc, ZCRCW);
    
    /* write out what we have so far */
    extModemWriteBlock(zmcore, zmcore->zmext, buf, cnt);
    
    /* send CRC, DLEd if required */
    zmcore->ch = crcxmHighbyte(&crc);
    sendDLEChar(zmcore);
    
    zmcore->ch = crcxmLowbyte(&crc);
    sendDLEChar(zmcore);
    
    zmcore->ch = 0x11;
    sendChar(zmcore); /* send end */
    return;
}

static void sendZDATA(ZMCORE *zmcore)
{
    zmcore->frameType = ZBIN;
    zmcore->headerType = ZDATA;
    zmcore->headerData[0] = (unsigned char)(zmcore->goodOffset & 0xffUL);
    zmcore->headerData[1] = (unsigned char)((zmcore->goodOffset >> 8) & 0xffUL);
    zmcore->headerData[2] = (unsigned char)((zmcore->goodOffset >> 16) & 0xffUL);
    zmcore->headerData[3] = (unsigned char)((zmcore->goodOffset >> 24) & 0xffUL);
    sendHeader(zmcore);
    return;
}

static void sendDATA(ZMCORE *zmcore)
{
    int cnt = 0;
    int x;
    int last;
    CRCXM crc;
    int sptype;

    cnt = zmcore->bytes;
    zmcore->mainBuf[cnt++] = ZDLE;
    /* send ZCRCE if more data, ZCRCG otherwise */
    if (zmcore->moreData)
    {
        sptype = ZCRCG;
    }
    else
    {
        sptype = ZCRCE;
    }
    zmcore->mainBuf[cnt++] = sptype;
    crcxmInit(&crc);
    last = -1;
    for (x = 0; x < zmcore->bytes; x++)
    {
        crcxmUpdate(&crc, zmcore->mainBuf[x]);
        if (needsDLE(zmcore->mainBuf[x]))
        {
            if (x > (last + 1))
            {
                extModemWriteBlock(zmcore, 
                                   zmcore->zmext,
                                   zmcore->mainBuf + last + 1,
                                   x - (last + 1));
            }
            zmcore->ch = zmcore->mainBuf[x];
            sendDLEChar(zmcore);
            last = x;
        }
    }
    if (x > (last + 1))
    {
        extModemWriteBlock(zmcore, 
                           zmcore->zmext,
                           zmcore->mainBuf + last + 1,
                           x - (last + 1));
    }
    crcxmUpdate(&crc, sptype);
    zmcore->ch = ZDLE;
    sendChar(zmcore);
    zmcore->ch = sptype;
    sendChar(zmcore);
    zmcore->mainBuf[cnt++] = (unsigned char)crcxmHighbyte(&crc);
    zmcore->mainBuf[cnt++] = (unsigned char)crcxmLowbyte(&crc);
    for (x += 2; x < cnt; x++)
    {
        zmcore->ch = *(zmcore->mainBuf + x);
        sendDLEChar(zmcore);
    }
    zmcore->goodOffset += zmcore->bytes;
    return;
}

static void sendOO(ZMCORE *zmcore)
{
    extModemWriteBlock(zmcore, zmcore->zmext, "OO", 2);
    return;
}
