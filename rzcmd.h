/* rzcmd.h
 * 12 Jan 2006
 * Scott Bronson
 *
 * Code used to specify an alternative rz command on the cmdline.
 */

extern const char *cmd_name;		// name of rz utility (argv[0])
extern const char *cmd_exec;		// executable of rz utility
extern const char **cmd_args;		// arguments to pass to rz utility


void parse_rz_cmd(const char *str);
void print_rz_cmd();
void init_rz_cmd();
void free_rz_cmd();

