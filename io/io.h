// io.h
// Scott Bronson
// 2 Oct 2003

/** @file io.h
 *
 * This is the generic Async I/O API.  It can be implemented using
 * select, poll, epoll, kqueue, aio, and /dev/poll (hopefully).
 *
 * TODO: should make it so that we can compile every io method
 * into the code then select the most appropriate one at runtime.
 */

#ifndef IO_H
#define IO_H

/// Flag, tells if we're interested in read events.
#define IO_READ 0x01
/// Flag, tells if we're interested in write events.
#define IO_WRITE 0x02
/// Flag, tells if we're interested in exceptional events.
#define IO_EXCEPT 0x04

// reserved for use by applications
#define IO_USER1 0x10
#define IO_USER2 0x20
#define IO_USER3 0x40
#define IO_USER4 0x80


/// Tells how many incoming connections we can handle at once
/// (this is just the backlog parameter to listen)
#ifndef STD_LISTEN_SIZE
#define STD_LISTEN_SIZE 128
#endif


struct io_atom;

/**
 * This routine is called whenever there is action on an atom.
 *
 * @param atom The atom that the proc is being called on.
 * @param flags What sort of action is happening.
 */
typedef void (*io_proc)(struct io_atom *atom, int flags);


/**
 * This structure is used to keep track of all the filehandles
 * we're watching for events and the functions to call when they
 * occur.
 *
 * On the surface, it may seem odd to have so small a structure.
 * Why not just pass the proc and fd to io_add() as arguments
 * and not worry about memory allocation at all?  The reason is
 * that this io_atom will probably be a small part of a much
 * larger structure.  It's easy to convert from the atom back
 * to the larger structure.  Better to keep the atom information
 * with its related data structures than off in an io-specific
 * chunk of memory.
 */
typedef struct io_atom {
	io_proc proc;   ///< The function to call when there is an event on fd.
	int fd;         ///< The fd to watch for events.
} io_atom;

#define io_atom_init(io,ff,pp) ((io)->fd=(ff),(io)->proc=(pp))

void io_init();     ///< Call this routine once when your code starts.  It prepares the io library for use.
void io_exit();     ///< Call this routine once when your program terminates.  It just releases any resources allocated by io_init.
int io_exit_check();	///< Returns how many fds were leaked.  Also prints them to stderr.


/** Adds the given io_atom to the current watch list.
 *
 * The io_atom must be pre-allocated and exist until you
 * call io_del() on it.
 *
 * @param atom The io_atom to add.
 * @param flags The events to watch for.  Note that there's no way to retrieve the flags once set.
 * @returns The appropriate error code or 0 if there was no error.
 */

int io_add(io_atom *atom, int flags);		///< Adds the given atom to the list of files being watched.
int io_enable(io_atom *atom, int flags);	///< Enables the given flag(s) without affecting any others.
int io_disable(io_atom *atom, int flags);	///< Disables the given flag(s) without affecting any others.
int io_set(io_atom *atom, int flags);   	///< Sets the io_atom::flags on the given atom to flags.
int io_del(io_atom *atom);              	///< Removes the atom from the list 

/// Waits for an event, then handles it.  Stops waiting if timeout occurs.
/// Specify MAXINT for no timeout.  The timeout is specified in ms.
int io_wait(unsigned int timeout);
void io_dispatch();

#endif

