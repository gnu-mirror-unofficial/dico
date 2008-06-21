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
#include <inttostr.h>
#include <c-strcase.h>

#define UINTMAX_STRSIZE_BOUND INT_BUFSIZE_BOUND(uintmax_t)

#if defined HAVE_SYSCONF && defined _SC_OPEN_MAX
# define getmaxfd() sysconf(_SC_OPEN_MAX)
#elif defined (HAVE_GETDTABLESIZE)
# define getmaxfd() getdtablesize()
#else
# define getmaxfd() 256
#endif

extern int mode;
extern int foreground;     /* Run in foreground mode */
extern int single_process; /* Single process mode */
extern int log_to_stderr;  /* Log to stderr */
extern char *config_file;
extern int config_lint_option;
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
extern const char *server_info;
extern char *msg_id;
extern dico_list_t module_load_path;
extern dico_list_t modinst_list;
extern dico_list_t database_list;
extern int timing_option;
extern char *client_id;
extern char *user_name;
extern dico_list_t user_groups;
extern int transcript;
extern const char *preprocessor;
extern int debug_level;
extern char *debug_level_str;
extern unsigned long total_forks;
extern char *access_log_format;
extern char *access_log_file;
extern int identity_check;
extern char *identity_name;
extern char *ident_keyfile;

extern struct sockaddr server_addr;
extern int server_addrlen;
extern struct sockaddr client_addr;
extern int client_addrlen;

struct dico_stat {
    unsigned long defines;
    unsigned long matches;
    unsigned long compares;
};

extern struct dico_stat current_stat, total_stat;

#ifndef LOG_FACILITY
# define LOG_FACILITY LOG_LOCAL1
#endif

#define DICTD_DEFAULT_STRATEGY "lev"

#define DICT_PORT 2628
#define DICTD_LOGGING_ENVAR "__DICTD_LOGGING__"

#define MODE_DAEMON  0 
#define MODE_INETD   1
#define MODE_PREPROC 2

void get_options(int argc, char *argv[]);

typedef struct {
    char *file;
    int line;
} dictd_locus_t;

extern dictd_locus_t locus;


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
#define TYPE_ARRAY  2

typedef struct config_value {
    int type;
    union {
	dico_list_t list;
	const char *string;
	struct {
	    size_t c;
	    struct config_value *v;
	} arg;
    } v;
} config_value_t;

