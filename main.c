#include <stdio.h>
#include "zmcore.h"


static ZMCORE zmc;
static ZMEXT zme;

int main(int argc, char **argv)
{
	// zmc and zme are initialized to all 0s
	zmcoreDefaults(&zmc);
	zmcoreInit(&zmc, &zme);

	// zmcoreSend(&zmc);
	zmcoreReceive(&zmc);

	zmcoreTerm(&zmc);

	return 0;
}

