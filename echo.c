/* echo.c
 * 13 June 2005
 * Scott Bronson
 *
 * Conveys data between the bgio master and stdin/stdout.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <values.h>
#include <assert.h>
#include <pty.h>

#include "log.h"
#include "bgio.h"
#include "fifo.h"
#include "io/io.h"
#include "pipe.h"
#include "task.h"
#include "rztask.h"
#include "zrq.h"
#include "util.h"


static void echo_destructor(task_spec *spec, int free_mem)
{
	// Don't want to close stdin/stdout/stderr
	spec->infd = -1;
	spec->outfd = -1;
	spec->errfd = -1;

	// call the default destructor
	task_default_destructor(spec, free_mem);
}


static task_spec *echo_create_spec()
{
	task_spec *spec = task_create_spec();
	if(spec == NULL) {
		perror("allocating echo task spec");
		bail(45);
	}

	spec->infd = STDIN_FILENO;
	spec->outfd = STDOUT_FILENO;
	spec->errfd = -1;	// we'll ignore stderr
	spec->child_pid = -1;
	spec->destruct_proc = echo_destructor;

	return spec;
}


/////////////////  Echo Scanner


// This routine is called when the zrq scanner discovers the zmodem
// start sequence.

static void echo_scanner_start_proc(void *refcon)
{
	// the refcon is the master_pipe
	rztask_install(refcon);
}


// This routine is called to process all data passing over the pipe.

static void echo_scanner_filter_proc(struct fifo *f, const char *buf, int size, int fd)
{
	if(size > 0) {
		zrq_scan(f->refcon, buf, buf+size, f, fd);
	}
}


static void echo_scanner_destructor(task_spec *spec, int free_mem)
{
	if(free_mem) {
		zrq_destroy(spec->maout_refcon);
	}
	
	echo_destructor(spec, free_mem);
}


/** An echo scanner is just the echo task but it has a zmodem
 *  start scanner attached.  When the start scanner notices a
 *  transfer request, it fires up a receive task.
 */

task_spec *echo_scanner_create_spec(master_pipe *mp)
{
	task_spec *spec = echo_create_spec();

	// ensure we're not clobbering anything unexpected.
	assert(!spec->maout_refcon);
	assert(!spec->maout_proc);
	assert(spec->destruct_proc == echo_destructor);

	spec->maout_refcon = zrq_create(echo_scanner_start_proc, mp);
	spec->maout_proc = echo_scanner_filter_proc;
	spec->destruct_proc = echo_scanner_destructor;

	return spec;
}

