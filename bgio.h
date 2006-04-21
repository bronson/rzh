/* bgio.h
 * Scott Bronson
 *
 * Starts up background I/O.
 * This file is MIT licensed.
 */


extern int st_child_pid;	// TODO: get rid of me!

// Opens a pty and forks the child process specified by bgio_subshell_command.
int bgio_start();
// Shuts down everything started by bgio_start, then exits.
void bgio_stop();
// closes the fds used by bgio but doesn't deallocate any memory
void bgio_close();

int bgio_get_window_width();

