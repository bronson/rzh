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
#include <time.h>
#include <unistd.h>
#include <setjmp.h>
#include <getopt.h>

#include "bgio.h"
#include "zmcore.h"
#include "scan.h"
#include "zio.h"

static PDCOMM zpd;
static ZMCORE zmc;
static ZMEXT zme;

int verbosity = 0;
int do_receive = 0;
int quiet = 0;
double timeout = 10.0;


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


static void init_receive()
{
	// reset the file transfer data structures
	// since we can transfer multiple files per invocation
	bzero(&zme, sizeof(ZMEXT));
	bzero(&zmc, sizeof(ZMCORE));
	zme.pdcomm = &zpd;
	zmcoreDefaults(&zmc);
	zmcoreInit(&zmc, &zme);
}

void receive_file()
{
	struct timespec start;
	struct timespec end;

	init_receive();
	set_timeout((int)timeout, (int)(1E6*(timeout-(int)timeout)));

	clock_gettime(CLOCK_REALTIME, &start);
	zmcoreReceive(&zmc);
	clock_gettime(CLOCK_REALTIME, &end);

	if(!quiet) {
		double ds, de, total;
		ds = start.tv_sec + ((double)start.tv_nsec)/1000000000;
		de = end.tv_sec + ((double)end.tv_nsec)/1000000000;
		total = de - ds;
		long tbt = zmc.total_bytes_transferred;

		fprintf(stderr, "%ld files and %ld bytes transferred in %.5f secs: %.3f kbyte/sec\r\n",
				zmc.total_files_transferred, tbt, total, (tbt/total/1024.0));
	}

	set_timeout(0, 0);
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


static void usage()
{
	printf(
			"Usage: rzh [OPTION]...\n"
			"  -r --receive : receives a file immediately without forking a subshell.\n"
			"  -t --timeout : timeout for zmodem transfers in seconds (fractions are OK).\n"
			"  -v --verbose : increase verbosity.\n"
			"  -V --version : print the version of rzh.\n"
			"  -h --help    : prints this help text\n"
			"Run rzh with no arguments to receive files into the current directory.\n"
		  );
}


static void process_args(int argc, char **argv)
{
	while(1) {
		int c;
		int optidx = 0;
		static struct option long_options[] = {
			// name, has_arg (1=reqd,2=opt), flag, val
			{"debug-attach", 0, 0, 'D'},
			{"help", 0, 0, 'h'},
			{"quiet", 0, 0, 'q'},
			{"receive", 0, 0, 'r'},
			{"timeout", 1, 0, 't'},
			{"verbose", 0, 0, 'v'},
			{"version", 0, 0, 'V'},
			{0, 0, 0, 0},
		};

		c = getopt_long(argc, argv, "Dhqrt:vV", long_options, &optidx);
		if(c == -1) break;

		switch(c) {
			case 'D':
				fprintf(stderr, "Waiting for debugger to attach to pid %d...\n", (int)getpid());
				// you must manually step the debugger past this infinite loop.
				while(1)
					;
				break;

			case 'h':
				usage();
				exit(0);

			case 'q':
				quiet++;
				break;

			case 'r':
				do_receive++;
				break;

			case 't':
				sscanf(optarg, "%lf", &timeout);
				break;

			case 'v':
				verbosity++;
				break;

			case 'V':
				printf("rzh version %s\n", stringify(VERSION));
				exit(0);

			case 0:
			case '?':
				break;

			default:
				exit(1);

		}
	}

	if(optind < argc) {
		fprintf(stderr, "Unrecognized arguments: ");
		while(optind < argc) {
			fprintf(stderr, "%s ", argv[optind++]);
		}
		fprintf(stderr, "\n");
		exit(1);
	}

	if(verbosity) {
		printf("Timeout is %lf seconds\n", timeout);
	}
}


int main(int argc, char **argv)
{
	process_args(argc, argv);

	zpd.read = pdcommReadStdin;
	zpd.write = pdcommWriteStdout;

	// TODO: if argv[0] =~ /sz$/ then do_send = 1
	// zmcoreSend(&zmc);
	// TODO: if argv[0] =~ /rz$/ then do_receive = 1
	if(do_receive) {
		receive_file();
	} else {
		do_rzh();
	}

	zmcoreTerm(&zmc);

	return 0;
}

