/* bgio.c
 * Scott Bronson
 *
 * Starts up background I/O behind another process.
 *
 * NOTE: can currently only handle one ongoing bgio session because of
 * the single global used by the signal callbacks.  Too bad C doesn't
 * have closures...  Some dynamic alloc/dealloc would fix this if need be.
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

#include "bgio.h"
#include "log.h"
#include "cmd.h"
#include "util.h"


command shellcmd;


bgio_state *g_state;	// this sucks.  it's for the signals.
int bgio_child_pid;		// this also sucks.  It's for the global sigchld handler.


static void window_resize(int dummy)
{
	bgio_state *state = g_state;

	ioctl(0, TIOCGWINSZ, (char*)&state->window);
	ioctl(state->slave, TIOCSWINSZ, (char*)&state->window);
	kill(state->child_pid, SIGWINCH);
}


void bgio_stop(bgio_state *state)
{
	tcsetattr(0, TCSAFLUSH, &state->stdin_termios);
	close(state->master);
	close(state->slave);
}


static void do_child(bgio_state *state, const char *cmd)
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
	ioctl(state->slave, TIOCSCTTY, 0);

	close(state->master);
	dup2(state->slave, 0);
	dup2(state->slave, 1);
	dup2(state->slave, 2);
	close(state->slave);

	log_close();
	fdcheck();

	if(cmd) {
		execl(shell, name, "-c", cmd, NULL);
		fprintf(stderr, "Could not exec %s -c %s: %s\n",
				shell, cmd, strerror(errno));
	} else {
		execl(shell, name, "-i", NULL);
		fprintf(stderr, "Could not exec %s -i: %s\n",
				shell, strerror(errno));
	}

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

void bgio_start(bgio_state *state, const char *cmd)
{
	struct termios tt;

	// this is why we can only handle one session at once.
	g_state = state;

	tcgetattr(0, &state->stdin_termios);
	ioctl(0, TIOCGWINSZ, (char *)&state->window);
	if (openpty(&state->master, &state->slave, NULL,
				&state->stdin_termios, &state->window) < 0) {
		perror("calling openpty");
		kill(0, SIGTERM);
		exit(fork_error1);
	}

	log_dbg("bgio: master=%d slave=%d", state->master, state->slave);

	tt = state->stdin_termios;
	cfmakeraw(&tt);
	tt.c_lflag &= ~ECHO;
	tcsetattr(0, TCSAFLUSH, &tt);

	state->child_pid = fork();
	if(state->child_pid < 0) {
		perror("forking child");
		kill(0, SIGTERM);
		bail(fork_error2);
	}

	if(state->child_pid == 0) {
		do_child(state, cmd);
		perror("executing child");
		kill(0, SIGTERM);
		bail(fork_error3);
	}

	bgio_child_pid = state->child_pid;
	signal(SIGWINCH, window_resize);
}

