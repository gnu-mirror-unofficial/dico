/* This file is part of GNU Dico.
   Copyright (C) 1998-2000, 2008 Sergey Poznyakoff

   GNU Dico is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Dico is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
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
#include <quotearg.h>
#include <appi18n.h>

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
extern char *debug_level_str;
extern unsigned long total_forks;
extern char *access_log_format;
extern char *access_log_file;
extern int identity_check;
extern char *identity_name;
extern char *ident_keyfile;
extern long ident_timeout;

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

#define DICTD_LOGGING_ENVAR "__DICTD_LOGGING__"

#define MODE_DAEMON  0 
#define MODE_INETD   1
#define MODE_PREPROC 2

struct dicod_conf_override {
    int transcript;
};

void get_options(int argc, char *argv[], struct dicod_conf_override *conf);

typedef struct {
    char *file;
    int line;
} dicod_locus_t;

extern dicod_locus_t locus;


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
    dicod_locus_t *    /* locus */,
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

void config_error(dicod_locus_t *locus, int errcode, const char *fmt, ...)
    DICO_PRINTFLIKE(3,4);
void config_warning(dicod_locus_t *locus, int errcode, const char *fmt, ...)
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
int string_to_bool(const char *string, int *pval);
int string_to_unsigned(uintmax_t *sval, const char *string, uintmax_t maxval,
		       dicod_locus_t *loc);

void format_docstring(FILE *stream, const char *docstring, int level);
void format_statement_array(FILE *stream, struct config_keyword *kwp,
			    int n, int level);
void config_help(void);


/* acl.c */
typedef struct dicod_acl *dicod_acl_t;

dicod_acl_t dicod_acl_create(const char *name, dicod_locus_t *locus);
int dicod_acl_check(dicod_acl_t acl, int res);

int parse_acl_line(dicod_locus_t *locus, int allow, dicod_acl_t acl,
		   config_value_t *value);

int dicod_acl_install(dicod_acl_t acl, dicod_locus_t *locus);
dicod_acl_t dicod_acl_lookup(const char *name);

extern dicod_acl_t connect_acl;


/* */

typedef struct dicod_module_instance {
    char *ident;
    char *command;
    struct dico_database_module *module; 
    lt_dlhandle handle;
} dicod_module_instance_t;

#define DICOD_DBF_LANG 0x0001

typedef struct dicod_database {
    int flags;
    
    char *name;        /* Dictionary name */
    char *descr;       /* Description (SHOW DB) */
    char *info;        /* Info (SHOW INFO) */
    
    dico_list_t langlist[2]; /* List of "source/dest" languages */
    
    dicod_acl_t  acl;  /* ACL for this database */
    int visible;       /* Result of the last dicod_acl_check */
    
    dico_handle_t mod_handle;        /* Dico module handle */

    char *content_type;
    char *content_transfer_encoding;
    
    dicod_module_instance_t *instance; /* Pointer to the module instance
					  structure */
    int argc;                 /* Handler arguments: count */
    char **argv;              /*  ... and pointers */
    char *command;            /* Handler command line (for diagnostics) */
} dicod_database_t;

/* dicod.c */
void dicod_server(int argc, char **argv);
void replace_io_stream(dico_stream_t str);
int dicod_loop(dico_stream_t stream);
int dicod_inetd(void);
void dicod_init_strategies(void);
void dicod_server_init(void);
void dicod_server_cleanup(void);
int get_input_line(dico_stream_t str, char **buf, size_t *size,
		   size_t *rdbytes);

dicod_database_t *find_database(const char *name);
void database_remove_dependent(dicod_module_instance_t *inst);
void dicod_database_free(dicod_database_t *dp);
size_t database_count(void);
int database_iterate(dico_list_iterator_t fun, void *data);
int show_sys_info_p(void);
void dicod_log_setup(void);
void dicod_log_pre_setup(void);
void dicod_log_encode_envar(void);
char *get_full_hostname(void);
void check_db_visibility(void);
void reset_db_visibility(void);
#define database_visible_p(db) ((db)->visible)
int dicod_any_lang_list_p(dico_list_t list);


