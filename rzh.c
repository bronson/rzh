/* rzh.c
 * 1 Nov 2004
 * Scott Bronson
 * 
 * The main routine for the rzh utility.
 *
 * This file is released under the MIT license.  This is basically the
 * same as public domain, but absolves the author of liability.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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

int verbosity = 0;			// print notification/debug messages
int do_receive = 0;			// receive files -- don't fork subshell
int do_send = 0;			// send files -- don't fork a subshell
int quiet = 0;				// suppress status messages
double timeout = 10.0;		// timeout in seconds
const char *dldir = NULL;	// download files to this directory


#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

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


static void init_zmcore()
{
	// reset the file transfer data structures
	// since we can transfer multiple files per invocation
	bzero(&zme, sizeof(ZMEXT));
	bzero(&zmc, sizeof(ZMCORE));
	zme.pdcomm = &zpd;
	zmcoreDefaults(&zmc);
	zmcoreInit(&zmc, &zme);
}

static int chdir_to_dldir()
{
	int succ = 0;

	if(dldir) {
		succ = chdir(dldir);
		if(succ != 0) {
			fprintf(stderr, "Could not chdir to \"%s\": %s\n",
					dldir, strerror(errno));
		}
	}

	return succ;
}

void receive_file()
{
	struct timespec start;
	struct timespec end;

	init_zmcore();
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
		long tft = zmc.total_files_transferred;

		fprintf(stderr, "%ld file%s and %ld bytes transferred in %.5f secs: %.3f kbyte/sec\r\n",
				tft, (tft==1?"":"s"), tbt, total, (tbt/total/1024.0));
	}

	set_timeout(0, 0);
}


static void print_greeting()
{
	char buf[PATH_MAX];

	if(getcwd(buf, sizeof(buf))) {
		if(!quiet) {
			printf("saving zmodem files to %s\r\n", buf);
		}
	} else {
		printf("rzh is running but couldn't figure out the CWD...\r\n");
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

		bgio_stop(err ? process_error : 0);
	}

	bgio_start();
	zio_setup(bgio_master, STDOUT_FILENO, STDIN_FILENO, bgio_master, &bail);
	set_backchannel_fn(&insi_to_outma_backchannel);
	zpd.read = read_master;
	zpd.write = write_master;
	if(chdir_to_dldir() != 0) {
		bgio_stop(chdir_error);
	}

	print_greeting();
	scan_for_zrqinit(&zmc);
}


static void usage()
{
	printf(
			"Usage: rzh [OPTION]... [DLDIR]\n"
			"  -r --receive : receives a file immediately without forking a subshell.\n"
			"  -t --timeout : timeout for zmodem transfers in seconds (fractions are OK).\n"
			"  -v --verbose : increase verbosity.\n"
			"  -V --version : print the version of this program.\n"
			"  -h --help    : prints this help text\n"
			"Run rzh with no arguments to receive files into the current directory.\n"
		  );
}


static void process_args(int argc, char **argv)
{
	volatile int bk = 0;

	while(1) {
		int c;
		int optidx = 0;
		static struct option long_options[] = {
			// name, has_arg (1=reqd,2=opt), flag, val
			{"debug-attach", 0, 0, 'D'},
			{"help", 0, 0, 'h'},
			{"quiet", 0, 0, 'q'},
			{"receive", 0, 0, 'r'},
			{"send", 0, 0, 's'},
			{"timeout", 1, 0, 't'},
			{"verbose", 0, 0, 'v'},
			{"version", 0, 0, 'V'},
			{0, 0, 0, 0},
		};

		c = getopt_long(argc, argv, "Dhqrst:vV", long_options, &optidx);
		if(c == -1) break;

		switch(c) {
			case 'D':
				fprintf(stderr, "Waiting for debugger to attach to pid %d...\n", (int)getpid());
				// you must manually step the debugger past this infinite loop.
				while(!bk) { }
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

			case 's':
				do_send++;
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
				exit(argument_error);

		}
	}

	// a single argument specifies the directory we should download to
	if(do_send) {
		if(optind >= argc) {
			fprintf(stderr, "No files specified!\n");
			exit(1);
		}
	} else {
		dldir = argv[optind++];
		
		// but supplying more than one directory is an error.
		if(optind < argc) {
			fprintf(stderr, "Unrecognized argument%s: ", (optind+1==argc ? "" : "s"));
			while(optind < argc) {
				fprintf(stderr, "%s ", argv[optind++]);
			}
			fprintf(stderr, "\n");
			exit(argument_error);
		}
	}


	if(verbosity) {
		printf("Timeout is %lf seconds\n", timeout);
	}
}


int main(int argc, char **argv)
{
	// TODO: if argv[0] =~ /sz$/ then do_send = 1
	// TODO: if argv[0] =~ /rz$/ then do_receive = 1
	process_args(argc, argv);

	zpd.read = pdcommReadStdin;
	zpd.write = pdcommWriteStdout;

	if(do_send) {
		init_zmcore();
		zme.argv = &argv[optind];
		zmcoreSend(&zmc);
	} else if(do_receive) {
		if(chdir_to_dldir() != 0) {
			exit(chdir_error);
		}
		receive_file();
	} else {
		do_rzh();
	}

	zmcoreTerm(&zmc);

	return 0;
}

