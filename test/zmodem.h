#include "zmcore.h"
#include "zmext.h"
#include "pdcomm.h"

typedef struct {
    ZMCORE zmcore;
    ZMEXT zmext;
    PDCOMM pdcomm;
} ZMODEM;

void zmodemDefaults(ZMODEM *zmodem);
void zmodemInit(ZMODEM *zmodem);
void zmodemTerm(ZMODEM *zmodem);
void zmodemSend(ZMODEM *zmodem, char *filespec);
void zmodemReceive(ZMODEM *zmodem);