typedef void (*dicod_cmd_fn) (dico_stream_t str, int argc, char **argv);

struct dicod_command {
    char *keyword;
    int minparam;
    int maxparam;
    char *param;
    char *help;
    dicod_cmd_fn handler;
};

#define DICOD_MAXPARAM_INF (-1)

void dicod_handle_command(dico_stream_t str, int argc, char **argv);
void dicod_init_command_tab(void);
void dicod_add_command(struct dicod_command *cmd);
void dicod_remove_command(const char *name);

/* capa.c */
void dicod_capa_register(const char *name, struct dicod_command *cmd,
			 int (*init)(void*), void *closure);
int dicod_capa_add(const char *name);
void dicod_capa_iterate(int (*fun)(const char*, int, void *), void *closure);
int dicod_capa_flush(void);

/* lang.c */
void register_lang(void);
int dicod_lang_check(dico_list_t list[2]);

/* mime.c */
void register_mime(void);

/* markup.c */
void register_markup(void);
void markup_flush_capa(void);

/* xidle.c */
void register_xidle(void);

/* xversion.c */
void register_xversion(void);

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

typedef struct dicod_user_db *dicod_user_db_t;

extern dicod_user_db_t user_db;

void udb_init(void);
int udb_create(dicod_user_db_t *pdb,
	       const char *urlstr, const char *qpw, const char *qgrp,
	       dicod_locus_t *locus);

int udb_open(dicod_user_db_t db);
int udb_close(dicod_user_db_t db);
int udb_get_password(dicod_user_db_t db, const char *key, char **pass);
int udb_get_groups(dicod_user_db_t db, const char *key, dico_list_t *groups);
void udp_define(struct udb_def *dptr);

/* auth.c */
void register_auth(void);
void init_auth_data(void);

/* loader.c */
void dicod_loader_init(void);
int dicod_load_module(dicod_module_instance_t *hptr);
int dicod_init_database(dicod_database_t *dp);
int dicod_open_database(dicod_database_t *dp);
int dicod_close_database(dicod_database_t *dp);
int dicod_free_database(dicod_database_t *dp);

int dicod_database_get_strats(dicod_database_t *dp);

char *dicod_get_database_descr(dicod_database_t *db);
void dicod_free_database_descr(dicod_database_t *db, char *descr);
char *dicod_get_database_info(dicod_database_t *db);
void dicod_free_database_info(dicod_database_t *db, char *info);
void dicod_get_database_languages(dicod_database_t *db, dico_list_t list[]);
dico_list_t dicod_langlist_copy(dico_list_t src);

void dicod_match_word_db(dicod_database_t *db, dico_stream_t stream,
			 const dico_strategy_t strat, const char *word);
void dicod_match_word_first(dico_stream_t stream,
			    const dico_strategy_t strat, const char *word);
void dicod_match_word_all(dico_stream_t stream,
			  const dico_strategy_t strat, const char *word);
void dicod_define_word_db(dicod_database_t *db, dico_stream_t stream,
			  const char *word);
void dicod_define_word_first(dico_stream_t stream, const char *word);
void dicod_define_word_all(dico_stream_t stream, const char *word);

/* ostream.c */
extern off_t total_bytes_out;
dico_stream_t dicod_ostream_create(dico_stream_t str, const char *type,
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

/* alias.c */
int alias_install(const char *kw, int argc, char **argv, dicod_locus_t *ploc);
int alias_expand(int argc, char **argv, int *pargc, char ***pargv);

/* gsasl.c */
extern int sasl_enable;
extern dico_list_t sasl_enabled_mech;
extern dico_list_t sasl_disabled_mech;
extern char *sasl_service;
extern char *sasl_realm;
extern dico_list_t sasl_anon_groups;

void register_sasl(void);

/* stratcl.c */
enum cmp_op {
    cmp_eq,
    cmp_ne,
    cmp_lt,
    cmp_le,
    cmp_gt,
    cmp_ge
};

void stratcl_add_word(dico_list_t list, const char *word);
void stratcl_add_cmp(dico_list_t list, enum cmp_op op, size_t len);
void stratcl_add_disable(dico_list_t list);
int stratcl_check_word(dico_list_t list, const char *word);

