/* child.c
 * 13 June 2005
 * Scott Bronson
 *
 * Starts up the zmodem child process and splices it into the data stream.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "child.h"
#include "fifo.h"
#include "io/io.h"
#include "log.h"
#include "pipe.h"


static sig_t save_sigchild;
static sig_t save_sigpipe;
static struct pipe *save_master_read_pipe;
static struct pipe *save_master_write_pipe;

static pipe_atom a_chin;
static pipe_atom a_chout;
static pipe_atom a_master;	// TODO this must be changed
static struct pipe f_mc; // master in to child out
static struct pipe f_cm; // master out to child in


void sigchild(int sig)
{
}


void sigpipe(int sig)
{
}


static void splice(int chstdin, int chstdout)
{
	pipe_atom_init(&a_chin, chstdin);
	pipe_atom_init(&a_chout, chstdout);

	save_sigchild = signal(SIGCHLD, sigchild);
	save_sigpipe = signal(SIGPIPE, sigpipe);
	save_master_read_pipe = a_master.read_pipe;
	save_master_write_pipe = a_master.write_pipe;

	pipe_init(&f_mc, &a_master, &a_chin);
	pipe_init(&f_cm, &a_chout, &a_master);
}


/** Forks the zmodem receive process and splices it into the data stream
 */

void start_proc(const char *buf, int size, void *refcon)
{
//	struct pipe *af = (struct pipe*)refcon;

	int chstdin[2];
	int chstdout[2];
	int chstderr[2];
	int pid;

	fprintf(stderr, "STARTING\n\n");
	bail(83);

	if(pipe(chstdin) < 0) {
		perror("creating output pipes");
		bail(77);
	}
	if(pipe(chstdout) < 0) {
		perror("creating input pipes");
		bail(78);
	}
	if(pipe(chstderr) < 0) {
		perror("creating input pipes");
		bail(78);
	}

	pid = fork();
	if(pid == 0) {
		close(chstdin[1]);
		close(chstdout[0]);
		close(chstderr[0]);
	
		dup2(chstdin[0], 0);
		dup2(chstdout[1], 1);
		dup2(chstderr[1], 2);

		close(chstdin[0]);
		close(chstdout[1]);
		close(chstderr[1]);

		execl("/usr/bin/rz", "rz");
		fprintf(stderr, "Could not exec /usr/bin/rz: %s\n",
				strerror(errno));
		exit(89);
	}

	close(chstdin[0]);
	close(chstdout[1]);
	close(chstderr[1]);

	splice(chstdin[1], chstdout[0]);

//	pipe_atom_init(&a_cherr, chstderr[0]);

	/*
	a_stdin.rfifo = cmdfifo;
	a_stdout.wfifo = progress_output;
	stderr goes straight to the debug log
	*/
}


