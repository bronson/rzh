/* bgio.c
 * Scott Bronson
 *
 * Starts up background I/O.  This file was derived from ancient BSD code.
 * It should probably fall under some sort of BSD license.
 */

#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <utmp.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "bgio.h"


char *bgio_subshell_command=NULL;
int bgio_master;
int bgio_slave;

static int child_pid;
static struct termios stdin_termios;



static void window_resize(int dummy)
{
	struct  winsize win;

	ioctl(0, TIOCGWINSZ, (char*)&win);
	ioctl(bgio_slave, TIOCSWINSZ, (char*)&win);

	kill(child_pid, SIGWINCH);
}


void bgio_stop(int code)
{
	tcsetattr(0, TCSAFLUSH, &stdin_termios);
	close(bgio_master);
	fprintf(stderr, "\r\nrzh exited.\r\n");
	exit(code);
}


static void sigchild(int dummy)
{
	int pid;

	while ((pid = wait3(&dummy, 0, 0)) > 0) {
		if (pid == child_pid) {
			bgio_stop(0);
		}
	}
}


static void do_child()
{
	char *shell;
	char *name;

	shell = getenv("SHELL");
	if(!shell) {
		shell = _PATH_BSHELL;
	}

	name = strrchr(shell, '/');
	if(name) {
		name++;
	} else {
		name = shell;
	}

	setsid();
	ioctl(bgio_slave, TIOCSCTTY, 0);

	close(bgio_master);
	dup2(bgio_slave, 0);
	dup2(bgio_slave, 1);
	dup2(bgio_slave, 2);
	close(bgio_slave);

	if(bgio_subshell_command) {
		execl(shell, name, "-c", bgio_subshell_command, 0);
	} else {
		execl(shell, name, "-i", 0);
	}
}


void bgio_start()
{
	struct  winsize win;
	struct termios tt;

	tcgetattr(0, &stdin_termios);
	ioctl(0, TIOCGWINSZ, (char *)&win);
	if (openpty(&bgio_master, &bgio_slave, NULL, &stdin_termios, &win) < 0) {
		perror("calling openpty");
		kill(0, SIGTERM);
		bgio_stop(fork_error1);
	}

	tt = stdin_termios;
	cfmakeraw(&tt);
	tt.c_lflag &= ~ECHO;
	tcsetattr(0, TCSAFLUSH, &tt);

	signal(SIGCHLD, sigchild);

	child_pid = fork();
	if(child_pid < 0) {
		perror("forking child");
		kill(0, SIGTERM);
		bgio_stop(fork_error2);
	}

	if(child_pid == 0) {
		do_child();
		perror("executing child");
		kill(0, SIGTERM);
		bgio_stop(fork_error3);
	}

	signal(SIGWINCH, window_resize);
}

