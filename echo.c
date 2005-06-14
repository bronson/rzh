/* echo.c
 * 13 June 2005
 * Scott Bronson
 *
 * Uses pipes to shuttle data back and forth.
 * Has procs to modify the data streams as they pass.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <values.h>

#include "bgio.h"
#include "child.h"
#include "fifo.h"
#include "io/io.h"
#include "log.h"
#include "pipe.h"
#include "zscan.h"



/* Analyzes all data moving from the master to stdout.  If it notices a
 * zmodem transfer start request, it starts the transfer (zscanstate::proc)
 */

static void maout_proc(struct fifo *f, const char *buf, int size)
{
	zscan(f->refcon, buf, buf+size, f);
}


void echo(bgio_state *bgio)
{
	pipe_atom a_stdin;
	pipe_atom a_stdout;
	pipe_atom a_master;

	struct pipe inma;	// stdin -> master
	struct pipe maout;	// master -> stdout

	zscanstate zscan;


	log_dbg("STDIN=%d -> master=%d", STDIN_FILENO, bgio->master);
	log_dbg("master=%d -> STDOUT=%d", bgio->master, STDOUT_FILENO);

	// init the atoms
	pipe_atom_init(&a_stdin, STDIN_FILENO);
	pipe_atom_init(&a_stdout, STDOUT_FILENO);
	pipe_atom_init(&a_master, bgio->master);

	// create the fifos that read from and write to the atoms.
	pipe_init(&inma, &a_stdin, &a_master);
	pipe_init(&maout, &a_master, &a_stdout);

	// add the zmodem start scanner
	zscanstate_init(&zscan);
	zscan.start_proc = start_proc;
	zscan.start_refcon = &maout;

	// insert the start scanner into the master->stdout link
	maout.fifo.proc = maout_proc;
	maout.fifo.refcon = &zscan;

	for(;;) {
		io_wait(MAXINT);
		io_dispatch();
	}
}

