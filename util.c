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
		fprintf(stderr, "On forking, highest fd should be %d but it's %d", g_highest_fd, now);
		bail(67);
	}
}


/** This signal handler is made somewhat difficult because it must handle
 *  two types of sigchlds: one if we're stopping an rz, and one if the
 *  slave is quitting.
 */

void sigchild(int tt)
{
	int pid;
	int status;

	log_dbg("Got sigchld");

	// Reap as many children as we can.
	for(;;) {
		pid = waitpid(-1, &status, WNOHANG);
		log_dbg(" ... pid=%d status=%d", pid, status);
		if(pid == 0) {
			log_wtf("waitpid returned 0?!");
			break;
		}
		if(pid < 0) {
			// no more children needed waiting
			break;
		}

		if (pid == bgio_child_pid) {
			stop_bgio_child();
		} else if(pid == rz_child_pid) {
			stop_rz_child();
		} else {
			log_wtf("Unknown pid %d!?", pid);
		}
	}
}

