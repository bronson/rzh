/* rzh.c
 * Scott Bronson
 * 
 * The main routine for the rzh utility.
 * This file is in the public domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <setjmp.h>

#include "bgio.h"
#include "zmcore.h"
#include "scan.h"
#include "zio.h"

static PDCOMM zpd;
static ZMCORE zmc;
static ZMEXT zme;

int do_receive = 0;


#define xstringify(x) #x
#define stringify(x) xstringify(x)


/**
 * The main channel is reading from the master and writing to
 * stdout.  The backchannel is reading from stdin and writing
 * to the master.  This callback implements the backchannel.
 *
 * inma -> outso   (inma: in from master, outso: out to stdout)
 * insi -> outma   (insi: in from stdin, outma: out to master)
 */

static void insi_to_outma_backchannel(void)
{
	/* first check for errors
	 */

	if(insi.error) {
		zio_bail(&insi, "backchannel read");
	}

	if(outma.error) {
		zio_bail(&insi, "backchannel write");
	}

	/* copy stdin to the master.  the fifos themselves are automatically
	 * read and written on each tick.  We just need to ensure that the data
	 * moves between the fifos.
	 */

	fifo_copy(&insi, &outma);
}


static void print_error(fifo *f)
{
	if(f->error) {
		fprintf(stderr, "Error %d on %s: %s\n", f->error,
				f->name, strerror(f->error));
	}
}


static void do_rzh()
{
	int err;
	jmp_buf bail;

	if((err=setjmp(bail)) != 0) {
		if(err == 2) {
			/* apparently we bailed due to an error.  Try to find
			 * and print it */
			print_error(&inma);
			if(outso.buf) print_error(&outso);
			if(insi.buf) print_error(&insi);
			print_error(&outma);
		}
		bgio_stop();
	}


	bgio_start();
	zio_setup(bgio_master, STDOUT_FILENO, STDIN_FILENO, bgio_master, &bail);
	set_backchannel_fn(&insi_to_outma_backchannel);
	zpd.read = read_master;
	zpd.write = write_master;
	scan_for_zrqinit(&zmc);
}


static void process_args(int argc, char **argv)
{
	// Don't want to use getopt because it might cause licensing issues
	// and we don't really need it anyway.

	while(*++argv) {
		char *cp = *argv;
		if(cp[0] == '-') {
			switch(cp[1]) {
			case 'D':
				fprintf(stderr, "Waiting for debugger to attach to pid %d...\n", (int)getpid());
				while(1)
					;
				break;

			case 'V':
				printf("rzh version %s\n", stringify(VERSION));
				exit(0);

			case 'r':
				do_receive = 1;
				break;

			default:
				fprintf(stderr, "Unknown option passed to rzh: %c\n", cp[1]);
				exit(1);
			}
		} else {
			fprintf(stderr, "Unknown argument passed to rzh: %s\n", cp);
			exit(1);
		}
	}
}


int main(int argc, char **argv)
{
	process_args(argc, argv);

	zpd.read = pdcommReadStdin;
	zpd.write = pdcommWriteStdout;
	zme.pdcomm = &zpd;
	zmcoreDefaults(&zmc);
	zmcoreInit(&zmc, &zme);

	// TODO: if argv[0] =~ /sz$/ then do_send = 1
	// zmcoreSend(&zmc);
	// TODO: if argv[0] =~ /rz$/ then do_receive = 1
	if(do_receive) {
		zmcoreReceive(&zmc);
	} else {
		do_rzh();
	}

	zmcoreTerm(&zmc);

	return 0;
}

