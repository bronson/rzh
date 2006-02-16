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
#include <ctype.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <setjmp.h>
#include <unistd.h>
#include <getopt.h>
#include <netdb.h>

#include "log.h"
#include "fifo.h"
#include "io/io_socket.h"
#include "pipe.h"
#include "task.h"
#include "cmd.h"
#include "echo.h"
#include "master.h"
#include "util.h"


static int opt_verbosity = 0;			// print notification/debug messages
int opt_quiet = 0;					// suppress status messages
const char *download_dir = NULL;	// download files to this directory
static jmp_buf g_bail;
socket_addr conn_addr;
int conn_fd = -1;


#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif

// the command that gets run when we try to receive a file
// (override this using the --rz command-line option)
#define DEFAULT_RZ_COMMAND "/usr/bin/rz"

#define xstringify(x) #x
#define stringify(x) xstringify(x)


void bail(int val)
{
	longjmp(g_bail, val);
}


// Aborts the initialization process if we notice an error.
// Once bgio is started, do not call this routine!  Call bail() instead.

void preabort(int val)
{
	fprintf(stderr, "rzh was not started.\n");
	longjmp(g_bail, val);
}


void rzh_fork_prepare()
{
	if(conn_fd > -1) {
		close(conn_fd);
	}

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
		printf("Saving to %s on %s.\r\n", buf, hostname);
	} else {
		// Some sort of error but not worth stopping the program.
		printf("rzh is running but couldn't figure out the CWD!\r\n");
	}
}


// Returns 1 if we have the specified permission, 0 if not.
// Op can be 0444 for read, 0222 for write, and 0111 for execute.

#define CAN_READ  0444
#define CAN_WRITE 0222
#define CAN_EXECUTE  0111

static int i_have_permission(const struct stat *st, int op)
{
	if(st->st_mode & S_IRWXU & op) {
		if(geteuid() == st->st_uid) {
			return 1;
		}
	}
	
	if(st->st_mode & S_IRWXG & op) {
		if(getegid() == st->st_gid) {
			return 1;
		}
	}

	if(st->st_mode & S_IRWXO & op) {
		return 1;
	}

	return 0;
}


// Run some checks to ensure we're running in a sane environment.
// NOTE that this routine also sets an environment variable so it
// does a bit more than JUST run checks...

