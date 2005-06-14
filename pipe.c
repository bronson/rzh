/* pipe.c
 * 13 June 2005
 * Scott Bronson
 *
 * Uses a single fifo to create a pipe between two fds.
 */

// TODO: make real error handling
#include <stdio.h>
#include <string.h>

#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

#include "fifo.h"
#include "io/io.h"
#include "log.h"
#include "pipe.h"


int set_nonblock(int fd)
{
	int i;

	i = fcntl(fd, F_GETFL);
	if(i != -1) {
		i = fcntl(fd, F_SETFL, i | O_NONBLOCK);
	}

	return i;
}


/** Reads from the input side of the pipe, through the fifo
 * proc, into the fifo.  Immediately writes as much as possible,
 * scheduling any remainer for later.
 */

static void pipe_read(struct pipe *pipe)
{
	assert(fifo_avail(&pipe->fifo) > 0);
	fifo_read(&pipe->fifo, pipe->read_atom->atom.fd);

	// perhaps the fifo proc sucked up all the data.
	if(!fifo_count(&pipe->fifo))
		return;

	// immediately try to write the fifo out
	fifo_write(&pipe->fifo, pipe->write_atom->atom.fd);

	// return if we're all done (should be the normal case)
	if(!fifo_count(&pipe->fifo))
		return;

	// There's still data in the fifo so the last write didn't
	// complete.  We need to be notified when we can write again.
	io_enable(&pipe->write_atom->atom, IO_WRITE);

	// if there's no more room in the fifo then we need to stop trying
	// to read.  We'll restart reading when we manage to write some bytes.
	if(!fifo_avail(&pipe->fifo)) {
		io_disable(&pipe->read_atom->atom, IO_READ);
		pipe->block_read = 1;
	}
}


/** Writes the contents of the fifo to the output side of the pipe.
 */

static void pipe_write(struct pipe *pipe)
{
	assert(fifo_count(&pipe->fifo) > 0);
	fifo_write(&pipe->fifo, pipe->write_atom->atom.fd);
	assert(fifo_avail(&pipe->fifo) > 0);

	// We just freed up some room.  If reads are currently
	// blocking, we need to unblock them.
	if(pipe->block_read) {
		io_enable(&pipe->read_atom->atom, IO_READ);
		pipe->block_read = 0;
	}

	// if there's no more data left in the fifo,
	// turn off write notification
	if(!fifo_count(&pipe->fifo)) {
		io_disable(&pipe->write_atom->atom, IO_WRITE);
	}
}


/** This is the entrypoint for all pipe atom i/o notifications.
 */

static void pipe_io_proc(io_atom *aa, int flags)
{
	pipe_atom *atom = (pipe_atom*)aa;

	if(flags & IO_READ) {
		pipe_read(atom->read_pipe);
	}

	if(flags & IO_WRITE) {
		pipe_write(atom->write_pipe);
	}
}


/** Creates a pipe atom.  Pipe atoms are the endpoints for
 *  struct pipes.
 */

void pipe_atom_init(pipe_atom *atom, int fd)
{
	int err;

	set_nonblock(fd);
	io_atom_init(&atom->atom, fd, pipe_io_proc);
	err = io_add(&atom->atom, 0);
	if(err != 0) {
		fprintf(stderr, "%s setting up pipe tom", strerror(-err));
		bail(77);
	}

	atom->read_pipe = NULL;
	atom->write_pipe = NULL;
}


void pipe_init(struct pipe *pipe, pipe_atom *ratom, pipe_atom *watom)
{
	fifo_init(&pipe->fifo, BUFSIZ);
	if(pipe->fifo.buf == NULL) {
		perror("could not allocate fifo");
		bail(99);
	}

	pipe->read_atom = ratom;
	ratom->read_pipe = pipe;
	pipe->write_atom = watom;
	watom->write_pipe = pipe;

	pipe->block_read = 0;

	// all pipes start out listening for readable events
	io_enable(&pipe->read_atom->atom, IO_READ);
}

