/* bgio.h
 * Scott Bronson
 *
 * Starts up background I/O.
 * This file is MIT licensed.
 */

#include <termios.h>


typedef struct {
	int master;				// the fd of the master
	int slave;					// the fd of the slave
	int child_pid;					// the pid of the subprocess
	struct termios stdin_termios;	// original termios to restore when finished
} bgio_state;


// Opens a pty and forks the child process specified by bgio_subshell_command.
void bgio_start(bgio_state *state, const char *cmd);

// Shuts down everything started by bgio_start, then exits.
void bgio_stop(bgio_state *state, int code);


// result codes returned by exit().
// actually, these are mostly used by main, not bgio

enum {
	argument_error=1,
	chdir_error=2,
	fork_error1=5,
	fork_error2=6,
	fork_error3=7,
	process_error=10,
};

