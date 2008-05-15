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

#ifndef __dico_types_h
#define __dico_types_h

#include <sys/types.h>

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

#define __dico_s_cat3__(a,b,c) a ## b ## c
#define DICO_EXPORT(module,name) __dico_s_cat3__(module,_LTX_,name)

typedef struct dico_line_buffer *dico_linebuf_t;
typedef struct dico_stream *dico_stream_t;

typedef struct dico_handle_struct *dico_handle_t;
typedef struct dico_result_struct *dico_result_t;
typedef int (*dico_select_t) (const char *, const char *, void *);

struct dico_handler_module {
    int (*module_init) (int argc, char **argv);
    dico_handle_t (*module_open) (const char *db, int argc, char **argv);
    int (*module_close) (dico_handle_t hp);
    char *(*module_db_info) (dico_handle_t hp);
    char *(*module_db_descr) (dico_handle_t hp);
    dico_result_t (*module_match) (dico_handle_t hp, const char *strat,
				   const char *word);
    dico_result_t (*module_match_all) (dico_handle_t hp, const char *word,
				       dico_select_t sel, void *closure);
    dico_result_t (*module_define) (dico_handle_t hp, const char *word);
    int (*module_output_result) (dico_result_t rp, size_t n, dico_stream_t str);
    size_t (*module_result_count) (dico_result_t rp);
    void (*module_free_result) (dico_result_t rp);
};

#endif
