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
	char rnum[64];
	char snum[64];
	char rbps[64];
	char sbps[64];
	double xfertime;
} idle_numbers;


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


idle_state* idle_create(master_pipe *mp)
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

static void adjust_string_width(char *buf, int width)
{
	int i;

	for(i=strlen(buf); i<width; i++) {
		buf[i] = ' ';
	}

	buf[width-1] = '\r';
	buf[width] = '\0';
}


/** Prints a continually updated status string during the transfer. */

int idle_proc(task_spec *spec)
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

void idle_end(task_spec *spec)
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

