/* cmd.h
 * 12 Jan 2006
 * Scott Bronson
 *
 * Code used to parse commands out of the command line.
 * (i.e. 'rzh --rz="lrz --disable-timeout"').
 */

typedef struct {
	char *path;
	char **args;
} command;


void cmd_init(command *cmd);
void cmd_parse(command *cmd, const char *str);
void cmd_print(const char *str, command *cmd);
void cmd_free(command *cmd);


extern command rzcmd, shellcmd;		// there must be a better place for this.

