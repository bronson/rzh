/* bgio.h
 * Scott Bronson
 *
 * Starts up background I/O.
 */

// Command to run in child.  If NULL, starts the default shell.
extern char *bgio_subshell_command;

// Need to call start_bgio before these are valid.
extern int bgio_master;
extern int bgio_slave;

// Opens a pty and forks the child process specified by bgio_subshell_command.
void bgio_start();

// Shuts down everything started by bgio_start, then exits.
void bgio_stop();

