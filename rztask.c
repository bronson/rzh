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
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "fifo.h"
#include "io/io.h"
#include "pipe.h"
#include "task.h"
#include "rztask.h"
#include "util.h"
#include "zscan.h"
#include "master.h"


typedef struct {
	int recv_start_count;	///< number of bytes in the write pipe when the rz started.
	int send_start_count;	///< number of bytes in the read pipe when the rz started.
	int call_cnt;			///< number of times idle proc has been called.
	struct timespec start_time;
} idle_state;


static void human_readable(size_t size, char *buf, int bufsiz)
{
	static const char *suffixes[] = { "B", "kB", "MB", "GB", 0 };
	enum { step = 1024 };

	const char **suffix = &suffixes[0];	
	size_t base = 1;
	size_t num;
	int rem;

	if(size > 0) {
		while(*suffix) {
			if(size >= base && size < base*step) {
				num = size / base;
				rem = (size * 100 / base) % 100;
				if(base == 1) {
					snprintf(buf, bufsiz, "%ld %s", (long)num, *suffix);
				} else {
					snprintf(buf, bufsiz, "%ld.%02d %s", (long)num, rem, *suffix);
				}
				return;
			}

			base *= 1024;
			suffix += 1;
		}
	}

	snprintf(buf, bufsiz, "%ld B", (long)size);
}


/** Returns the difference between the two timepsecs in seconds. */
static double timespec_diff(struct timespec *end, struct timespec *start)
{
	double ds, de;

	ds = start->tv_sec + ((double)start->tv_nsec)/1000000000;
	de = end->tv_sec + ((double)end->tv_nsec)/1000000000;

	return de - ds;
}


static idle_state* idle_create(master_pipe *mp)
{
	idle_state *idle = malloc(sizeof(idle_state));
	if(idle == NULL) {
		fprintf(stderr, "Could not allocate the idle structure.\n");
		bail(51);
	}

	idle->send_start_count = mp->input_master.bytes_written;
	idle->recv_start_count = mp->master_output.bytes_written;
	idle->call_cnt = 0;
	clock_gettime(CLOCK_REALTIME, &idle->start_time);

	return idle;
}


typedef struct {
	char rnum[64];
	char snum[64];
	char rbps[64];
	char sbps[64];
	double xfertime;
} idle_numbers;


static void idle_get_numbers(task_spec *spec, idle_numbers *out)
{
	struct timespec end_time;
	double xfertime;

	idle_state *idle = (idle_state*)spec->idle_refcon;

	clock_gettime(CLOCK_REALTIME, &end_time);
	xfertime = timespec_diff(&end_time, &idle->start_time);
	if(xfertime == 0.0) {
		// assume a nanosecond if no time elapsed.
		// this ensures we never divide by 0.
		xfertime = 0.000000001;
	}

	int sendcnt = spec->master->input_master.bytes_written - idle->send_start_count;
	human_readable(sendcnt, out->snum, sizeof(out->snum));
	human_readable((size_t)((double)sendcnt/xfertime), out->sbps, sizeof(out->sbps));

	int recvcnt = spec->master->master_output.bytes_written - idle->recv_start_count;
	human_readable(recvcnt, out->rnum, sizeof(out->rnum));
	human_readable((size_t)((double)recvcnt/xfertime), out->rbps, sizeof(out->rbps));

	out->xfertime = xfertime;
}


/** Pads the string out to the given number of characters with
 *  spaces and then appends a \r.
 */

void adjust_string_width(char *buf, int width)
{
	int i;

	for(i=strlen(buf); i<width; i++) {
		buf[i] = ' ';
	}

	buf[width-1] = '\r';
	buf[width] = '\0';
}


/** Prints a continually updated status string during the transfer. */

int rzt_idle_proc(task_spec *spec)
{
	char buf[256];
	idle_numbers numbers, *n = &numbers;
	int len;

	idle_state *idle = (idle_state*)spec->idle_refcon;

	idle->call_cnt += 1;
	idle_get_numbers(spec, &numbers);

	snprintf(buf, sizeof(buf),
		"Receiving %s in %.5f s: %s/s   Sending %s: %s",
		n->rnum, n->xfertime, n->rbps, n->snum, n->sbps);

	len = master_get_window_width(spec->master);
	if(len > sizeof(buf) - 1) {
		len = sizeof(buf) - 1;
	}
	adjust_string_width(buf, len);

	// ok, this list of pointers is a little silly.
	write(spec->master->task_head->next->spec->outfd, buf, len);

	return 1000;	// call us again in 1 second
}


/** Called at the end of the transfer to print a final status string. */

void rzt_idle_end(task_spec *spec)
{
	// We know that the new task is established before the
	// old task's destructor is called.

	char buf[256];
	idle_numbers numbers, *n = &numbers;

	idle_get_numbers(spec, &numbers);

	snprintf(buf, sizeof(buf),
		"Received %s in %.5f s: %s/s   Sent %s: %s/s\r\n",
		n->rnum, n->xfertime, n->rbps, n->snum, n->sbps);

	pipe_write(&spec->master->master_output, buf, strlen(buf));
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
	rzt_idle_end(spec);

	// if the maout zfin scanner saved some text for us, we
	// need to manually re-insert it into the pipe.
	zfinscanstate *maout = (zfinscanstate*)spec->maout_refcon;
	if(maout->savebuf) {
		log_dbg("RESTORE %d saved bytes into pipe: %s", maout->savecnt, sanitize(maout->savebuf, maout->savecnt));
		pipe_write(&spec->master->master_output, maout->savebuf, maout->savecnt);
	}

	if(free_mem) {
		zfinscan_destroy(spec->inma_refcon);
		zfinscan_destroy(spec->maout_refcon);
	}
}


static task_spec* rz_create_spec(master_pipe *mp, int fd[3], int child_pid)
{
	task_spec *spec = task_create_spec();

	spec->infd = fd[0];
	spec->outfd = fd[1];
	spec->errfd = fd[2];
	spec->child_pid = child_pid;

	spec->inma_proc = zfinscan;
	spec->inma_refcon = zfinscan_create(zfindrop);
	spec->maout_proc = zfinscan;
	spec->maout_refcon = zfinscan_create(zfinnooo);
	
	spec->idle_proc = rzt_idle_proc;
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
	task_install(mp, rz_create_spec(mp, fds, child_pid));
}

