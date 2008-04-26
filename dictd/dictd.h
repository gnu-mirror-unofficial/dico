/* This file is part of Gjdict.
   Copyright (C) 1998-2000, 2008 Sergey Poznyakoff

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <xalloc.h>
#include <ctype.h>    
#include <gjdict.h>

extern int foreground;     /* Run in foreground mode */
extern int single_process; /* Single process mode */
extern int log_to_stderr;  /* Log to stderr */
extern char *config_file;

void get_options(int argc, char *argv[]);

typedef struct {
    char *file;
    int line;
} gd_locus_t;

extern gd_locus_t locus;

#ifndef GD_ARG_UNUSED
# define GD_ARG_UNUSED __attribute__ ((__unused__))
#endif

#ifndef GD_PRINTFLIKE
# define GD_PRINTFLIKE(fmt,narg) __attribute__ ((__format__ (__printf__, fmt, narg)))
#endif

void config_error(gd_locus_t *locus, int errcode, const char *fmt, ...)
    GD_PRINTFLIKE(3,4);
int config_lex_begin(const char *name);
void config_lex_end(void);
int config_parse(const char *name);
void config_gram_trace(int);
void config_lex_trace(int);
void line_begin(void);
void line_add(char *text, size_t len);
void line_add_unescape_last(char *text, size_t len);
void line_finish(void);
char *line_finish0(void);
