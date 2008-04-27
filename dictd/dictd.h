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
#include <syslog.h>
#include <inttypes.h>
#include <limits.h>
#include <size_max.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <gjdict.h>

extern int foreground;     /* Run in foreground mode */
extern int single_process; /* Single process mode */
extern int log_to_stderr;  /* Log to stderr */
extern char *config_file;

#define MODE_DAEMON 0
#define MODE_INETD  1

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


/* Configuration file stuff */

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

enum config_data_type {
    cfg_void,
    cfg_string,
    cfg_short,
    cfg_ushort,
    cfg_int,
    cfg_uint,
    cfg_long,
    cfg_ulong,
    cfg_size,
/*    cfg_off,*/
    cfg_uintmax,
    cfg_intmax,
    cfg_time,
    cfg_bool,
    cfg_ipv4,
    cfg_cidr,
    cfg_host,
    cfg_sockaddr,
    cfg_callback,
    cfg_section
};

#define CFG_LIST 0x8000
#define CFG_TYPE_MASK 0x00ff
#define CFG_TYPE(c) ((c) & CFG_TYPE_MASK)
#define CFG_IS_LIST(c) ((c) & CFG_LIST)

enum cfg_callback_command {
    callback_section_begin,
    callback_section_end,
    callback_set_value
};

#define TYPE_STRING 0
#define TYPE_LIST   1

typedef struct {
    int type;
    union {
	dict_list_t *list;
	const char *string;
    } v;
} config_value_t;

typedef int (*config_callback_fn) (
    enum cfg_callback_command cmd,
    gd_locus_t *       /* locus */,
    void *             /* varptr */,
    config_value_t *   /* value */,
    void *             /* cb_data */
    );

#ifndef offsetof
# define offsetof(s,f) ((size_t)&((s*)0)->f)
#endif
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

struct config_keyword {
    const char *ident;
    enum config_data_type type;
    void *varptr;
    size_t offset;
    config_callback_fn callback;
    void *callback_data;
    struct config_keyword *kwd;
};

config_value_t *config_value_dup(config_value_t *input);

typedef union {
    struct sockaddr s;
    struct sockaddr_in s_in;
    struct sockaddr_un s_un;
} sockaddr_union_t;


/* */

enum dictd_handler_type {
    handler_extern
    /* FIXME: More types to come */
};

typedef struct dictd_handler {
    char *ident;
    enum dictd_handler_type type;
    char *command;
} dictd_handler_t;

typedef struct dictd_dictionary {
    char *name;
    char *descr;
    dictd_handler_t *handler;
} dictd_dictionary_t;
