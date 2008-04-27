#ifndef __gjdict_h
#define __gjdict_h

#include <stdlib.h>
#include <stdarg.h>

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
void logmsg(int lvl, int errcode, const char *fmt, ...);
void die(int exitcode, int lvl, int errcode, char *fmt, ...);

char * ip_hostname(IPADDR ipaddr);
IPADDR get_ipaddr(char *host);
int str2port(char *str);

int str_to_diag_level(const char *str);


void *xmalloc(size_t size);
void *xzalloc(size_t size);
void *xcalloc(size_t nmemb, size_t size);
void *xrealloc(void *ptr, size_t size);


/* Lists */
typedef struct list dict_list_t;
typedef struct iterator dict_iterator_t;

typedef int (*dict_list_iterator_t)(void *item, void *data);
typedef int (*dict_list_comp_t)(const void *, const void *);

dict_list_t *dict_list_create(void);
void dict_list_destroy(dict_list_t **list, dict_list_iterator_t free, void *data);
void dict_list_iterate(dict_list_t *list, dict_list_iterator_t itr, void *data);
void *dict_list_item(dict_list_t *list, size_t n);
size_t dict_list_count(dict_list_t *list);
void dict_list_append(dict_list_t *list, void *data);
void dict_list_prepend(dict_list_t *list, void *data);
int dict_list_insert_sorted(struct list *list, void *data, dict_list_comp_t cmp);

#define dict_list_push dict_list_prepend
void *dict_list_pop(struct list *list);

void *dict_list_locate(dict_list_t *list, void *data, dict_list_comp_t cmp);
void *dict_list_remove(dict_list_t *list, void *data, dict_list_comp_t cmp);

void *dict_iterator_current(dict_iterator_t *ip);
dict_iterator_t *dict_iterator_create(dict_list_t *list);
void dict_iterator_destroy(dict_iterator_t **ip);
void *dict_iterator_first(dict_iterator_t *ip);
void *dict_iterator_next(dict_iterator_t *ip);

void dict_iterator_remove_current(dict_iterator_t *ip);
void dict_iterator_set_data(dict_iterator_t *ip, void *data);


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

#endif
    




