/* rztask.c
 * 13 June 2005
 * Scott Bronson
 *
 * The task that handles the zmodem child.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "log.h"
#include "fifo.h"
#include "io/io.h"
#include "cmd.h"
#include "pipe.h"
#include "task.h"
#include "rztask.h"
#include "util.h"
#include "zrq.h"
#include "zfin.h"
#include "idle.h"


command rzcmd;	// specifies the rz executable we should run.


static void parse_typing(const char *buf, int len, void *refcon)
{
	int i;
	task_spec *spec = (task_spec*)refcon;

	for(i=0; i<len; i++) {
		switch(buf[i]) {
			case 3:		// ^C
			case 24: 	// ^X
			case 27:	// ESC
			case 'q':
			case 'Q':
				log_info("TYPING: Cancel!");
				task_terminate(spec->master);
				break;

			default:
				fprintf(stderr, "KEY: len=%d <<%.*s>>\r\n", len, len, buf);
				;
		}
	}

	log_dbg("TYPING (%d chars): %.*s", len, len, buf);
}


static void typing_io_proc(io_atom *inatom, int flags)
{
	pipe_atom *atom = (pipe_atom*)inatom;

	char buf[128];
	int cnt;

	if(flags != IO_READ) {
		log_warn("Got flags=%d in parse_typing_proc!");
		if(!(flags & IO_READ)) {
			return;
		}
	}

	do {
		errno = 0;
		cnt = read(atom->atom.fd, buf, sizeof(buf));
	} while(cnt == -1 && errno == EINTR);

	if(cnt > 0) {
		parse_typing(buf, cnt, (void*)atom->read_pipe);
	} else if(cnt == 0) {
		log_warn("TYPING 0 read???");
	} else {
		log_warn("TYPING read error: %d (%s)", errno, strerror(errno));
	}
}


static void cherr_proc(io_atom *inatom, int flags)
{
	heavy_atom *atom = (heavy_atom*)inatom;
	char buf[512];
	int cnt;

	if(flags != IO_READ) {
		log_warn("Got flags=%d in cherr_proc!");
		if(!(flags & IO_READ)) {
			return;
		}
	}

	do {
		errno = 0;
		cnt = read(atom->atom.fd, buf, sizeof(buf));
	} while(cnt == -1 && errno == EINTR);

	if(cnt > 0) {
		log_warn("CHILD STDERR fd=%d: <<<%.*s>>>", atom->atom.fd, cnt, buf);
	} else if(cnt == 0) {
		// eof on stderr.
		io_del(&atom->atom);
		log_dbg("Closed child stderr %d.", atom->atom.fd);
		atom->atom.fd = -1;
	} else {
		log_warn("CHILD STDERR fd=%d:read error: %d (%s)",
				atom->atom.fd, errno, strerror(errno));
	}
}


static void rzt_destructor_proc(task_spec *spec, int free_mem)
{
	idle_end(spec);

	// if the maout zfin scanner saved some text for us, we
	// need to manually re-insert it into the pipe.
	zfinscanstate *maout = (zfinscanstate*)spec->maout_refcon;
	if(maout->savebuf) {
		log_dbg("RESTORE %d saved bytes into pipe: %s", maout->savecnt, sanitize(maout->savebuf, maout->savecnt));
		pipe_write(&spec->master->master_output, maout->savebuf, maout->savecnt);
	}

	if(free_mem) {
		zfin_destroy(spec->inma_refcon);
		zfin_destroy(spec->maout_refcon);
	}

	task_default_destructor(spec, free_mem);
}


static task_spec* rz_create_spec(master_pipe *mp, int fd[3], int child_pid)
{
	task_spec *spec = task_create_spec();

	spec->infd = fd[0];
	spec->outfd = fd[1];
	spec->errfd = fd[2];
	spec->child_pid = child_pid;

	spec->inma_proc = zfin_scan;
	spec->inma_refcon = zfin_create(mp, zfin_term);
	spec->maout_proc = zfin_scan;
	spec->maout_refcon = zfin_create(mp, zfin_nooo);
	
	spec->idle_proc = idle_proc;
	spec->idle_refcon = idle_create(mp);

	spec->destruct_proc = rzt_destructor_proc;
	spec->err_proc = cherr_proc;
	spec->verso_input_proc = typing_io_proc;
	spec->verso_input_refcon = spec;

	return spec;
}


/** Forks the zmodem receive process.  Fills in outfds with the fds
 *  of the new process, and child_pid with its pid.
 */

static void fork_rz_process(master_pipe *mp, int outfds[3], int *child_pid)
{
	int chstdin[2];
	int chstdout[2];
	int chstderr[2];
	int pid;

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
		bail(79);
	}

	log_dbg("FD chstdin: rd=%d wr=%d", chstdin[0], chstdin[1]);
	log_dbg("FD chstdout: rd=%d wr=%d", chstdout[0], chstdout[1]);
	log_dbg("FD chstderr: rd=%d wr=%d", chstderr[0], chstderr[1]);

	pid = fork();

	if(pid < 0) {
		perror("forking receive process");
		bail(23);
	}

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

		chdir_to_dldir();
		task_fork_prepare(mp);
		rzh_fork_prepare();
		io_exit_check();

		execv(rzcmd.path, rzcmd.args);
		fprintf(stderr, "Could not exec /usr/bin/rz: %s\n",
				strerror(errno));
		exit(89);
	}

	close(chstdin[0]);
	close(chstdout[1]);
	close(chstderr[1]);

	outfds[0] = chstdout[0];	// we read from child's stdout
	outfds[1] = chstdin[1];		// and write to the child's stdin
	outfds[2] = chstderr[0];	// and read (sorta) from child's stderr
	*child_pid = pid;
}


void rztask_install(master_pipe *mp)
{
	int fds[3];
	int child_pid;

	fork_rz_process(mp, fds, &child_pid);
	task_install(mp, rz_create_spec(mp, fds, child_pid));
}

