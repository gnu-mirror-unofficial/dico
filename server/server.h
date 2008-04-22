#include <stdio.h>
#include "gjdict.h"
#include "dict.h"

#define ENC_JIS 0
#define ENC_EUC 1

#define NITEMS(a) sizeof(a)/sizeof((a)[0])

struct searchctl_t {
    int tree;
    int mode;
    Matchdir dir;
    int count;
};

extern int encoding;
extern char * answer_str[];
extern char * match_str[];
extern char * tree_str[];
extern char * dir_str[];
extern int binary;
extern struct searchctl_t search_ctl;
extern long inactivity_timeout;
extern IPADDR ipaddr;
extern unsigned max_children;
extern char *portstr;
extern int single_user;
extern int inetd;

extern char usage[];
extern char licence[];
extern char version[];

/* main.c */
void set_inactivity_timeout(char *str);

/* gram.y */
int yyparse(void);
int yylex(void);
int yyerror(char *);
void enable_yydebug(void);

/* cmdline.h */
void get_options (int argc, char *argv[]);

/* server.c */
int set_fd_ctl_addr(int fd);
void set_lo_ctl_addr(void);
int server(FILE *in, FILE *out);
void active(int a1, int a2, int a3, int a4, int p1, int p2);
int passive(void);
int opendata(void);
void closedata(void);
void xlat_entry(DictEntry *dst, DictEntry *src);
void send_entries(DictEntry *entry, int cnt);
int send_recno(int cnt, Recno *buf);
int quit(int code);
void cmsg(int num, char *fmt, ...);
void msg(int num, char *fmt, ...);
void udata(char *fmt, ...);	
void getstr(Offset offset);
void find(char *str);
void server_stat(void);

/* search.c */
void initsearch(void);
void set_mode(int match_mode, Matchdir dir, int cnt);
int simple_query(void *data, int size);
void respond(void *data, int size);
void xref(int code);
void search_string16(char *str);
void search_string32(char *str);
void search_number32(char *str);
void search_number16(char *str);
void search_number8(char *str);
void search_corner(char *str);
void search_bushu(char *str);
void search_skip(char *str);
void getrecord(Recno recno);

/* utils.c */
char * mkfullname(char *dir, char *name);
void format_skipcode(char *buf, unsigned code);

/* dict.c */
void makenames(void);
void initdict(void);
int have_tree(int n);
char * tree_name(int n);
Index * tree_ptr(int n);
Recno tree_position(int n);
DictEntry * dict_entry(DictEntry *entry, Recno recno);
void * dict_string(Offset off);
void dict_setcmplen(int tree_no, int len);
void dict_setcmpcut(int tree_no, int num);
DictEntry * read_match_entry(DictEntry *entry, int tree_no, void *value, Matchdir dir);
DictEntry * read_closest_entry(DictEntry *entry, int tree_no, void *value);
DictEntry * read_exact_match_entry(DictEntry *entry, int tree_no, void *value);
Recno dict_index(int tree_no, void *value);
Recno next_dict_index(int tree_no, void *value);
void dict_enum(int tree_no, int (*fun)());

/* log.c */
void syslog_log_printer(int lvl, int exitcode, int errcode,
			const char *fmt, va_list ap);

#define DEBUG_GRAM         0
#define DEBUG_SEARCH       1
#define DEBUG_COMM         2
#define NUMDEBUG           3

extern int console_log;
extern int debug_level[];

#define debug(c,l)\
 if (debug_level[c] >= l) dlog

void initlog(void);
void set_debug_level(char *str);
void dlog(char *fmt, ...);
