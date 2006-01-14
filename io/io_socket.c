/** @file io_socket.c
 *
 * This file adds socket functionality to whatever underlying io
 * layer you decide to use (io_epoll, io_select, etc).
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <values.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "io_socket.h"


static int set_nonblock(int sd)
{
    int flags = 1;

    // set nonblocking IO on the socket
    if(ioctl(sd, FIONBIO, &flags)) {
        // ioctl failed -- try alternative
        flags = fcntl(sd, F_GETFL, 0);
        if(flags < 0) {
            return -1;
        }
        if(fcntl(sd, F_SETFL, flags | O_NONBLOCK) < 0) {
            return -1;
        }
    }

    return 0;
}


/** Creates an outgoing connection.
 *
 * @param io An uninitialized io_atom.  This call will fill it all in.
 * @param addr The address you want to connect to.
 * @param port The port you want to connect to.
 * @param io_proc The io_proc that you want this atom to use.
 * @param flags The flags that the atom should have initially.
 * 	(IO_READ will set it up initially to watch for read events,
 * 	IO_WRITE for write events).
 * 
 * @returns The new fd (>=0) if successful, or an error (<0) if not.
 * If there was an error, you should find the reason in strerror.
 */

int io_socket_connect(io_atom *io, io_proc proc, socket_addr remote, int flags)
{   
    struct sockaddr_in sa;
    int err;
    
    io->fd = socket(AF_INET, SOCK_STREAM, 0);
    if(io->fd < 0) {
		return io->fd;
    }

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(0);
    
	err = bind(io->fd, (struct sockaddr*)&sa, sizeof(sa));
	if(err < 0) {
		goto bail;
	}

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr = remote.addr;
    sa.sin_port = htons(remote.port);
    
    err = connect(io->fd, (struct sockaddr*)&sa, sizeof(sa));
    if(err < 0) {
		goto bail;
    }
    
    if(set_nonblock(io->fd) < 0) {
		goto bail;
    }

	io->proc = proc;
    err = io_add(io, flags);
    if(err < 0) {
		goto bail;
    }

    return io->fd;

bail:
	close(io->fd);
	io->fd = -1;
	return -1;
}


/** Accepts an incoming connection.
 *
 * You should first set up a listening socket using io_listen.
 * The event proc on the listening socket should call io_accept
 * every time it gets a connection request.
 *
 * Note that this only returns the fd.  If you want to use io_atoms
 * on the connected socket, you must set up the new atom yourself.
 * This way, you're not locked into using atoms to read even if
 * you're using atoms to listen.
 *
 * @param io The socket that the incoming connection is arriving on.
 * @param remoteaddr If specified, store the address of the remote
 * computer initiating the connection here.  NULL means ignore.
 * @param remoteport If specified, store the port of the remote
 * computer initiating the connection here.  NULL means ignore.
 * @returns the new socket descriptor if we succeeded or -1 if
 * there was an error.
 */

int io_socket_accept(io_atom *io, io_proc proc, int flags, io_atom *listener, socket_addr *remote)
{
    struct sockaddr_in pin;
    socklen_t plen;
	int err;

    plen = sizeof(pin);
    while((io->fd = accept(listener->fd, (struct sockaddr*)&pin, &plen)) < 0) {
        if(errno == EINTR) {
            // This call was interrupted by a signal.  Try again and
            // see if we receive a connection.
            continue;
        }
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            // No pending connections are present (socket is nonblocking).
            return -1;
        }

        // Probably there is a network error pending for this
        // connection (already!).  Should probably just ignore it...?
        return -1;
    }

    // Excellent.  We managed to connect to the remote socket.
    if(set_nonblock(io->fd) < 0) {
        close(io->fd);
        return -1;
    }

	io->proc = proc;
    err = io_add(io, flags);
    if(err < 0) {
        close(io->fd);
        return -1;
    }

    if(remote) {
        remote->addr = pin.sin_addr;
        remote->port = (int)ntohs(pin.sin_port);
    }

    return io->fd;
}


/** Sets up a socket to listen on the given port.
 *
 * @param atom This should be the atom that will handle the events on
 * the socket.  It must be pre-allocated, and live for as long as the
 * socket will be listening.  You must fill in io_atom::proc before
 * calling this routine.  io_atom::fd will be filled in with the new
 * fd or -1 if there was an error.
 *
 * @returns the new fd.
 *
 * Call io_socket_close() to stop listening on the socket.
 */

int io_socket_listen(io_atom *io, io_proc proc, socket_addr local)
{
    struct sockaddr_in sin;

    if((io->fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		return -1;
    }

    if(set_nonblock(io->fd) < 0) {
        close(io->fd);
		return -1;
    }

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr = local.addr;
    sin.sin_port = htons(local.port);

    if(bind(io->fd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        close(io->fd);
		return -1;
    }

    // NOTE: if we're dropping connection requests under load, we
    // may need to tune the second parameter to listen.
    if (listen(io->fd, STD_LISTEN_SIZE) == -1) {
        close(io->fd);
		return -1;
    }

    io->proc = proc;
    if(io_add(io, IO_READ) < 0) {
        close(io->fd);
		return -1;
    }

	return io->fd;
}


void io_socket_close(io_atom *io)
{
	io_del(io);
	close(io->fd);
	io->fd = -1;
}


/** Reads from the given io_atom in response to an IO_READ event.
 *
 * While this was written for use by sockets there should be nothing
 * that prevents its use for regular read operations.  This routine
 * should probably be moved into io.c...?
 *
 * @param readlen returns the number of bytes you need to process.
 * If readlen is nonzero, then error is guaranteed to be 0.
 *
 * @returns 0 on success, the error code stored in errno if there was
 * an error.  If there was an error but errno doesn't say what it was
 * then -1 is returned.  A return value of -EPIPE means the remote
 * peer closed its connection.
 */

int io_socket_read(io_atom *io, char *buf, size_t cnt, size_t *readlen)
{
    ssize_t len;

	*readlen = 0;
    do {
        len = read(io->fd, buf, cnt);
    } while (errno == EINTR);   // stupid posix

    if(len > 0) {
		*readlen = len;
        return 0;
	} else if(len == 0) {
		// the remote has closed the connection.
		return -EPIPE;
    } else /* len < 0 */ {
        // an error ocurred during the read
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            // turns out there was nothing to read after all?  weird.
			// there was no error, but nothing to process either.
            return 0;
        } 

        // there's some sort of error on this socket.
        return errno ? errno : -1;
    }
}

