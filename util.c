#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "util.h"
#include "log.h"


int g_highest_fd;


int find_highest_fd()
{
	int i, err;

	// Assume we'll never need more than 64 fds
	for(i=64; i; i--) {
		err = fcntl(i, F_GETFL);
		if(err != -1) {
			return i;
		}
	}
	
	assert(!"WTF?  No FDs??");
	return 0;
}


void fdcheck()
{
	int now = find_highest_fd();
	if(now != g_highest_fd) {
		fprintf(stderr, "ERROR: On forking, highest fd should be %d but it's %d\n", g_highest_fd, now);
		log_err("ERROR: On forking, highest fd should be %d but it's %d", g_highest_fd, now);
		// Keep running because it's not a fatal error.
	}
}

