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

#ifndef __xdico_h
#define __xdico_h

#include <dico.h>

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

dico_list_t xdico_list_create(void);
dico_iterator_t xdico_iterator_create(dico_list_t list);
void xdico_list_append(struct list *list, void *data);
void xdico_list_prepend(struct list *list, void *data);
dico_assoc_list_t xdico_assoc_create(void);
void xdico_assoc_add(dico_assoc_list_t assoc, const char *key,
		     const char *value);

#endif
    
