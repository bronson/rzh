/* rzcmd.c
 * 12 Jan 2006
 * Scott Bronson
 *
 * Code used to parse commands out of the command line.
 * (i.e. 'rzh --rz="lrz --disable-timeout"').
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "cmd.h"


// Calls the given function once for each individual token found
// in the given string.  Right now this routine doesn't handle
// quoting or backslashing though ideally it should.

static int for_tokens(command *cmd, const char *cp, void (*proc)(command *cmd, int tokno, const char *str, int len))
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
			(*proc)(cmd, cnt, tok, cp-tok);
		}
		cnt += 1;
	}

	return cnt;
}


// Parses the command name ("ls") out of the executable path ("/bin/ls").

static char* cmd_name_from_path(const char *path)
{
	const char *name = (const char*)strrchr(path, '/');

	if(name == NULL) {
		name = path;
	} else {
		name += 1;	// skip the slash
	}

	return strdup(name);
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

static void store_proc(command *cmd, int tokno, const char *str, int len)
{
	cmd->args[tokno+1] = my_strndup(str, len);
}


void cmd_parse(command *cmd, const char *str)
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

	cmd_free(cmd);
	cmd->path = my_strndup(str, cp-str);

	cnt = for_tokens(cmd, cp, NULL);
	cnt += 2;	// space for argv[0] and the NULL terminator
	cmd->args = malloc(cnt * sizeof(char*));

	cmd->args[0] = cmd_name_from_path(cmd->path);
	if(for_tokens(cmd, cp, store_proc) != cnt-2) {
		assert(!"counts didn't match!");
	}
	cmd->args[cnt-1] = NULL;
}


void cmd_print(command *cmd)
{
	int i;

	printf("receive command: ('%s': ", cmd->path);
	for(i=0; cmd->args[i] != NULL; i++) {
		printf(", '%s'", cmd->args[i]);
	}
	printf(")\n");
}


void cmd_init(command *cmd)
{
	cmd->path = NULL;
	cmd->args = NULL;
}


void cmd_free(command *cmd)
{
	int i;

	if(cmd->path) {
		free(cmd->path);
	}

	if(cmd->args) {
		for(i=0; cmd->args[i] != NULL; i++) {
			free((void*)cmd->args[i]);
		}
		free(cmd->args);
	}
}

