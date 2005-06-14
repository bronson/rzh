/* echo.c
 * 13 June 2005
 * Scott Bronson
 *
 * Uses pipes to shuttle data back and forth.
 * Has procs to modify the data streams as they pass.
 */

#include <unistd.h>
#include <values.h>

#include "bgio.h"
#include "fifo.h"
#include "io/io.h"
#include "log.h"
#include "pipe.h"
#include "child.h"
#include "zscan.h"



/* Analyzes all data moving from the master to stdout.  If it notices a
 * zmodem transfer start request, it starts the transfer (zscanstate::proc)
 */

static void master_output_proc(struct fifo *f, const char *buf, int size)
{
	zscan(f->refcon, buf, buf+size, f);
}


void echo(bgio_state *bgio)
{
	zscanstate zscan;

	log_dbg("STDIN=%d -> master=%d", STDIN_FILENO, bgio->master);
	log_dbg("master=%d -> STDOUT=%d", bgio->master, STDOUT_FILENO);

	// init the atoms
	pipe_atom_init(&a_stdin, STDIN_FILENO);
	pipe_atom_init(&a_stdout, STDOUT_FILENO);
	pipe_atom_init(&a_master, bgio->master);

	// create the fifos that read from and write to the atoms.
	pipe_init(&p_input_master, &a_stdin, &a_master, 8192);
	pipe_init(&p_master_output, &a_master, &a_stdout, 8192);

	// add the zmodem start procedure to the scanner
	zscanstate_init(&zscan);
	zscan.start_proc = start_proc;
	zscan.start_refcon = NULL;	// not needed?

	// insert the scanner into the master->stdout link
	p_master_output.fifo.proc = master_output_proc;
	p_master_output.fifo.refcon = &zscan;

	for(;;) {
		io_wait(MAXINT);
		io_dispatch();
	}
}

