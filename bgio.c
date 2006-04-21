/* bgio.c
 * Scott Bronson
 *
 * Starts up background I/O behind another process.
 */

#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>

#include "bgio.h"
#include "log.h"
#include "cmd.h"
#include "util.h"



static int st_master_fd;
static int st_slave_fd;
int st_child_pid;
static struct termios st_stdin_ios;	// restore when finished
static struct winsize st_window;			// current winsize of terminal



static void window_resize(int dummy)
{
	ioctl(0, TIOCGWINSZ, (char*)&st_window);
	ioctl(st_slave_fd, TIOCSWINSZ, (char*)&st_window);
	kill(st_child_pid, SIGWINCH);
}


void bgio_stop()
{
	tcsetattr(0, TCSAFLUSH, &st_stdin_ios);
	log_info("Closed FD %d (master)", st_master_fd);
	close(st_master_fd);
	log_info("Closed FD %d (slave)", st_slave_fd);
	close(st_slave_fd);
}


void bgio_close()
{
	close(st_master_fd);
	close(st_slave_fd);
}


int bgio_get_window_width()
{
	// This routine may be called even if we're using socketio
	// instead of bgio.  Ensure we still return decent data.
	return st_window.ws_col ? st_window.ws_col : 80;
}


static void do_child()
{
	char *shell;
	char *name;

	shell = getenv("SHELL");
	if(!shell) {
#ifdef _PATH_BSHELL
		shell = _PATH_BSHELL;
#else
		shell = "/bin/sh";
#endif
	}

	name = strrchr(shell, '/');
	if(name) {
		name++;
	} else {
		name = shell;
	}

	setsid();
	ioctl(st_slave_fd, TIOCSCTTY, 0);

	close(st_master_fd);
	dup2(st_slave_fd, 0);
	dup2(st_slave_fd, 1);
	dup2(st_slave_fd, 2);
	close(st_slave_fd);

	log_close();
	fdcheck();

	// There's probably no need for -i since input and output are
	// to ttys anway.  However, since all shells support it, why not?
	execl(shell, name, "-i", NULL);
	fprintf(stderr, "Could not exec %s -i: %s\n",
			shell, strerror(errno));

	exit(86);
}

/**
 * http://www.dusek.ch/manual/glibc/libc_17.html:
 * Data written to the master side is received by the slave side as
 * if it was the result of a user typing at an ordinary terminal,
 * and data written to the slave side is sent to the master side as
 * if it was written on an ordinary terminal.
 *
 * @param state: uninitialized memory to store the state for this conn.
 * @param cmd: Command to run in child.  If NULL, starts the default shell.
 *
 * @returns: Nothing!  It prints a message to stdout and exits on failure.
 * This should really be changed one day.  (it's been mostly changed...
 * it only exits if it couldn't allocate a pty.  else it exits through bail).
 */

int bgio_start()
{
	struct termios tt;

	tcgetattr(0, &st_stdin_ios);
	ioctl(0, TIOCGWINSZ, (char *)&st_window);
	if (openpty(&st_master_fd, &st_slave_fd, NULL,
				&st_stdin_ios, &st_window) < 0) {
		perror("calling openpty");
		kill(0, SIGTERM);
		exit(fork_error1);
	}

	log_dbg("bgio: master=%d slave=%d", st_master_fd, st_slave_fd);

	tt = st_stdin_ios;
	cfmakeraw(&tt);
	tt.c_lflag &= ~ECHO;
	tcsetattr(0, TCSAFLUSH, &tt);

	st_child_pid = fork();
	if(st_child_pid < 0) {
		perror("forking child");
		kill(0, SIGTERM);
		bail(fork_error2);
	}

	if(st_child_pid == 0) {
		do_child();
		perror("executing child");
		kill(0, SIGTERM);
		bail(fork_error3);
	}

	signal(SIGWINCH, window_resize);

	return st_master_fd;
}

