#include <stdio.h>

#include "zmodem.h"
#include "zmcore.h"
#include "zmext.h"
#include "pdcomm.h"

void zmodemDefaults(ZMODEM *zmodem)
{
    pdcommDefaults(&zmodem->pdcomm);
    zmcoreDefaults(&zmodem->zmcore);
    zmextDefaults(&zmodem->zmext);
    return;
}

void zmodemInit(ZMODEM *zmodem)
{
    pdcommInit(&zmodem->pdcomm, 
#ifdef MSDOS
    /* io port and irq! */
    "COM1:03F8,4,19200"
    /*"COM2:02F8,3,19200"*/
#else
    "COM1:19200"
#endif        
    );
    zmextInit(&zmodem->zmext, &zmodem->pdcomm);
    zmcoreInit(&zmodem->zmcore, &zmodem->zmext);
    return;
}

void zmodemTerm(ZMODEM *zmodem)
{
    zmcoreTerm(&zmodem->zmcore);
    pdcommTerm(&zmodem->pdcomm);
    return;
}

void zmodemSend(ZMODEM *zmodem, char *filespec)
{
    zmextFileSetSpec(&zmodem->zmext, filespec);
    zmcoreSend(&zmodem->zmcore);
    return;
}

void zmodemReceive(ZMODEM *zmodem)
{
    zmcoreReceive(&zmodem->zmcore);
    return;
}
