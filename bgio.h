/* bgio.h
 * Scott Bronson
 *
 * Starts up background I/O.
 * This file is MIT licensed.
 */

// Command to run in child.  If NULL, starts the default shell.
extern char *bgio_subshell_command;

// Need to call start_bgio before these are valid.
extern int bgio_master;
extern int bgio_slave;

// Opens a pty and forks the child process specified by bgio_subshell_command.
void bgio_start();

// Shuts down everything started by bgio_start, then exits.
void bgio_stop(int code);


// result codes returned by exit().
// actually, htese are mostly used by main, not bgio
// but bgio needs to access the fork_errors.

enum {
	argument_error=1,
	chdir_error=2,
	fork_error1=5,
	fork_error2=6,
	fork_error3=7,
	process_error=10,
};

