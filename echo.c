/* echo.c
 * 13 June 2005
 * Scott Bronson
 *
 * Uses fifos and the io lib to shuttle data back and forth.
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
#include "fifo.h"
#include "io/io.h"
#include "log.h"


struct async_fifo;


// Because a single atom may be used for reading one fifo while
// simultaneously writing another, it needs its own structure.
typedef struct {
	io_atom atom;				// represents a file or socket
	struct async_fifo *rfifo;	// this fifo uses this atom to obtain its data from
	struct async_fifo *wfifo;	// this fifo uses this atom to write its data to
} fifo_atom;


typedef struct async_fifo {
	fifo ff;				// the fifo itself
	fifo_atom *ratom;		// all data read from here
	fifo_atom *watom;		// ... gets written to here
	int block_read;			// 1 if we're blocking reads, 0 if not.
} async_fifo;


int set_nonblock(int fd)
{
	int i;

	i = fcntl(fd, F_GETFL);
	if(i != -1) {
		i = fcntl(fd, F_SETFL, i | O_NONBLOCK);
	}

	return i;
}



void async_fifo_read(async_fifo *fifo)
{
	int cnt;

	assert(fifo_avail(&fifo->ff) > 0);
	cnt = fifo_read(&fifo->ff, fifo->ratom->atom.fd);
	assert(fifo_count(&fifo->ff) > 0);

	// immediately try to write it out
	fifo_write(&fifo->ff, fifo->watom->atom.fd);

	// if there's still data in the fifo, then the last write didn't
	// complete.  We need to be notified when we can write again.
	if(fifo_count(&fifo->ff)) {
		io_enable(&fifo->watom->atom, IO_WRITE);
		// if there's no more room in the fifo then we need to stop trying
		// to read.  We'll restart reading when we manage to write some bytes.
		if(!fifo_avail(&fifo->ff)) {
			io_disable(&fifo->ratom->atom, IO_READ);
			fifo->block_read = 1;
		}
	}
}


void async_fifo_write(async_fifo *fifo)
{
	assert(fifo_count(&fifo->ff) > 0);
	fifo_write(&fifo->ff, fifo->watom->atom.fd);
	assert(fifo_avail(&fifo->ff) > 0);

	// if reads are currently blocking, we need to unblock them.
	if(fifo->block_read) {
		io_enable(&fifo->ratom->atom, IO_READ);
		fifo->block_read = 0;
	}

	// if there's no more data left in the fifo,
	// turn off write notification
	if(!fifo_count(&fifo->ff)) {
		io_disable(&fifo->watom->atom, IO_WRITE);
	}
}


void atomproc(io_atom *aa, int flags)
{
	fifo_atom *atom = (fifo_atom*)aa;

	if(flags & IO_READ) {
		async_fifo_read(atom->rfifo);
	}

	if(flags & IO_WRITE) {
		async_fifo_write(atom->wfifo);
	}
}


void fifo_atom_init(fifo_atom *atom, int fd)
{
	int err;

	set_nonblock(fd);
	io_atom_init(&atom->atom, fd, atomproc);
	err = io_add(&atom->atom, 0);
	if(err != 0) {
		fprintf(stderr, "%s setting up read atom", strerror(-err));
		bail(77);
	}

	atom->rfifo = NULL;
	atom->wfifo = NULL;
}


void async_fifo_init(async_fifo *fifo, fifo_atom *ratom,
		fifo_atom *watom, fifo_proc proc)
{
	fifo_init(&fifo->ff, BUFSIZ);
	if(fifo->ff.buf == NULL) {
		perror("could not allocate fifo");
		bail(99);
	}

	fifo->ff.proc = proc;
	fifo->ratom = ratom;
	ratom->rfifo = fifo;
	fifo->watom = watom;
	watom->wfifo = fifo;

	fifo->block_read = 0;

	io_enable(&fifo->ratom->atom, IO_READ);
}


void echo(bgio_state *bgio)
{
	fifo_atom a_stdin;
	fifo_atom a_stdout;
	fifo_atom a_master;

	async_fifo f_im; // master out (fed from stdin)
	async_fifo f_mo; // master in (fed to stdout)

	log_dbg("STDIN=%d -> master=%d", STDIN_FILENO, bgio->master);
	log_dbg("master=%d -> STDOUT=%d", bgio->master, STDOUT_FILENO);

	// init the atoms
	fifo_atom_init(&a_stdin, STDIN_FILENO);
	fifo_atom_init(&a_stdout, STDOUT_FILENO);
	fifo_atom_init(&a_master, bgio->master);

	// init the fifos
	async_fifo_init(&f_im, &a_stdin, &a_master, NULL);
	async_fifo_init(&f_mo, &a_master, &a_stdout, NULL);

	for(;;) {
		io_wait(MAXINT);
		io_dispatch();
	}
}

