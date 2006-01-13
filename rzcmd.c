/* rzcmd.c
 * 12 Jan 2006
 * Scott Bronson
 *
 * Code used to specify an alternative rz command on the cmdline.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "rzcmd.h"


const char *cmd_name;		// name of rz utility (argv[0])
const char *cmd_exec;		// executable of rz utility
const char **cmd_args;		// arguments to pass to rz utility



// Calls the given function once for each individual token found
// in the given string.  Right now this routine doesn't handle
// quoting or backslashing though ideally it should.

static int for_tokens(const char *cp, void (*proc)(int tokno, const char *str, int len))
{
	const char *tok;
	int cnt = 0;

	for(;;) {
		// skip any whitespace before the token
		while(*cp != '\0' && isspace(*cp)) {
			cp += 1;
		}

		if(!*cp) {
			break;
		}
		tok = cp;

		// scan past token
		while(*cp != '\0' && !isspace(*cp)) {
			cp += 1;
		}

		// we have a token, send it to the proc
		if(proc) {
			(*proc)(cnt, tok, cp-tok);
		}
		cnt += 1;
	}

	return cnt;
}


// Parses the command name ("ls") out of the executable path ("/bin/ls").

static void cmd_name_from_exec()
{
	const char *name = (const char*)strrchr(cmd_exec, '/');
	if(name == NULL) {
		name = cmd_exec;
	} else {
		name += 1;	// skip the slash
	}

	if(cmd_name) {
		free((void*)cmd_name);
	}

	cmd_name = strdup(name);
}


// Can't rely on strndup on all platforms
// note that this ALWAYS allocates and copies len bytes, even
// if the string is shorter (contrary to the built-in strndup).

static char* my_strndup(const char *str, int len)
{
	char *cp = malloc(len+1);
	if(cp) {
		memcpy(cp, str, len);
		cp[len] = '\0';
	}

	return cp;
}


// Stores the given token into the pre-allocated argument list.

static void store_proc(int tokno, const char *str, int len)
{
	cmd_args[tokno] = my_strndup(str, len);
}


void parse_rz_cmd(const char *str)
{
	const char *cp = str;
	int cnt;

	// skip any whitespace before the command
	while(*cp != '\0' && isspace(*cp)) {
		cp += 1;
	}

	if(!*cp) {
		// leave old command unchanged because there was no
		// new one specified.
		fprintf(stderr, "Warning: null command specified for rz!\n");
		return;
	}

	// beginning of the command
	str = cp;

	// scan past token
	while(*cp != '\0' && !isspace(*cp)) {
		cp += 1;
	}
	
	free((void*)cmd_exec);
	cmd_exec = my_strndup(str, cp-str);
	cmd_name_from_exec();

	cnt = for_tokens(cp, NULL);
	free(cmd_args);
	cmd_args = malloc((cnt+1) * sizeof(char*));
	for_tokens(cp, store_proc);
	cmd_args[cnt] = NULL;
}


void print_rz_cmd()
{
	int i;

	printf("receive command: ('%s', '%s'", cmd_exec, cmd_name);
	for(i=0; cmd_args[i] != NULL; i++) {
		printf(", '%s'", cmd_args[i]);
	}
	printf(")\n");
}


void init_rz_cmd()
{
	// initialize the command we'll use to receive the zmodem.
	cmd_exec = strdup("/usr/bin/rz");
	cmd_name_from_exec();
	cmd_args = malloc(sizeof(char*));
	cmd_args[0] = NULL;
}


void free_rz_cmd()
{
	int i;

	free((void*)cmd_exec);
	free((void*)cmd_name);
	for(i=0; cmd_args[i] != NULL; i++) {
		free((void*)cmd_args[i]);
	}
	free((void*)cmd_args);
}

