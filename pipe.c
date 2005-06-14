/* pipe.c
 * 13 June 2005
 * Scott Bronson
 *
 * Uses a single fifo to create a pipe between two fds.
 */

// TODO: make real error handling
#include <stdio.h>
#include <string.h>

#include <errno.h>
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

static void pipe_auto_read(struct pipe *pipe)
{
	assert(fifo_avail(&pipe->fifo) > 0);
	fifo_read(&pipe->fifo, pipe->read_atom->atom.fd);

	// perhaps the fifo proc sucked up all the data.
	if(!fifo_count(&pipe->fifo)) {
		return;
	}

	// immediately try to write the fifo out
	fifo_write(&pipe->fifo, pipe->write_atom->atom.fd);

	// return if we're all done (should be the normal case)
	if(!fifo_count(&pipe->fifo)) {
		return;
	}

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

static void pipe_auto_write(struct pipe *pipe)
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


/** Tries to write to the atom immediately.  Anything that the
 *  atom didn't consume will be stored by the pipe for later.
 *  This is intended to fill pipes programmatically rather than
 *  from a file handle.
 *
 *  @returns The number of bytes written.  This will always equal size
 *  unless the receiving filehandle is blocking AND the fifo is full.
 *  (this should never happen)
 */

int pipe_write(struct pipe *pipe, const char *buf, int size)
{
	int cnt;
	int total = 0;

	if(!fifo_count(&pipe->fifo)) {
		// Nothing in the pipe.  We can try an immediate write.
		do {
			errno = 0;
			cnt = write(pipe->write_atom->atom.fd, buf, size);
		} while(cnt == -1 && errno == EINTR);
		if(cnt < 0) {
			log_warn("pipe write: cnt=%d error=%d (%s)", cnt, errno, strerror(errno));
		}
		if(cnt > 0) {
			buf += cnt;
			total += cnt;
			size -= cnt;
		}
	}

	if(size <= 0) {
		// no need to watch for write events on this file
		io_disable(&pipe->write_atom->atom, IO_WRITE);
		return total;
	}

	// There was unwritten data.  Try to write it to the fifo.
	cnt = fifo_avail(&pipe->fifo);
	if(size < cnt) cnt = size;
	fifo_unsafe_append(&pipe->fifo, buf, cnt);
	buf += cnt;
	total += cnt;
	size -= cnt;

	// Need to be notified when we can write again
	io_enable(&pipe->write_atom->atom, IO_WRITE);

	return total;
}


/** Like pipe_write() except that the data is inserted BEFORE the
 *  existing data rather than being appended after.
 *
 *  Note that this can only be called once.  It must try to drain the
 *  pipe, so anything inserted later may come after the data in the pipe.
 *
 *  @returns The number of bytes written.  This will always equal size
 *  unless the receiving filehandle is blocking AND the fifo is full.
 *  (this should never happen)
 */

int pipe_prepend(struct pipe *pipe, const char *buf, int size)
{
	int cnt;
	int total = 0;

	// Always try an immediate write since we're inserting
	do {
		errno = 0;
		cnt = write(pipe->write_atom->atom.fd, buf, size);
	} while(cnt == -1 && errno == EINTR);
	if(cnt < 0) {
		log_warn("pipe write: cnt=%d error=%d (%s)", cnt, errno, strerror(errno));
	}
	if(cnt > 0) {
		buf += cnt;
		total += cnt;
		size -= cnt;
	}

	if(size <= 0) {
		// If there's still more data in the fifo, we should
		// try to write so flags are set properly.
		if(fifo_count(&pipe->fifo)) {
			pipe_auto_write(pipe);
		}
		return total;
	}

	// There was unwritten data.  Try to write it to the fifo.
	cnt = fifo_avail(&pipe->fifo);
	if(size < cnt) cnt = size;
	fifo_unsafe_prepend(&pipe->fifo, buf, cnt);
	buf += cnt;
	total += cnt;
	size -= cnt;

	// Need to be notified when we can write again
	io_enable(&pipe->write_atom->atom, IO_WRITE);

	return total;
}

/** This is the entrypoint for all pipe atom i/o notifications.
 */

static void pipe_io_proc(io_atom *aa, int flags)
{
	pipe_atom *atom = (pipe_atom*)aa;

	if(flags & IO_READ) {
		pipe_auto_read(atom->read_pipe);
	}

	if(flags & IO_WRITE) {
		pipe_auto_write(atom->write_pipe);
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


void pipe_init(struct pipe *pipe, pipe_atom *ratom, pipe_atom *watom, int size)
{
	fifo_init(&pipe->fifo, size);
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
	// unless there's no atom on the read side (i.e. the progress pipe
	// which is filled by a function, not by a reader).
	if(pipe->read_atom) {
		io_enable(&pipe->read_atom->atom, IO_READ);
	}
}

