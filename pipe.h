struct pipe;


/** A pipe atom is used to either fill or drain a pipe fifo.
 *  A single atom may be used for reading one fifo and simultaneously
 *  writing another.
 */

typedef struct {
	io_atom atom;				// represents a file or socket
	struct pipe *read_pipe;		// the pipe that this atom reads its data into
	struct pipe *write_pipe;	// the pipe that this atom gets its data from
} pipe_atom;


struct pipe {
	struct fifo fifo;			// the fifo itself
	pipe_atom *read_atom;		// all data read from here ...
	pipe_atom *write_atom;		// ... gets written to here
	int block_read;				// 1 if we need to stop reading, 0 if not.
};


int set_nonblock(int fd);
void pipe_atom_init(pipe_atom *atom, int fd);
void pipe_init(struct pipe *pipe, pipe_atom *ratom, pipe_atom *watom);


// Must be implemented by the caller.  Getting pretty hackish.
extern void bail(int val);