static void preflight()
{
	static const char *envname = "RZHDIR";
	struct stat st;

	char buf[PATH_MAX];
	char var[PATH_MAX];
	char *s;

	if(!opt_quiet) {
		s = getenv(envname);
		if(s) {
			fprintf(stderr, "Warning: we're overriding another rzh process which is downloading to %s\n", s);
		}
	}

	if(!getcwd(buf, sizeof(buf))) {
		fprintf(stderr, "couldn't figure out the CWD!\n");
		preabort(24);
	}

	if(download_dir) {
		if(chdir(download_dir) != 0) {
			fprintf(stderr, "couldn't cd to %s: %s\n",
					download_dir, strerror(errno));
			preabort(25);
		}
	}

	if(!getcwd(var, sizeof(var))) {
		fprintf(stderr, "couldn't figure out the dldir!\n");
		preabort(26);
	}

	if(setenv(envname, var, 1) != -0) {
		fprintf(stderr, "setenv %s to %s failed!\n", envname, var);
		preabort(39);
	}

	// check the destination directory
	if(stat(var, &st) != 0) {
		fprintf(stderr, "Could not stat %s: %s\n", var, strerror(errno));
		preabort(49);
	}
	if(!(st.st_mode & S_IFDIR)) {
		fprintf(stderr, "Warning: destination %s is not a directory!\n", var);
	}
	if(!i_have_permission(&st, CAN_WRITE)) {
		fprintf(stderr, "Warning: can't write to %s!\n", var);
	}

	// check the rz executable
	if(stat(rzcmd.path, &st) != 0) {
		fprintf(stderr, "Could not stat receive program \"%s\": %s\n", rzcmd.path, strerror(errno));
		preabort(70);
	}
	if(!S_ISREG(st.st_mode)) {
		fprintf(stderr, "Error: receive program \"%s\" is not a regular file!\n", rzcmd.path);
		preabort(71);
	}
	if(!i_have_permission(&st, CAN_READ)) {
		fprintf(stderr, "Error: can't read receive program \"%s\"!\n", rzcmd.path);
		preabort(72);
	}
	if(!i_have_permission(&st, CAN_EXECUTE)) {
		fprintf(stderr, "Error: can't execute receive program \"%s\"!\n", rzcmd.path);
		preabort(73);
	}

	if(!opt_quiet) {
		print_greeting();
	}

	if(download_dir) {
		if(chdir(buf) != 0) {
			fprintf(stderr, "couldn't return to %s: %s\n",
					buf, strerror(errno));
			preabort(27);
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
	char *fmt;
	int addr_specified = 0;

	enum {
		LOG_LEVEL = CHAR_MAX + 1,
		RZ_CMD,
		SHELL_CMD,
		CONNECT_CMD,
		INMA_FIFO_SIZE,
		MAOU_FIFO_SIZE,
	};

	while(1) {
		int c, i;
		int optidx = 0;
		static struct option long_options[] = {
			// name, has_arg (1=reqd,2=opt), flag, val
			{"connect", 1, 0, CONNECT_CMD},
			{"debug-attach", 0, 0, 'D'},
			{"fifo-inma", 1, 0, INMA_FIFO_SIZE},
			{"fifo-maout", 1, 0, MAOU_FIFO_SIZE},
			{"help", 0, 0, 'h'},
			{"loglevel", 1, 0, LOG_LEVEL},
			{"log-level", 1, 0, LOG_LEVEL},
			{"quiet", 0, 0, 'q'},
			{"rz", 1, 0, RZ_CMD},
			{"shell", 1, 0, SHELL_CMD},
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

			// options taking integer arguments
			case LOG_LEVEL:
			case INMA_FIFO_SIZE:
			case MAOU_FIFO_SIZE:
				if(!io_safe_atoi(optarg, &i)) {
					fprintf(stderr, "Invalid number: \"%s\"\n", optarg);
					exit(argument_error);
				}

				switch(c) {
					case LOG_LEVEL:
						if(i>0 && !log_get_priority()) {
							log_init("/tmp/rzh.log");
						}
						log_set_priority(i);
						if(!opt_quiet) {
							fprintf(stderr, "log level set to %d\n", i);
						}
						break;

					case INMA_FIFO_SIZE:
					case MAOU_FIFO_SIZE:
						if(i < 0 || i > 1024*1024) {
							fprintf(stderr, "Value out of range: %d\n", i);
						}
						if(c == INMA_FIFO_SIZE) {
							inma_fifo_size = i;	
						} else if(c == MAOU_FIFO_SIZE) {
							maou_fifo_size = i;
						} else {
							assert(!"No handler for option");
						}
						break;

					default:
						assert(!"No handler for option");

				}
				break;
			
			case 'q':
				opt_quiet++;
				break;

			case RZ_CMD:
				cmd_parse(&rzcmd, optarg);
				break;

			case SHELL_CMD:
				cmd_parse(&shellcmd, optarg);
				break;

			case CONNECT_CMD:
				addr_specified++;
				fmt = io_socket_parse(optarg, &conn_addr);
				if(fmt != 0) {
					fprintf(stderr, fmt, optarg);
					exit(argument_error);
				}
				break;

			case 'v':
				opt_verbosity++;
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
		fprintf(stderr, "Too many arguments: ");
		while(optind < argc) {
			fprintf(stderr, "%s ", argv[optind++]);
		}
		fprintf(stderr, "\n");
		exit(argument_error);
	}

	if(opt_verbosity) {
		cmd_print(&rzcmd);
	}

	if(addr_specified && conn_addr.port == 0) {
		fprintf(stderr, "You must specify a port to connect to!\n");
		exit(argument_error);
	}
}


int main(int argc, char **argv)
{
	int val;
	master_pipe *mp;

	// helps verify we're not leaking filehandles to the kid.
	// (this would be a security risk if any of the filehandles
	// were to files/devices with sensitive data)
	g_highest_fd = find_highest_fd();

	log_set_priority(0);

	// We do not ensure that io_exit is called after forking but
	// before execing.  For select this is OK.  If we move to a
	// fd-based select scheme, though, it may be an issue.
	io_init();

	cmd_init(&rzcmd);
	cmd_init(&shellcmd);
	conn_addr.addr.s_addr = inet_addr("127.0.0.1");
	conn_addr.port = 0;

	process_args(argc, argv);

	if(rzcmd.path == NULL) {
		// if user didn't specify the rzcmd to use, load default
		cmd_parse(&rzcmd, DEFAULT_RZ_COMMAND);
	}

	if(conn_addr.port > 0) {
		// user wants to connect to a socket instead of a tty (for testing).
		conn_fd = io_socket_connect_fd(conn_addr);
		if(conn_fd < 0) {
			fprintf(stderr, "Could not connect to %s:%d: %s\n",
				inet_ntoa(conn_addr.addr), conn_addr.port, strerror(errno));
			exit(runtime_error);
		}
		log_info("FD test socket: %d", conn_fd);
	}

	val = setjmp(g_bail);
	if(val == 0) {
		preflight();
		mp = master_setup(conn_fd);
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

	if(conn_fd >= 0) {
		close(conn_fd);
	}

	cmd_free(&rzcmd);
	cmd_free(&shellcmd);

	exit(val);
}

