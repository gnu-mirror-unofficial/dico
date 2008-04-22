/*
 * log.c	Logging module.
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <syslog.h>
#include <stdarg.h>
#include <errno.h>
#include <server.h>

#ifndef LOGFACILITY
# define LOGFACILITY LOG_LOCAL4
#endif

int console_log;
int debug_level[NUMDEBUG];

void
initlog()
{
    /* FIXME: 3rd arg should be configurable */
    openlog(program_name, LOG_PID, LOGFACILITY);
}

void
set_debug_level(char *str)
{
    int fac, lev;
    char *p;
    
    fac = strtol(str, &p, 10);
    if (*p != '.') {
	logmsg(L_ERR|L_CONS, 0, "bad debug format (near `%s')", p);
	return;
    }
    if (fac < 0 || fac > NUMDEBUG) {
	logmsg(L_ERR|L_CONS, 0, "bad debug facility: %d", fac);
	return;
    }
    lev = strtol(p+1, &p, 10);
    if (*p != 0) {
	logmsg(L_ERR|L_CONS, 0, "bad debug level (near `%s')", p);
	return;
    }
    debug_level[fac] = lev;
    if (fac == DEBUG_GRAM && lev >= 10)
	enable_yydebug();
}


void
syslog_log_printer(int lvl, int exitcode, int errcode,
		   const char *fmt, va_list ap)
{
    char *s;
    int prio = LOG_INFO;
    int cons = console_log;
    static struct {
	char *prefix;
	int priority;
    } loglevels[] = {
	{ "Debug",  LOG_DEBUG },
	{ "Info",   LOG_INFO },
	{ "Notice", LOG_NOTICE },
	{ "Warning",LOG_WARNING },
	{ "Error",  LOG_ERR },
	{ "CRIT",   LOG_CRIT },
	{ "ALERT",  LOG_ALERT },     
	{ "EMERG",  LOG_EMERG },      
    };
    char buf[512];
    int level = 0;
    
    if (lvl & L_CONS)
	cons++;
    if (cons)
	_stderr_log_printer(lvl, exitcode, errcode, fmt, ap);
    
    s    = loglevels[lvl & L_MASK].prefix;
    prio = loglevels[lvl & L_MASK].priority;

    /* FIXME: This should be conditional */
    level += snprintf(buf + level, sizeof(buf) - level, "%s: ", s);
    level += vsnprintf(buf + level, sizeof(buf) - level, fmt, ap);
    if (errcode)
	level += snprintf(buf + level, sizeof(buf) - level, ": %s",
			  strerror(errcode));
    syslog(prio, "%s", buf);
}

void
dlog(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    syslog_log_printer(L_DEBUG, 0, 0, fmt, ap);
    va_end(ap);
}


