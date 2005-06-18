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
#include "pipe.h"
#include "task.h"
#include "rztask.h"
#include "util.h"


int idle_cnt;


void idle_init()
{
	idle_cnt = 0;
}


int idle_proc(task_spec *spec)
{
	char buf[128];

	idle_cnt += 1;
	snprintf(buf, sizeof(buf), "%02d Hello!\r", idle_cnt);

	// ok, this is idiotic.
	write(spec->master->task_head->next->spec->outfd, buf, strlen(buf));

	return 1000;	// call us again in 1 second
}


static void parse_typing(const char *buf, int len, void *refcon)
{
	int i;
	task_spec *spec = (task_spec*)refcon;

	for(i=0; i<len; i++) {
		switch(buf[i]) {
			case 3:		// ^C
			case 24: 	// ^X
			case 27:	// ESC
				fprintf(stderr, "CANCEL!\r\n");
				master_pipe_terminate(spec->master);
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


static void rz_destructor(task_spec *spec, int free_mem)
{
	if(free_mem) {
		// we're not exiting due to a fork
		if(send_extra_nl) {
			// see the manpage for why we send the extra newline.
			write(spec->master->master_atom.atom.fd, "\n", 1);
		}
	}
	task_default_destructor(spec, free_mem);
}


static task_spec* rz_create_spec(int fd[3], int child_pid)
{
	task_spec *spec = task_create_spec();

	idle_init();

	spec->infd = fd[0];
	spec->outfd = fd[1];
	spec->errfd = fd[2];
	spec->child_pid = child_pid;

	spec->idle_proc = idle_proc;
	spec->err_proc = cherr_proc;
	spec->destruct_proc = rz_destructor;
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

		execl("/usr/bin/rz", "rz", "--disable-timeouts", NULL);
//		execl("/usr/bin/rz", "rz", 0);
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
	task_install(mp, rz_create_spec(fds, child_pid));
}

