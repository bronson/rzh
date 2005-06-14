/* rzh.c
 * 1 Nov 2004
 * Scott Bronson
 * 
 * The main routine for the rzh utility.
 *
 * This file is released under the MIT license.  This is basically the
 * same as public domain but absolves the author of liability.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <setjmp.h>
#include <unistd.h>
#include <getopt.h>

#include "bgio.h"
#include "echo.h"
#include "io/io.h"
#include "log.h"


static int verbosity = 0;			// print notification/debug messages
static int quiet = 0;				// suppress status messages
static const char *dldir = NULL;	// download files to this directory

static jmp_buf g_bail;


#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

#define xstringify(x) #x
#define stringify(x) xstringify(x)


void bail(int val)
{
	longjmp(g_bail, val);
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


static void usage()
{
	printf(
			"Usage: rzh [OPTION]... [DLDIR]\n"
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

	dldir = argv[optind++];
	
	// supplying more than one directory is an error.
	if(optind < argc) {
		fprintf(stderr, "Unrecognized argument%s: ", (optind+1==argc ? "" : "s"));
		while(optind < argc) {
			fprintf(stderr, "%s ", argv[optind++]);
		}
		fprintf(stderr, "\n");
		exit(argument_error);
	}
}


void run(bgio_state *bgio)
{
	if(chdir_to_dldir() != 0) {
		bail(25);
	}

	print_greeting();
	echo(bgio);
}


int main(int argc, char **argv)
{
	int val;
	bgio_state bgio;

	process_args(argc, argv);

	io_init();
	log_init("/tmp/rzh_log");

	// after this call, everything must exit past bgio_stop
	bgio_start(&bgio, NULL);

	val = setjmp(g_bail);
	if(val == 0) {
		// perform operations
		run(&bgio);
	}

	bgio_stop(&bgio);
	io_exit();
	exit(val);
}

