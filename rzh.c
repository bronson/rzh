/* rzh.c
 * Scott Bronson
 * 
 * The main routine for the rzh utility.
 * This file is in the public domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include "zmcore.h"


static ZMCORE zmc;
static ZMEXT zme;


void process_args(int argc, char **argv)
{
	// Don't want to use getopt because it might cause licensing issues.

	while(*++argv) {
		char *cp = *argv;
		if(cp[0] == '-') {
			if(cp[1] == 'D') {
				fprintf(stderr, "Waiting for debugger to attach to pid %d...\n", (int)getpid());
				while(1)
					;
			}
		}
	}
}


int main(int argc, char **argv)
{
	process_args(argc, argv);

	// zmc and zme are initialized to all 0s
	zmcoreDefaults(&zmc);
	zmcoreInit(&zmc, &zme);

	// zmcoreSend(&zmc);
	zmcoreReceive(&zmc);

	zmcoreTerm(&zmc);

	return 0;
}

