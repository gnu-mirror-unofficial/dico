#ifndef __gjdict_h
#define __gjdict_h

#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>

#ifndef offsetof
# define offsetof(s,f) ((size_t)&((s*)0)->f)
#endif
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

#ifndef GD_ARG_UNUSED
# define GD_ARG_UNUSED __attribute__ ((__unused__))
#endif

#ifndef GD_PRINTFLIKE
# define GD_PRINTFLIKE(fmt,narg) __attribute__ ((__format__ (__printf__, fmt, narg)))
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

typedef void (*gjdict_log_printer_t) (int /* lvl */,
				      int /* exitcode */,
				      int /* errcode */,
				      const char * /* fmt */,
				      va_list);
void _stderr_log_printer(int, int, int, const char *, va_list);

void set_log_printer(gjdict_log_printer_t prt);
void vlogmsg(int lvl, int errcode, const char *fmt, va_list ap);
void logmsg(int lvl, int errcode, const char *fmt, ...)
    GD_PRINTFLIKE(3,4);
void die(int exitcode, int lvl, int errcode, char *fmt, ...)
    GD_PRINTFLIKE(4,5);

char * ip_hostname(IPADDR ipaddr);
IPADDR get_ipaddr(char *host);
int str2port(char *str);

int str_to_diag_level(const char *str);


void *xmalloc(size_t size);
void *xzalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);


/* Lists */
typedef struct list *dict_list_t;
typedef struct iterator *dict_iterator_t;

typedef int (*dict_list_iterator_t)(void *item, void *data);
typedef int (*dict_list_comp_t)(const void *, const void *);

dict_list_t dict_list_create(void);
void dict_list_destroy(dict_list_t *list, dict_list_iterator_t free, void *data);
void dict_list_iterate(dict_list_t list, dict_list_iterator_t itr, void *data);
void *dict_list_item(dict_list_t list, size_t n);
size_t dict_list_count(dict_list_t list);
void dict_list_append(dict_list_t list, void *data);
void dict_list_prepend(dict_list_t list, void *data);
int dict_list_insert_sorted(dict_list_t list, void *data, dict_list_comp_t cmp);

#define dict_list_push dict_list_prepend
void *dict_list_pop(dict_list_t list);

void *dict_list_locate(dict_list_t list, void *data, dict_list_comp_t cmp);
void *dict_list_remove(dict_list_t list, void *data, dict_list_comp_t cmp);

void *dict_iterator_current(dict_iterator_t itr);
dict_iterator_t dict_iterator_create(dict_list_t list);
void dict_iterator_destroy(dict_iterator_t *ip);
void *dict_iterator_first(dict_iterator_t ip);
void *dict_iterator_next(dict_iterator_t ip);

void dict_iterator_remove_current(dict_iterator_t ip);
void dict_iterator_set_data(dict_iterator_t ip, void *data);


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


struct sockaddr;
void sockaddr_to_str(const struct sockaddr *sa, int salen,
		     char *bufptr, size_t buflen,
		     size_t *plen);
char *sockaddr_to_astr(const struct sockaddr *sa, int salen);

#endif
    

int switch_to_privs (uid_t uid, gid_t gid, dict_list_t retain_groups);


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
    

