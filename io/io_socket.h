/** @file io_socket.c
 *
 * This is actually a layer that adds sockets to whatever underlying io
 * layer you decide to use.
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include "io.h"


typedef struct {
	struct in_addr addr;	///< packed address (use inet_ntoa)
	int port;				///< port number in host order (regular int)
} socket_addr;



/** Sets up an outgoing connection
 *
 * @param io The io_atom to use.
 * @param proc The io_proc to give the atom.
 * @param remote the IP address and port number of the system to connect to.
 * @param flags The read/write flags that the atom should start with.
 *
 * todo: I don't think we give the user a way to select the local socket?
 * do we?
 */

int io_socket_connect(io_atom *io, io_proc proc, socket_addr remote, int flags);


/** Accepts an incoming connection.
 *
 * You must have previously set up a listening socket using io_socket_listen.
 *
 * @param io       The io_atom to initialize with the new connection.
 * @param proc     The io_proc to initialize the atom with.
 * @param listener The io_atom listening for incoming connections.
 *                 Previously set up using io_socket_listen().
 * @param remote   Returns the IP address and port number of the remote
 *                 system.  Pass NULL if you don't care.
 */

int io_socket_accept(io_atom *io, io_proc proc, int flags, io_atom *listener, socket_addr *remote);


/** Sets up a socket to listen for incoming connections.
 *
 * Connections are passed to the io_proc using IO_READ.
 * (todo: insert example code here)
 *
 * @param io The io_atom to initialize for the new connection.
 * @param proc The io_proc to initialize the atom with.
 * @param the local IP address and port to listen on.  Use INADDR_ANY
 * 		to get the
 */

int io_socket_listen(io_atom *io, io_proc proc, socket_addr local);


/** Closes an open socket.  The io_atom must have been set up previously using
 * one of io_socket_connect(), io_socket_listen(), or io_socket_accept().
 */

void io_socket_close(io_atom *io);


/** Reads data from a socket.
 *
 * This routine handles strange boundary cases like re-trying a read that
 * was interrupted by a signal.
 *
 * @returns The number of bytes that were read.  0 will be returned
 * if there was no data to read and the socket was marked nonblocking
 * (otherwise, if the socket is marked blocking, it will block until
 * data is available for reading).  -EPIPE is returned if the remote
 * connection was closed.  Otherwise, a negative number will be
 * returned for any other sort of error.  Check errno for the reason.
 *
 * @param io The io_atom to read from.
 * @param buf The buffer ot fill with the incoming adata.
 * @param cnt The size of the buffer.  Not more than this number of
 * 		bytes will be written to the buffer.
 */

int io_socket_read(io_atom *io, char *buf, size_t cnt, size_t *len);

