/* Communicates with pdpzm.
 * Scott Bronson
 * This file is in the public domain.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "pdcomm.h"


#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif


size_t pdcommWriteStdout(void *buf, size_t num)
{
	ssize_t cnt;

	cnt = write(STDOUT_FILENO, buf, num);

	// check for errors...
	if(cnt != num) {
		fprintf(stderr, "Didn't write all data, only %d of %d bytes.\n",
				(int)cnt, (int)num);
	}
	if(errno != 0) {
		fprintf(stderr, "Error when writing: %s.\n", strerror(errno));
	}

	return cnt;
}


size_t pdcommReadStdin(void *buf, size_t num)
{
	ssize_t cnt;
	cnt = read(STDIN_FILENO, buf, num);
	return cnt;
}


void DosSleep(int ct)
{
	// nothing to do...  i think...
}

