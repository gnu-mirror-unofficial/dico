#ifndef __dico_h
#define __dico_h

#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>

#ifndef offsetof
# define offsetof(s,f) ((size_t)&((s*)0)->f)
#endif
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

#ifndef DICO_ARG_UNUSED
# define DICO_ARG_UNUSED __attribute__ ((__unused__))
#endif

#ifndef DICO_PRINTFLIKE
# define DICO_PRINTFLIKE(fmt,narg) __attribute__ ((__format__ (__printf__, fmt, narg)))
#endif


typedef unsigned long UINT4;
typedef UINT4 IPADDR;

#define L_DEBUG     0
#define L_INFO      1
#define L_NOTICE    2
#define L_WARN      3
#define L_ERR       4
#define L_CRIT      5
#define L_ALERT     6
#define L_EMERG     7

#define L_MASK      0xff

#define L_CONS      0x8000

extern const char *program_name;
void set_program_name(char *name);

typedef void (*dico_log_printer_t) (int /* lvl */,
			            int /* exitcode */,
				    int /* errcode */,
				    const char * /* fmt */,
				    va_list);
void _stderr_log_printer(int, int, int, const char *, va_list);

void set_log_printer(dico_log_printer_t prt);
void vlogmsg(int lvl, int errcode, const char *fmt, va_list ap);
void logmsg(int lvl, int errcode, const char *fmt, ...)
    DICO_PRINTFLIKE(3,4);
void die(int exitcode, int lvl, int errcode, char *fmt, ...)
    DICO_PRINTFLIKE(4,5);

char * ip_hostname(IPADDR ipaddr);
IPADDR get_ipaddr(char *host);
int str2port(char *str);

int str_to_diag_level(const char *str);


void *xmalloc(size_t size);
void *xzalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);


/* Lists */
typedef struct list *dico_list_t;
typedef struct iterator *dico_iterator_t;

typedef int (*dico_list_iterator_t)(void *item, void *data);
typedef int (*dico_list_comp_t)(const void *, const void *);

dico_list_t dico_list_create(void);
void dico_list_destroy(dico_list_t *list, dico_list_iterator_t free, void *data);
void dico_list_iterate(dico_list_t list, dico_list_iterator_t itr, void *data);
void *dico_list_item(dico_list_t list, size_t n);
size_t dico_list_count(dico_list_t list);
void dico_list_append(dico_list_t list, void *data);
void dico_list_prepend(dico_list_t list, void *data);
int dico_list_insert_sorted(dico_list_t list, void *data, dico_list_comp_t cmp);
dico_list_t  dico_list_intersect(dico_list_t a, dico_list_t b,
				 dico_list_comp_t cmp);

#define dico_list_push dico_list_prepend
void *dico_list_pop(dico_list_t list);

void *dico_list_locate(dico_list_t list, void *data, dico_list_comp_t cmp);
void *dico_list_remove(dico_list_t list, void *data, dico_list_comp_t cmp);

void *dico_iterator_current(dico_iterator_t itr);
dico_iterator_t dico_iterator_create(dico_list_t list);
void dico_iterator_destroy(dico_iterator_t *ip);
void *dico_iterator_first(dico_iterator_t ip);
void *dico_iterator_next(dico_iterator_t ip);

void dico_iterator_remove_current(dico_iterator_t ip);
void dico_iterator_set_data(dico_iterator_t ip, void *data);


/* Association lists */
struct dico_assoc {
    char *key;
    char *value;
};

typedef dico_list_t dico_assoc_list_t;

dico_assoc_list_t dico_assoc_create(void);
void dico_assoc_destroy(dico_assoc_list_t *passoc);
void dico_assoc_add(dico_assoc_list_t assoc, const char *key, const char *value);
const char *dico_assoc_find(dico_assoc_list_t assoc, const char *key);
void dico_assoc_remove(dico_assoc_list_t assoc, const char *key);



/* Simple translation tables */
struct xlat_tab {
    char *string;
    int num;
};

#define XLAT_ICASE 0x01

int xlat_string(struct xlat_tab *tab, const char *string, size_t len,
		int flags, int *result);
int xlat_c_string(struct xlat_tab *tab, const char *string, int flags,
		  int *result);


/* URLs */
struct dico_url {
    char *string;
    char *proto;
    char *host;
    char *path;
    char *user;
    char *passwd;
    dico_assoc_list_t args;
};

typedef struct dico_url *dico_url_t;
int dico_url_parse(dico_url_t *purl, const char *str);
void dico_url_destroy(dico_url_t *purl);
const char *dico_url_get_arg(dico_url_t url, const char *argname);
char *dico_url_full_path(dico_url_t url);



struct sockaddr;
void sockaddr_to_str(const struct sockaddr *sa, int salen,
		     char *bufptr, size_t buflen,
		     size_t *plen);
char *sockaddr_to_astr(const struct sockaddr *sa, int salen);


int switch_to_privs (uid_t uid, gid_t gid, dico_list_t retain_groups);


size_t utf8_char_width(const unsigned char *p);
size_t utf8_strlen (const char *s);
size_t utf8_strbytelen (const char *s);

struct utf8_iterator {
    char *string;
    char *curptr;
    unsigned curwidth;
};

#define utf8_iter_isascii(itr) \
 ((itr).curwidth == 1 && isascii((itr).curptr[0]))

int utf8_iter_end_p(struct utf8_iterator *itr);
int utf8_iter_first(struct utf8_iterator *itr, unsigned char *ptr);
int utf8_iter_next(struct utf8_iterator *itr);

int utf8_mbtowc_internal (void *data, int (*read) (void*), unsigned int *pwc);
int utf8_wctomb (unsigned char *r, unsigned int wc);

unsigned utf8_wc_toupper (unsigned wc);
int utf8_toupper (char *s, size_t len);
unsigned utf8_wc_tolower (unsigned wc);
int utf8_tolower (char *s, size_t len);
size_t utf8_wc_strlen (const unsigned *s);
unsigned *utf8_wc_strdup (const unsigned *s);
size_t utf8_wc_hash_string (const unsigned *ws, size_t n_buckets);
int utf8_wc_strcmp (const unsigned *a, const unsigned *b);
int utf8_wc_to_mbstr(const unsigned *wordbuf, size_t wordlen, char *s,
		     size_t size);
    


/* Utility functions */
char *make_full_file_name(const char *dir, const char *file);
void trimnl(char *buf, size_t len);

#endif
    
