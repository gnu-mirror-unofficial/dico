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
#include <gettext.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include <obstack.h>
#include <quotearg.h>
#if defined(WITH_READLINE) && defined(HAVE_READLINE_READLINE_H)
# include <readline/readline.h>
# include <readline/history.h>
#endif

enum dico_client_mode {
    mode_define,
    mode_match,
    mode_dbs,
    mode_strats,
    mode_help,
    mode_info,
    mode_server
};

struct match_result {
    char *database;
    char *word;
};

struct define_result {
    char *word;
    char *database;
    char *descr;
    char *defn;
    size_t nlines;
};

enum dict_result_type {
    dict_result_define,
    dict_result_match,
    dict_result_text
};

struct dict_result {
    struct dict_result *prev;          /* Previous result */
    struct dict_connection *conn;      /* Associated connection */
    enum dict_result_type  type;       /* Result type */
    size_t count;                      /* Number of elements in the set */ 
    char *base;                        /* Base data pointer */
    union {                             
	struct define_result *def;
	struct match_result *mat;
    } set;                             /* Result set */ 
};

struct dict_connection {
    dico_stream_t str;    /* Communication stream */
    int capac;            /* Number of reported server capabilities */
    char **capav;         /* Array of capabilities */
    char *msgid;          /* msg-id */
    char *buf;            /* Input buffer */
    size_t size;          /* Buffer size */
    size_t level;         /* Buffer fill level */
    size_t levdist;       /* Configured Levenshtein distance */
    struct obstack stk;   /* Obstack for input data */
    struct dict_result *last_result; /* Last result */
};

struct auth_cred {
    char *user;
    char *pass;
};

#define DICO_CLIENT_ID PACKAGE_STRING 

extern struct dico_url dico_url;
extern struct auth_cred default_cred;
extern char *client;
extern enum dico_client_mode mode;
extern int transcript;
extern IPADDR source_addr;
extern int noauth_option;
extern unsigned levenshtein_threshold;
extern char *autologin_file;

void get_options (int argc, char *argv[], int *index);

/* connect.c */
int dict_connect(struct dict_connection **pconn, dico_url_t url);
void dict_conn_close(struct dict_connection *conn);
int dict_read_reply(struct dict_connection *conn);
int dict_status_p(struct dict_connection *conn, char *status);
int dict_capa(struct dict_connection *conn, char *capa);
int dict_multiline_reply(struct dict_connection *conn);
struct dict_result *dict_result_create(struct dict_connection *conn,
				       enum dict_result_type type,
				       size_t count, char *base);
void dict_result_free(struct dict_result *res);
#define dict_conn_last_result(c) ((c)->last_result)

/* lookup.c */
int dict_lookup_url(dico_url_t url);
int dict_word(char *word);
int dict_lookup(struct dict_connection *conn, dico_url_t url);
int dict_single_command(char *cmd, char *arg, char *code);
dico_stream_t create_pager_stream(size_t nlines);

char *get_homedir(void);

/* netrc.c */
char *skipws(char *buf);
int parse_netrc (const char *filename, char *host, struct auth_cred *pcred);

/* shell.c */
void parse_init_scripts(void);
void dico_shell(void);
void script_warning(int errcode, const char *fmt, ...);
void script_error(int errcode, const char *fmt, ...);

void ds_silent_close(void);
void ds_open(int argc, char **argv);
void ds_close(int argc, char **argv);
void ds_autologin(int argc, char **argv);
void ds_database(int argc, char **argv);
void ds_strategy(int argc, char **argv);
void ds_transcript(int argc, char **argv);
void ds_define(int argc, char **argv);
void ds_match(int argc, char **argv);
void ds_distance(int argc, char **argv);
void ds_version(int argc, char **argv);
void ds_warranty(int argc, char **argv);
