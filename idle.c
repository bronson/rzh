/* idle.c
 * 13 June 2005
 * Scott Bronson
 *
 * Prints the in-progress display while a zmodem transfer is happening.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "fifo.h"
#include "io/io.h"
#include "pipe.h"
#include "task.h"
#include "idle.h"
#include "util.h"
#include "master.h"


typedef struct {
	// this data structure contains the stats for the transfer
	// in human-readable units.
	char rnum[64];		// number of bytes received
	char snum[64];		// number of bytes sent
	char rbps[64];		// receive rate
	char sbps[64];		// send rate
	char xfertime[64];	// elapsed time of transfer
} idle_numbers;


static void human_bytes(size_t size, char *buf, int bufsiz)
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


static int human_time(double dsecs, char *buf, int bufsiz)
{
	char *cp = buf;
	int cnt;

	int seconds = (int)dsecs;
	int minutes = seconds / 60;
	int hours = seconds / 3600;

	if(hours > 0) {
		cnt = snprintf(cp, bufsiz, "%d:", hours);
		cp += cnt; bufsiz -= cnt;
	}

	cnt = snprintf(buf, bufsiz, "%d:%02d.%d", minutes % 60,
			seconds % 60, (int)(10*dsecs)%10);
	cp += cnt; bufsiz -= cnt;

	return cp - buf;
}


/** Returns the difference between the two timepsecs in seconds. */
static double timespec_diff(struct timespec *end, struct timespec *start)
{
	double ds, de;

	ds = start->tv_sec + ((double)start->tv_nsec)/1000000000;
	de = end->tv_sec + ((double)end->tv_nsec)/1000000000;

	return de - ds;
}


idle_state* idle_create(master_pipe *mp, const char *command)
{
	idle_state *idle = malloc(sizeof(idle_state));
	if(idle == NULL) {
		fprintf(stderr, "Could not allocate the idle structure.\n");
		bail(51);
	}
	memset(idle, 0, sizeof(idle_state));

	idle->command = command;
	idle->send_start_count = mp->input_master.bytes_written;
	idle->recv_start_count = mp->master_output.bytes_written;
	idle->call_cnt = 0;
	clock_gettime(CLOCK_REALTIME, &idle->start_time);

	return idle;
}


void idle_destroy(idle_state* idle)
{
	free(idle);
}


static void idle_get_numbers(task_spec *spec, idle_numbers *out)
{
	struct timespec end_time;
	double xfertime;

	idle_state *idle = (idle_state*)spec->idle_refcon;

	clock_gettime(CLOCK_REALTIME, &end_time);
	xfertime = timespec_diff(&end_time, &idle->start_time);
	if(xfertime <= 0.0) {
		// assume a nanosecond if no time elapsed.
		// this ensures we never divide by 0.
		xfertime = 0.000000001;
	}

	int sendcnt = spec->master->input_master.bytes_written - idle->send_start_count;
	human_bytes(sendcnt, out->snum, sizeof(out->snum));
	human_bytes((size_t)((double)sendcnt/xfertime), out->sbps, sizeof(out->sbps));

	int recvcnt = spec->master->master_output.bytes_written - idle->recv_start_count;
	human_bytes(recvcnt, out->rnum, sizeof(out->rnum));
	human_bytes((size_t)((double)recvcnt/xfertime), out->rbps, sizeof(out->rbps));

	human_time(xfertime, out->xfertime, sizeof(out->xfertime));
}


/** Pads the string out to the given number of characters with spaces
 */

static void adjust_string_width(char *buf, int width)
{
	int i;

	for(i=strlen(buf); i<width; i++) {
		buf[i] = ' ';
	}
}


/** Prints a continually updated status string during the transfer.
 */

int idle_proc(task_spec *spec)
{
	enum {
		sleeptime = 300,	// time between invocations in ms.
		fuzztime = 50,		// if there are less than fuzztime ms until
			// the idle proc comes due, then we just run the idle proc
			// now.  It's early, but the user won't know the difference.
			// More efficient to do it this way rather than requiring our
			// own timeout at precisely the right time.
		// In short:
		// minimum time to next idleproc: sleeptime - fuzztime.
		// maximum time to next idleproc: sleeptime.
	};

	char buf[256];
	int len;
	idle_numbers numbers, *n = &numbers;
	idle_state *idle = (idle_state*)spec->idle_refcon;

	struct timespec now_time;
	double diff;
	int bah;


	if(opt_quiet) {
		return sleeptime;
	}

	// We will get called much more often than our sleeptime when i/o
	// is heavy.  Ensure that we don't update the display too fast.
	// The first time we're called, tv_sec will be 0.  After that,
	// we just need to ensure that we wait for the appropriate amount
	// of time before updating the display again.
	if(idle->last_time.tv_sec != 0) {
		clock_gettime(CLOCK_REALTIME, &now_time);
		diff = timespec_diff(&now_time, &idle->last_time);
		bah = sleeptime - (int)(diff*1000);
		if(bah > fuzztime) {
			return bah;
		}
	}


	log_dbg("updating display");

	idle->call_cnt += 1;
	idle_get_numbers(spec, &numbers);

	snprintf(buf, sizeof(buf),
		"%s %s: received %s at %s/s, sent %s at %s/s",
		n->xfertime, idle->command, n->rnum, n->rbps, n->snum, n->sbps);

	len = master_get_window_width(spec->master);
	if(len > sizeof(buf) - 1) {
		len = sizeof(buf) - 1;
	}
	adjust_string_width(buf, len);

	buf[len-1] = '\r';

	// ok, this list of pointers is a little silly.
	write(spec->master->task_head->next->spec->outfd, buf, len);

	clock_gettime(CLOCK_REALTIME, &idle->last_time);
	return sleeptime;
}


/** Called at the end of the transfer to print a final status string.
 *  It also frees the idle state.
 */

void idle_end(task_spec *spec)
{
	int len;
	// We know that the new task is established before the
	// old task's destructor is called.

	char buf[256];
	idle_numbers numbers, *n = &numbers;
	idle_state *idle = (idle_state*)spec->idle_refcon;

	if(opt_quiet) {
		return;
	}

	idle_get_numbers(spec, &numbers);

	snprintf(buf, sizeof(buf),
		"Received %s at %s/s   Sent %s at %s/s.",
		n->rnum, n->rbps, n->snum, n->sbps);

	len = master_get_window_width(spec->master);
	if(len > sizeof(buf) - 1) {
		len = sizeof(buf) - 1;
	}
	adjust_string_width(buf, len);

	buf[len-2] = '\r';
	buf[len-1] = '\n';

	pipe_write(&spec->master->master_output, buf, len);

	idle_destroy(idle);
}

