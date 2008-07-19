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
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <dict.h>
#include <xdico.h>
#include <db.h>
#define obstack_chunk_alloc xmalloc
#define obstack_chunk_free free
#include <obstack.h>
#include <xalloc.h>
#include <appi18n.h>

#define HIRAGANA_BYTE 0x24
#define KATAKANA_BYTE 0x25

#define UNCOMPRESS "gzip -d -c" 
#define UNCOMPRESSEXT ".gz"

#define NUMITEMS(a) sizeof(a)/sizeof((a)[0])

/* This comes from <X11/Xutil.h> */
typedef struct {                /* normal 16 bit characters are two bytes */
    unsigned char byte1;
    unsigned char byte2;
} XChar2b;

struct dbidx;

typedef int (*get_key_fn)(DB *, const DBT *, const DBT *, DBT *);
typedef int (*cmp_key_fn)(DB *, const DBT *, const DBT *);
typedef int (*iter_fn)(struct dbidx *, unsigned, DictEntry *);

struct dbidx {
    char *name;
    get_key_fn get_key;
    cmp_key_fn cmp_key;
    iter_fn iter;
    DB *dbp;
    /* FIXME: What's more? */
};

extern char *dictdir;
extern char *kanjidict;
extern char *edict;
extern char *outdir;
extern char *dictname;
extern int verbose;
extern int maxstr8;
extern int maxstr16;
extern int maxbuflen8;
extern int maxbuflen16;
extern struct dbidx dbidx[];
extern unsigned numberofkanji, highestkanji, lowestkanji;
extern unsigned nedict;

void get_options(int argc, char **argv);

int compile (void);

DB *open_db(void);
void close_db(DB *);
void iterate_db(DB *);
void count_xref(DB *);

void open_secondary(DB *master, struct dbidx *idx);
int insert_dict_entry(DB *dbp, unsigned num, DictEntry *entry, size_t size);


FILE *open_compressed(char *dictname, int *pflag);

void  compile_kanjidic(DB *dbp);

/* romaji.c */
/* FIXME: Move these to the library. */
typedef int (*romaji_out_fn)(void *, const char *);

int kana_to_romaji(const XChar2b *input, size_t size, romaji_out_fn fun,
		   void *closure);
int kana_to_romaji_str(const XChar2b *input, size_t size, char **sptr);

/* getopt.m4 */
extern void print_help(void);
extern void print_usage(void);

