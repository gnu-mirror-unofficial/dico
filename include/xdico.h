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


/* Timer */
typedef struct timer_slot *xdico_timer_t;

xdico_timer_t timer_get(const char *name);
xdico_timer_t timer_start(const char *name);
xdico_timer_t timer_stop(const char *name);
xdico_timer_t timer_reset(const char *name);
double timer_get_real(xdico_timer_t t);
double timer_get_user(xdico_timer_t t);
double timer_get_system(xdico_timer_t t);
void timer_format_time(dico_stream_t stream, double t);


#endif
    
