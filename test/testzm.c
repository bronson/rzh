#include <stdio.h>

#include "zmodem.h"

int main(void)
{
    static ZMODEM zmodem;
    
    zmodemDefaults(&zmodem);
    zmodemInit(&zmodem);
    zmodemSend(&zmodem, "*.txt");
    zmodemReceive(&zmodem);
    zmodemTerm(&zmodem);
    return (0);
}
