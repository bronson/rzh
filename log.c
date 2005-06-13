/* log.c
 * Scott Bronson
 * 25 Jan 2005
 * 
 * Logging infrastructure.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"


static int g_prio = 255;
static FILE *g_logfile;


void log_set_priority(int prio)
{
    // we won't bother to call setlogmask(3) since we'll handle
    // the priorities internally.

    g_prio = prio;
}


int log_get_priority()
{
    return g_prio;
}


void log_init(const char *path)
{
	g_logfile = stderr;

	if(path != NULL) {
		g_logfile = fopen(path, "a");
		if(g_logfile == NULL) {
			perror("opening log file");
			exit(99);
		}
		fprintf(g_logfile, " -=-\n");
		fflush(g_logfile);
	}
}


void log_close()
{
	if(g_logfile != NULL) {
		if(g_logfile != stderr) {
			fclose(g_logfile);
		}
	}
}


int log_msg(int prio, const char *fmt, ...)
{
	va_list ap;
	int ret;

    va_start(ap, fmt);
	ret = log_vmsg(prio, fmt, ap);
    va_end(ap);

	return ret;
}


int log_vmsg(int prio, const char *fmt, va_list ap)
{
	const char *pre;
    int ret;

	switch(prio) {
		case LOG_EMERG:
			pre = "!!!!";
			break;
		case LOG_ALERT:
			pre = "WTF!";
			break;
		case LOG_CRIT:
			pre = "CRIT";
			break;
		case LOG_ERR:
			pre = "ERR!";
			break;
		case LOG_WARNING:
			pre = "warn";
			break;
		case LOG_NOTICE:
			pre = "note";
			break;
		case LOG_INFO:
			pre = "info";
			break;
		case LOG_DEBUG:
			pre = "dbug";
			break;
		default:
			pre = "????";
	}

    if(g_prio < prio) {
        return 0;
    }

	ret = fprintf(g_logfile, "%s: ", pre);
    ret += vfprintf(g_logfile, fmt, ap);
	ret += fprintf(g_logfile, "\n");
	fflush(g_logfile);

    return ret;
}

