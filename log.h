/* log.h
 * Scott Bronson
 * 25 Jan 2005
 * 
 * Logging infrastructure.
 */

#include <syslog.h>
#include <stdarg.h>

void log_set_priority(int prio);
int log_get_priority();

void log_init(const char *file);
void log_close();

// we only use logerr, logwarn, loginfo, and logdebug.
#define log_emerg(...) log_msg(LOG_EMERG, __VA_ARGS__)
#define log_emergency(...) log_msg(LOG_EMERG, __VA_ARGS__)
// I've usurped LOG_ALERT to mean "wtf?!"
//#define log_alert(...) log_msg(LOG_ALERT, __VA_ARGS__)
#define log_wtf(...) log_msg(LOG_ALERT, __VA_ARGS__)
#define log_crit(...) log_msg(LOG_CRIT, __VA_ARGS__)
#define log_critical(...) log_msg(LOG_CRIT, __VA_ARGS__)
#define log_err(...) log_msg(LOG_ERR, __VA_ARGS__)
#define log_error(...) log_msg(LOG_ERR, __VA_ARGS__)
#define log_warn(...) log_msg(LOG_WARNING, __VA_ARGS__)
#define log_warning(...) log_msg(LOG_WARNING, __VA_ARGS__)
#define log_note(...) log_msg(LOG_NOTICE, __VA_ARGS__)
#define log_info(...) log_msg(LOG_INFO, __VA_ARGS__)
#define log_dbg(...) log_msg(LOG_DEBUG, __VA_ARGS__)
#define log_debug(...) log_msg(LOG_DEBUG, __VA_ARGS__)


int log_msg(int priority, const char *fmt, ...);
int log_vmsg(int priority, const char *fmt, va_list ap);

