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
#include <netdb.h>

#include "log.h"
#include "fifo.h"
#include "io/io.h"
#include "pipe.h"
#include "task.h"
#include "echo.h"
#include "master.h"
#include "util.h"


static int verbosity = 0;			// print notification/debug messages
int opt_quiet = 0;					// suppress status messages
const char *download_dir = NULL;	// download files to this directory

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


void rzh_fork_prepare()
{
	log_close();
	io_exit();
	fdcheck();
}


static void print_greeting()
{
	int err;
	struct hostent *hp;
	char buf[PATH_MAX];
	char *hostname = "UNKNWOWN";

	// Try to discover what machine we're running on
	err = gethostname(buf, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';	// stupid clib
	if(err == 0) {
		hp = gethostbyname(buf);
		if(hp != NULL) {
			hostname = hp->h_name;
		}
	}

	if(getcwd(buf, sizeof(buf))) {
		printf("Saving to %s on %s\r\n", buf, hostname);
	} else {
		// Some sort of error but not worth stopping the program.
		printf("rzh is running but couldn't figure out the CWD!\r\n");
	}
}


static void preflight()
{
	static const char *envname = "RZHDIR";

	char buf[PATH_MAX];
	char var[PATH_MAX];
	char *s;

	if(!opt_quiet) {
		s = getenv(envname);
		if(s) {
			fprintf(stderr, "Another rzh process is downloading to %s\n", s);
		}
	}

	if(!getcwd(buf, sizeof(buf))) {
		fprintf(stderr, "couldn't figure out the CWD!\n");
		bail(24);
	}

	if(download_dir) {
		if(chdir(download_dir) != 0) {
			fprintf(stderr, "couldn't cd to %s: %s\n",
					download_dir, strerror(errno));
			bail(25);
		}
	}

	if(!getcwd(var, sizeof(var))) {
		fprintf(stderr, "couldn't figure out the dldir!\n");
		bail(26);
	}

	if(setenv(envname, var, 1) != -0) {
		fprintf(stderr, "setenv %s to %s failed!\n", envname, var);
		bail(39);
	}

	if(!opt_quiet) {
		print_greeting();
	}

	if(download_dir) {
		if(chdir(buf) != 0) {
			fprintf(stderr, "couldn't return to %s: %s\n",
					buf, strerror(errno));
			bail(27);
		}
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

	enum {
		LOG_LEVEL = CHAR_MAX + 1,
	};

	while(1) {
		int c, i;
		int optidx = 0;
		static struct option long_options[] = {
			// name, has_arg (1=reqd,2=opt), flag, val
			{"debug-attach", 0, 0, 'D'},
			{"help", 0, 0, 'h'},
			{"loglevel", 1, 0, LOG_LEVEL},
			{"log-level", 1, 0, LOG_LEVEL},
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

			case LOG_LEVEL:
				i = atoi(optarg);
				if(i>0 && !log_get_priority()) {
					log_init("/tmp/rzh.log");
				}
				log_set_priority(i);
				fprintf(stderr, "log level set to %d\n", i);
				break;

			case 'q':
				opt_quiet++;
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

	download_dir = argv[optind++];
	
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


int main(int argc, char **argv)
{
	int val;
	master_pipe *mp;

	// helps verify we're not leaking filehandles to the kid.
	g_highest_fd = find_highest_fd();

	log_set_priority(0);

	// We do not ensure that io_exit is called after forking but
	// before execing.  For select this is OK.  If we move to a
	// fd-based select scheme, though, it may be an issue.
	io_init();

	process_args(argc, argv);

	val = setjmp(g_bail);
	if(val == 0) {
		preflight();
		mp = master_setup();
		task_install(mp, echo_scanner_create_spec(mp));
		for(;;) {
			// main loop, only ends through longjmp
			io_wait(master_idle(mp));
			io_dispatch();
			// Turns out we need to dispatch before handling sigchlds.
			// Otherwise, since the sigchld probably causes fds to open
			// and close, we end up dispatching on stale events.  Bad.
			master_check_sigchild(mp);
		}
	}
	if(val == 1) {
		// Setjmp converts a zero return value into a 1.  Therefore,
		// we'll treat a 0 or a 1 return from setjmp as no error.
		val = 0;
	}

	if(val == 0) {
		// We're not forking, we're qutting normally.  The requirements are
		// exactly the same: verify everything has been shut down properly.
		rzh_fork_prepare();
		io_exit_check();
	}

	exit(val);
}

