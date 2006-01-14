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


/** Just a lower-level version of io_socket_connect.  Returns the fd
 *  instead of the fully-prepared atom.  You then need to call io_atom_init
 *  and io_add yourself.
 */

int io_socket_connect_fd(socket_addr remote);


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


/** Just a utility function.  This routine tries to parse an integer
 *  and, unlike any of the stdc routines, will actually tell you if
 *  an error occurred or not.
 */

int io_safe_atoi(const char *str, int *num);


/** Parses a string to an address suitable for use with io_socket.
 *  Accepts "1.1.1.1:22", "1.1.1.1" (default port), and "22" (default
 *  address).  If either the address or port weren't specified, then
 *  this code leaves the original value unchanged.  You can either
 *  fill in defaults before passing sock to this routine, or you can
 *  use sock.addr.s_addr=INADDR_ANY and sock.port=0 for illegal NULL
 *  values.
 *
 *  If the string doesn't specify either an address or a port then the
 *  original contents of the sock variable remain unchanged.
 *
 *  @returns NULL if no error, or an error format string if an error was
 *  discovered.  To handle the error, use something like the following
 *  code:
 *
 *  	char *errstr = io_socket_parse(str, &sock);
 *  	if(errstr) {
 *  		fprintf(stderr, errstr, str);
 *  		exit(1);
 *  	}
 */

char* io_socket_parse(const char *spec, socket_addr *sock);

