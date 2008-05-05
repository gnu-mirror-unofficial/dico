/* This file is part of Dico.
   Copyright (C) 2008 Sergey Poznyakoff

   Dico is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Dico is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Dico.  If not, see <http://www.gnu.org/licenses/>. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <dico.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

const char *program_name;

void
set_program_name(char *name)
{
    const char *progname;

    if (!name)
	progname = name;
    else {
	progname = strrchr(name, '/');
	if (progname)
	    progname++;
	else
	    progname = name;
	
	if (strlen(progname) > 3 && memcmp(progname, "lt-", 3) == 0)
	    progname += 3;
    }

    program_name = progname;
}

static char *prefix[] = {
    "Debug", 
    "Info",  
    "Notice", 
    "Warning",
    "Error",  
    "CRIT",   
    "ALERT",       
    "EMERG",       
};

int
str_to_diag_level(const char *str)
{
    int i;
    if (str[1] == 0 && isdigit(*str))
	return *str - '0';
    for (i = 0; i < sizeof(prefix)/sizeof(prefix[0]); i++)
	if (strcasecmp(prefix[i], str) == 0)
	    return i;
    return -1;
}

void
_stderr_log_printer(int lvl, int exitcode, int errcode,
		    const char *fmt, va_list ap)
{
    fprintf(stderr, "%s: %s: ", program_name, prefix[lvl & L_MASK]);
    vfprintf(stderr, fmt, ap);
    if (errcode)
	fprintf(stderr, ": %s", strerror(errcode));
    fprintf(stderr, "\n");
}

static dico_log_printer_t _log_printer = _stderr_log_printer;

void
set_log_printer(dico_log_printer_t prt)
{
    _log_printer = prt;
}

void
vlogmsg(int lvl, int errcode, const char *fmt, va_list ap)
{
    _log_printer(lvl, 0, errcode, fmt, ap);
}

void
logmsg(int lvl, int errcode, const char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    _log_printer(lvl, 0, errcode, fmt, ap);
    va_end(ap);
}

void
die(int exitcode, int lvl, int errcode, char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    _log_printer(lvl, exitcode, errcode, fmt, ap);
    va_end(ap);
    exit(exitcode);
}
