/* This file is part of Dico.
   Copyright (C) 1998-2000, 2008 Sergey Poznyakoff

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
#include <ltdl.h>

#include <xdico.h>
#include <c-strcase.h>

extern int mode;
extern int foreground;     /* Run in foreground mode */
extern int single_process; /* Single process mode */
extern int log_to_stderr;  /* Log to stderr */
extern char *config_file;
extern char *pidfile_name;
extern dico_list_t listen_addr;
extern uid_t user_id;
extern gid_t group_id;
extern dico_list_t /* of gid_t */ group_list;
extern unsigned int max_children;
extern unsigned int shutdown_timeout;
extern unsigned int inactivity_timeout;
extern char *hostname;
extern const char *program_version;
extern char *initial_banner_text;
extern int got_quit;
extern char *help_text;
extern dico_list_t dictionary_list;
extern const char *server_info;
extern char *msg_id;
extern dico_list_t module_load_path;
extern dico_list_t handler_list;
extern dico_list_t dictionary_list;

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
	dico_list_t list;
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
    const char *argname;
    const char *docstring;
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
    DICO_PRINTFLIKE(3,4);
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

void format_statement_array(FILE *stream, struct config_keyword *kwp,
			    int level);
void config_help(void);


/* Dictd-specific streams */
dico_stream_t fd_stream_create(int ifd, int oufd);

int stream_writez(dico_stream_t str, char *buf);
int stream_printf(dico_stream_t str, const char *fmt, ...);
void stream_write_multiline(dico_stream_t str, const char *text);


/* */

enum dictd_handler_type {
    handler_loadable
    /* FIXME: More types to come:
       handler_guile,
       handler_extern
       etc.
     */
    
};

typedef struct dictd_handler {
    char *ident;
    enum dictd_handler_type type;
    char *command;

    struct dico_handler_module *module;
    lt_dlhandle handle;
} dictd_handler_t;

typedef struct dictd_dictionary {
    char *name;
    char *descr;
    char *info;
    dictd_handler_t *handler;
} dictd_dictionary_t;

void dictd_server(int argc, char **argv);
int dictd_loop(dico_stream_t stream);
int dictd_inetd(void);
dictd_dictionary_t *find_dictionary(const char *name);
void dictionary_remove_dependent(dictd_handler_t *handler);


typedef void (*dictd_cmd_fn) (dico_stream_t str, int argc, char **argv);

struct dictd_command {
    char *keyword;
    int nparam;
    char *param;
    char *help;
    dictd_cmd_fn handler;
};

void dictd_handle_command(dico_stream_t str, int argc, char **argv);
void dictd_init_command_tab(void);
void dictd_add_command(struct dictd_command *cmd);

/* capa.c */
void dictd_capa_register(const char *name, struct dictd_command *cmd,
			 int (*init)(void*), void *closure);
int dictd_capa_add(const char *name);
void dictd_capa_iterate(int (*fun)(const char*, int, void *), void *closure);

/* user db */
struct udb_def {
    const char *proto;
    int (*_db_open) (void **, dico_url_t);
    int (*_db_close) (void *);
    int (*_db_get_password) (void *, const char *, const char *, char **);
    int (*_db_get_groups) (void *, const char *, const char *, char ***);
};

struct udb_def text_udb_def;

typedef struct dictd_user_db *dictd_user_db_t;

extern dictd_user_db_t user_db;

void udb_init(void);
int udb_create(dictd_user_db_t *pdb,
	       const char *urlstr, const char *qpw, const char *qgrp,
	       gd_locus_t *locus);

int udb_open(dictd_user_db_t db);
int udb_close(dictd_user_db_t db);
int udb_get_password(dictd_user_db_t db, const char *key, char **pass);
int udb_get_groups(dictd_user_db_t db, const char *key, char ***groups);
void udp_define(struct udb_def *dptr);

/* auth.c */
void register_auth(void);

/* loader.c */
void dictd_loader_init(void);
int dictd_load_module(dictd_handler_t *hptr);
