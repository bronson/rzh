/* master.c
 * 15 June 2005
 *
 * Sets up the bgio master pipe and prepares it to accept tasks.
 * Automatically installs the echo task as its first task.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <values.h>
#include <pty.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "log.h"
#include "bgio.h"
#include "fifo.h"
#include "io/io.h"
#include "pipe.h"
#include "task.h"
#include "master.h"
#include "util.h"


static int sigchild_received;


static void sigchild(int tt)
{
	// Don't want to handle the child here as it could lead to races.
	// The signal causes select to return early so we'll handle it
	// before doing any I/O.
	log_dbg("Got a sigchild!  sigchild_received=%d->%d", sigchild_received, sigchild_received+1);
	sigchild_received++;
}


static void sigpipe(int tt)
{
	log_dbg("Got a sigpipe!");
}


void master_check_sigchild(master_pipe *mp)
{
    int pid, status;

	if(!sigchild_received) {
		return;
	}

    // Wait for the child that caused this signal
    log_dbg("Got sigchld");
    pid = waitpid(-1, &status, WNOHANG);
    log_dbg(" ... pid=%d status=%d", pid, status);
    if(pid <= 0) {
        log_wtf("waitpid returned %d?! errno=%d (%s)", pid, errno, strerror(errno));
        return;
    }

	// dispatch it through the pipe
    task_dispatch_sigchild(mp, pid);
	sigchild_received = 0;
}


static void master_pipe_destructor(master_pipe *mp, int free_mem)
{
	bgio_state *bgio = mp->refcon;

	master_pipe_default_destructor(mp, free_mem);

	if(free_mem) {
		if(bgio) {
			bgio_stop(bgio);
			free(bgio);
		}

		// When we're done destroying the pipe, bail with no error.
		if(!opt_quiet) {
			fprintf(stderr, "rzh exited.\n");
		}

		// We only want to bail if we're exiting normally.  If we're
		// just cleaning up before forking, no need to bail!
		bail(0);
	}

	// we're only forking so only close the files.
	if(bgio) {
		close(bgio->master);
		close(bgio->slave);
	}
}


static void master_terminate(master_pipe *mp)
{
	bgio_state *bgio = mp->refcon;
	if(!bgio) {
		return;
	}

	kill(bgio->child_pid, SIGTERM);
}


static void master_pipe_sigchild(master_pipe *mp, int pid)
{
	bgio_state *bgio = mp->refcon;
	if(!bgio) {
		return;
	}

	if(pid == bgio->child_pid) {
		// our child shell has disappeared.
		// Kill off all tasks.  The removal of the last task will
		// trigger the destructor which leaps directly home.
		while(mp->task_head) {
			task_remove(mp);
		}
	}
}


/** Idles the topmost task on the master pipe.
 */

int master_idle(master_pipe *mp)
{
	if(mp->master_atom.atom.fd < 0) {
		// If we were reading from a socket, the socket has gone away.
		// Either that or the pty has closed (that shouldn't be possible).
		// Either way, our recourse is clear: time to bail.
		log_dbg("Master fd has closed; time to bail");
		while(mp->task_head) {
			task_remove(mp);
		}
	}

	task_spec *spec = mp->task_head->spec;
	return spec->idle_proc ? (*spec->idle_proc)(spec) : MAXINT;
}


static bgio_state* master_start_bgio()
{
	bgio_state *bgio = malloc(sizeof(bgio_state));
	if(bgio == NULL) {
		perror("allocating bgio_state");
		bail(47);
	}
	bgio_start(bgio, NULL);

	log_dbg("FD Master: %d", bgio->master);
	log_dbg("FD Slave: %d", bgio->slave);

	return bgio;
}


master_pipe* master_setup(int sockfd)
{
	master_pipe *mp;
	bgio_state *bgio = NULL;

	if(sockfd < 0) {
		// no socket, so open a tty
		bgio = master_start_bgio();
		mp = master_pipe_init(bgio->master);
	} else {
		// we were given a socket so use that
		mp = master_pipe_init(sockfd);
	}

	if(mp == NULL) {
		perror("allocating master pipe");
		bail(49);
	}
	mp->destruct_proc = master_pipe_destructor;
	mp->sigchild_proc = master_pipe_sigchild;
	mp->terminate_proc = master_terminate;
	mp->refcon = bgio;

	signal(SIGCHLD, sigchild);
	signal(SIGPIPE, sigpipe);

	return mp;
}


int master_get_window_width(master_pipe *mp)
{
	bgio_state *bgio = mp->refcon;
	return bgio ? bgio->window.ws_col : 80;
}

