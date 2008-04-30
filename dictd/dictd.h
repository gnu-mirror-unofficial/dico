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
#include <time.h>
#include <unistd.h>
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
#include <signal.h>

#include <gjdict.h>
#include <c-strcase.h>

extern int mode;
extern int foreground;     /* Run in foreground mode */
extern int single_process; /* Single process mode */
extern int log_to_stderr;  /* Log to stderr */
extern char *config_file;
extern char *pidfile_name;
extern dict_list_t listen_addr;
extern uid_t user_id;
extern gid_t group_id;
extern dict_list_t /* of gid_t */ group_list;
extern unsigned int max_children;
extern unsigned int shutdown_timeout;
extern char *hostname;
extern const char *program_version;
extern char *initial_banner_text;
extern int got_quit;
extern char *help_text;

#ifndef LOG_FACILITY
# define LOG_FACILITY LOG_LOCAL1
#endif

#define DICT_PORT 2628

#define MODE_DAEMON 0
#define MODE_INETD  1

void get_options(int argc, char *argv[]);

typedef struct {
    char *file;
    int line;
} gd_locus_t;

extern gd_locus_t locus;


/* Configuration file stuff */

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
	dict_list_t list;
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

int yylex(void);
int yyerror(char *); 

void config_error(gd_locus_t *locus, int errcode, const char *fmt, ...)
    GD_PRINTFLIKE(3,4);
int config_lex_begin(const char *name);
void config_lex_end(void);
void config_set_keywords(struct config_keyword *kwd);
int config_parse(const char *name);
void config_gram_trace(int);
void config_lex_trace(int);
void line_begin(void);
void line_add(char *text, size_t len);
void line_add_unescape_last(char *text, size_t len);
void line_finish(void);
char *line_finish0(void);
int quote_char(int c);
int unquote_char(int c);


/* Line buffer */
typedef struct _line_buffer *linebuf_t;
enum line_buffer_type { lb_in, lb_out };
typedef struct stream *stream_t;

int linebuf_create(linebuf_t *s, stream_t stream,
		   enum line_buffer_type type, size_t size);
void linebuf_destroy(linebuf_t *s);
void linebuf_drop(linebuf_t s);

int linebuf_grow(linebuf_t s, const char *ptr, size_t size);
size_t linebuf_read(linebuf_t s, char *ptr, size_t size);
int linebuf_readline(linebuf_t s, char *ptr, size_t size);
int linebuf_write(linebuf_t s, char *ptr, size_t size);
int linebuf_writelines(linebuf_t s);
size_t linebuf_level(linebuf_t s);
char *linebuf_data(linebuf_t s);
int linebuf_flush(linebuf_t s);


/* Streams */

stream_t fd_stream_create(int ifd, int oufd);

int stream_create(stream_t *pstream,
		  void *data, 
		  int (*readfn) (void *, char *, size_t, size_t *),
		  int (*writefn) (void *, char *, size_t, size_t *),
		  int (*closefn) (void *));

void stream_set_error_string(stream_t stream,
			     const char *(*error_string) (void *, int));

int stream_set_buffer(stream_t stream, enum line_buffer_type type,
		      size_t size);

int stream_read_unbuffered(stream_t stream, char *buf, size_t size,
			   size_t *pread);
int stream_write_unbuffered(stream_t stream, char *buf, size_t size,
			    size_t *pwrite);

int stream_read(stream_t stream, char *buf, size_t size, size_t *pread);
int stream_readln(stream_t stream, char *buf, size_t size, size_t *pread);
int stream_getline(stream_t stream, char **pbuf, size_t *psize, size_t *pread);
int stream_write(stream_t stream, char *buf, size_t size);
int stream_writeln(stream_t stream, char *buf, size_t size);

const char *stream_strerror(stream_t stream, int rc);
int stream_last_error(stream_t stream);
void stream_clearerr(stream_t stream);
int stream_eof(stream_t stream);


int stream_flush(stream_t stream);
int stream_close(stream_t stream);
void stream_destroy(stream_t *stream);

/* Dictd-specific streams */
int stream_writez(stream_t str, char *buf);
int stream_printf(stream_t str, const char *fmt, ...);


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
    char *info;
    dictd_handler_t *handler;
} dictd_dictionary_t;

int dictd_loop(stream_t stream);
int dictd_inetd(void);


typedef void (*dictd_cmd_fn) (stream_t str, int argc, char **argv);

struct dictd_command {
    const char *keyword;
    int minargs;
    int maxargs;
    dictd_cmd_fn handler;
};

void dictd_handle_command(stream_t str, int argc, char **argv);