typedef int (*config_callback_fn) (
    enum cfg_callback_command cmd,
    dictd_locus_t *       /* locus */,
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

void config_error(dictd_locus_t *locus, int errcode, const char *fmt, ...)
    DICO_PRINTFLIKE(3,4);
void config_warning(dictd_locus_t *locus, int errcode, const char *fmt, ...)
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

void format_docstring(FILE *stream, const char *docstring, int level);
void format_statement_array(FILE *stream, struct config_keyword *kwp,
			    int n, int level);
void config_help(void);


/* acl.c */
typedef struct dictd_acl *dictd_acl_t;

dictd_acl_t dictd_acl_create(const char *name, dictd_locus_t *locus);
int dictd_acl_check(dictd_acl_t acl, int res);

int parse_acl_line(dictd_locus_t *locus, int allow, dictd_acl_t acl,
		   config_value_t *value);

int dictd_acl_install(dictd_acl_t acl, dictd_locus_t *locus);
dictd_acl_t dictd_acl_lookup(const char *name);

extern dictd_acl_t connect_acl;


/* Dictd-specific streams */
dico_stream_t fd_stream_create(int ifd, int oufd);

int stream_writez(dico_stream_t str, char *buf);
int stream_printf(dico_stream_t str, const char *fmt, ...);
void stream_write_multiline(dico_stream_t str, const char *text);


/* */

typedef struct dictd_module_instance {
    char *ident;
    char *command;
    struct dico_handler_module *module; 
    lt_dlhandle handle;
} dictd_module_instance_t;

typedef struct dictd_database {
    char *name;   /* Dictionary name */
    char *descr;  /* Description (SHOW DB) */
    char *info;   /* Info (SHOW INFO) */

    dictd_acl_t  acl;  /* ACL for this database */
    int visible;       /* Result of the last dictd_acl_check */
    
    dico_handle_t mod_handle;        /* Dico module handle */

    char *content_type;
    char *content_transfer_encoding;
    
    dictd_module_instance_t *instance; /* Pointer to the module instance
					  structure */
    int argc;                 /* Handler arguments: count */
    char **argv;              /*  ... and pointers */
    char *command;            /* Handler command line (for diagnostics) */
} dictd_database_t;

void dictd_server(int argc, char **argv);
int dictd_loop(dico_stream_t stream);
int dictd_inetd(void);
void dictd_init_strategies(void);
void dictd_server_init(void);

dictd_database_t *find_database(const char *name);
void database_remove_dependent(dictd_module_instance_t *inst);
void dictd_database_free(dictd_database_t *dp);
size_t database_count(void);
int database_iterate(dico_list_iterator_t fun, void *data);
int show_sys_info_p(void);
void dictd_log_setup(void);
void dictd_log_pre_setup(void);
void dictd_log_encode_envar(void);
char *get_full_hostname(void);
void check_db_visibility(void);
void reset_db_visibility(void);
#define database_visible_p(db) ((db)->visible)


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
int dictd_capa_flush(void);

/* mime.c */
void register_mime(void);

/* lev.c */
void register_lev(void);

/* regex.c */
void register_regex(void);

/* user db */
struct udb_def {
    const char *proto;
    int (*_db_open) (void **, dico_url_t);
    int (*_db_close) (void *);
    int (*_db_get_password) (void *, const char *, const char *, char **);
    int (*_db_get_groups) (void *, const char *, const char *, dico_list_t *);
};

struct udb_def text_udb_def;

typedef struct dictd_user_db *dictd_user_db_t;

extern dictd_user_db_t user_db;

void udb_init(void);
int udb_create(dictd_user_db_t *pdb,
	       const char *urlstr, const char *qpw, const char *qgrp,
	       dictd_locus_t *locus);

int udb_open(dictd_user_db_t db);
int udb_close(dictd_user_db_t db);
int udb_get_password(dictd_user_db_t db, const char *key, char **pass);
int udb_get_groups(dictd_user_db_t db, const char *key, dico_list_t *groups);
void udp_define(struct udb_def *dptr);

/* auth.c */
void register_auth(void);
void init_auth_data(void);

/* loader.c */
void dictd_loader_init(void);
int dictd_load_module(dictd_module_instance_t *hptr);
int dictd_init_database(dictd_database_t *dp);
int dictd_open_database(dictd_database_t *dp);
int dictd_close_database(dictd_database_t *dp);
int dictd_free_database(dictd_database_t *dp);

int dictd_database_get_strats(dictd_database_t *dp);

char *dictd_get_database_descr(dictd_database_t *db);
void dictd_free_database_descr(dictd_database_t *db, char *descr);
char *dictd_get_database_info(dictd_database_t *db);
void dictd_free_database_info(dictd_database_t *db, char *info);

void dictd_match_word_db(dictd_database_t *db, dico_stream_t stream,
			 const dico_strategy_t strat, const char *word);
void dictd_match_word_first(dico_stream_t stream,
			    const dico_strategy_t strat, const char *word);
void dictd_match_word_all(dico_stream_t stream,
			  const dico_strategy_t strat, const char *word);
void dictd_define_word_db(dictd_database_t *db, dico_stream_t stream,
			  const char *word);
void dictd_define_word_first(dico_stream_t stream, const char *word);
void dictd_define_word_all(dico_stream_t stream, const char *word);

/* ostream.c */
extern off_t total_bytes_out;
dico_stream_t dictd_ostream_create(dico_stream_t str, const char *type,
                                   const char *enc);

/* stat.c */
void begin_timing(const char *name);
void report_timing(dico_stream_t stream, xdico_timer_t t,
		   struct dico_stat *sp);
void report_current_timing(dico_stream_t stream, const char *name);

/* xscript.c */
dico_stream_t transcript_stream_create(dico_stream_t transport,
				       dico_stream_t logstr,
				       const char *prefix[]);

/* logstr.c */
dico_stream_t log_stream_create(int level);

/* getopt.m4 */
extern void print_help(void);
extern void print_usage(void);

/* pp.c */
void include_path_setup(void);
void add_include_dir(char *dir);
int pp_init(const char *name);
void pp_done(void);
int preprocess_config(const char *extpp);
void pp_make_argcv(int *pargc, const char ***pargv);
FILE *pp_extrn_start(int argc, const char **argv, pid_t *ppid);
void pp_extrn_shutdown(pid_t pid);
size_t pp_fill_buffer(char *buf, size_t size);
void run_lint();
char *install_text(const char *str);

/* accesslog.c */
void access_log_status(const char *first, const char *last);
void access_log(int argc, char **argv);
void compile_access_log(void);
void access_log_free_cache(void);

/* ident.c */
char *query_ident_name(struct sockaddr_in *srv_addr,
		       struct sockaddr_in *clt_addr);

