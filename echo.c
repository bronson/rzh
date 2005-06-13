/* echo.c
 * 1 Nov 2004
 * Scott Bronson
 * 
 * The main routine for the rzh utility.
 *
 * This file is released under the MIT license.  This is basically the
 * same as public domain but absolves the author of liability.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <values.h>

#include "bgio.h"
#include "fifo.h"
#include "io/io.h"
#include "log.h"


typedef struct {
	io_atom ratom;	// all data read from here
	io_atom watom;	// gets written to here
	fifo ff;
	int block_read;	// 1 if we're blocking reads, 0 if not.
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


void ioproc(async_fifo *fifo, int flags)
{
	log_dbg("In ioproc, flags=%d", flags);

	if(flags & IO_READ) {
		assert(fifo_avail(&fifo->ff) > 0);
		fifo_read(&fifo->ff, fifo->ratom.fd);
		assert(fifo_count(&fifo->ff) > 0);

		// immediately try to write it out
		fifo_write(&fifo->ff, fifo->watom.fd);

		// if there's still data in the fifo, then the last write didn't
		// complete.  We need to be notified when we can write again.
		if(fifo_count(&fifo->ff)) {
			io_set(&fifo->watom, IO_WRITE);
			// if there's no more room in the fifo then we need to stop trying
			// to read.  We'll restart reading when we manage to write some bytes.
			if(!fifo_avail(&fifo->ff)) {
				io_set(&fifo->ratom, 0);
				fifo->block_read = 1;
			}
		}
	}

	if(flags & IO_WRITE) {
		assert(fifo_count(&fifo->ff) > 0);
		fifo_write(&fifo->ff, fifo->watom.fd);
		assert(fifo_avail(&fifo->ff) > 0);

		// if reads are currently blocking, we need to unblock them.
		if(fifo->block_read) {
			io_set(&fifo->ratom, IO_READ);
			fifo->block_read = 0;
		}

		// if there's no more data left in the fifo,
		// turn off write notification
		if(!fifo_count(&fifo->ff)) {
			io_set(&fifo->watom, 0);
		}
	}
}


void rdproc(io_atom *atom, int flags)
{
	if(flags != IO_READ) {
		log_dbg("ERROR: rdproc got flags of %d", flags);
	}

	ioproc((async_fifo*)((char*)atom - offsetof(async_fifo, ratom)), flags);
}


void wrproc(io_atom *atom, int flags)
{
	if(flags != IO_WRITE) {
		log_dbg("ERROR: wrproc got flags of %d", flags);
	}

	ioproc((async_fifo*)((char*)atom - offsetof(async_fifo, watom)), flags);
}


void async_fifo_init(async_fifo *fifo, int rd, int wr, int size)
{
	fifo->block_read = 0;

	fifo_init(&fifo->ff, size);
	if(fifo->ff.buf == NULL) {
		perror("could not allocate fifo");
		exit(99);
	}

	set_nonblock(rd);
	io_atom_init(&fifo->ratom, rd, rdproc);
	io_add(&fifo->ratom, IO_READ);

	set_nonblock(wr);
	io_atom_init(&fifo->watom, wr, wrproc);
	io_add(&fifo->watom, 0);
}


void dump_read(int fd)
{
	char buf[BUFSIZ];
	int cnt;

	for(;;) {
		cnt = read(fd, buf, sizeof(buf));
		log_dbg("read: cnt = %d: <<<%.*s>>>", cnt, cnt, buf);
	}
}


void echo(bgio_state *bgio)
{
	async_fifo mao; // master out (fed from stdin)
	async_fifo mai; // master in (fed to stdout)

	log_dbg("STDIN=%d -> master=%d", STDIN_FILENO, bgio->master);
	log_dbg("master=%d -> STDOUT=%d", bgio->master, STDOUT_FILENO);

	async_fifo_init(&mao, STDIN_FILENO, bgio->master, BUFSIZ);
	async_fifo_init(&mai, bgio->master, STDOUT_FILENO, BUFSIZ);

	for(;;) {
		io_wait(MAXINT);
		io_dispatch();
	}
}

