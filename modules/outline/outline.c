/* This file is part of Dico.
   Copyright (C) 2008 Sergey Poznyakoff

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

/* This module provides support for databases in Emacs outline format.
   It is intended to work as a testbed for Dico's database module system.
   Do not use it in production environment.

   An outline database has the following structure:

   * Description
   <text>
   
   * Info
   <text>

   * Dictionary
   ** <entry>
   <text>
   [any number of entries follows]

   Any other text is ignored.

   To use this module, add the following to your dictd.conf file:

   handler outline {
	type loadable;
	command "outline";
   }
   database {
        handler "outline <filename>";
	...
   }
*/   


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <dico.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

struct entry {
    char *word;             /* Word */
    size_t wordlen;         /* Its length in characters */
    off_t offset;           /* Offset of the corresponding article in file */
    size_t size;            /* Size of the article */
};

struct outline_file {
    char *name;
    FILE *fp;
    size_t count;
    struct entry *index;

    struct entry *info_entry, *descr_entry;
};

#define STATE_INITIAL 0
#define STATE_DICT    1

static size_t
trimnl(char *buf)
{
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
	buf[--len] = 0;
    return len;
}

static size_t
trimws(char *buf)
{
    size_t len = strlen(buf);
    while (len > 0 && isspace(buf[len - 1]))
	buf[--len] = 0;
    return len;
}
    
int
find_header(struct outline_file *file, char *buf, size_t size, size_t *pread)
{
    while (fgets(buf, size, file->fp)) {
	size_t rdbytes = strlen(buf);
	size_t len = trimnl(buf);
	
	if (len > 0) {
	    int level;

	    for (level = 0; buf[level] == '*' && level < len; level++)
		;
	    if (level) {
		*pread = rdbytes;
		return level;
	    }
	}
    }
    return 0;
}

off_t
skipws(struct outline_file *file, char *buf, size_t size)
{
    while (fgets(buf, size, file->fp)) {
	size_t len = strlen(buf);
	if (!(len == 1 && buf[0] == '\n')) {
	    fseek(file->fp, -(off_t)len, SEEK_CUR);
	    break;
	}
    }
    return ftell(file->fp);
}

struct entry *
alloc_entry(const char *text, size_t len)
{
    struct entry *ep = malloc(sizeof(*ep));
    if (ep) {
	memset(ep, 0, sizeof(*ep));
	ep->word = malloc(len + 1);
	if (!ep->word) {
	    free(ep);
	    return NULL;
	}
	memcpy(ep->word, text, len);
	ep->word[len] = 0;
	ep->wordlen = len; /* FIXME: multibyte!! */
    }
    return ep;
}

static struct entry *
read_entry(struct outline_file *file, int *plevel)
{
    char buf[128], *p;
    struct entry *ep;
    int level;
    size_t rdbytes, len;
    
    level = find_header(file, buf, sizeof(buf), &rdbytes);

    if (level == 0)
	return NULL;

    *plevel = level;
    for (p = buf + level; *p && isspace(*p); p++) 
	;

    len = trimws(p);

    ep = alloc_entry(p, len);
    if (!ep)
	return NULL;

    ep->offset = skipws(file, buf, sizeof(buf));

    find_header(file, buf, sizeof(buf), &rdbytes);
    fseek(file->fp, -(off_t)rdbytes, SEEK_CUR);
    ep->size = ftell(file->fp) - ep->offset;
    
    return ep;
}


enum result_type {
    result_match,
    result_define
};

struct result {
    struct outline_file *file;
    enum result_type type;
    const struct entry *ep;
    size_t count;
};

typedef int (*entry_match_t) (struct outline_file *file,
			      const char *word,
			      struct result *res);

struct strategy_def {
    dico_strategy_t strat;
    entry_match_t match;
};

static int exact_match(struct outline_file *, const char *, struct result *);
static int prefix_match(struct outline_file *, const char *, struct result *);

static struct strategy_def strat_tab[] = {
    { { "exact", "Match words exactly" }, exact_match },
    { { "prefix", "Match word prefixes" }, prefix_match },
};

static entry_match_t
find_matcher(const char *strat)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(strat_tab); i++) 
	if (strcmp(strat, strat_tab[i].strat.name) == 0)
	    return strat_tab[i].match;
    return NULL;
}

int
outline_init(int argc, char **argv)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(strat_tab); i++) 
	dico_strategy_add(&strat_tab[i].strat);
    return 0;
}


static int
compare_entry(const void *a, const void *b)
{
    const struct entry *epa = a;
    const struct entry *epb = b;
    return strcasecmp(epa->word, epb->word);
}

static int
exact_match(struct outline_file *file, const char *word, struct result *res)
{
    struct entry x, *ep;
    x.word = (char*) word;
    x.wordlen = strlen(word);
    ep = bsearch(&x, file->index, file->count, sizeof(file->index[0]),
		 compare_entry);
    if (ep) {
	res->ep = ep;
	res->count = 1;
	return 0;
    }
    return 1;
}


static int
compare_prefix(const void *a, const void *b)
{
    const struct entry *pkey = a;
    const struct entry *pelt = b;
    size_t wordlen = pkey->wordlen;
    if (pelt->wordlen < wordlen)
	wordlen = pelt->wordlen;
    return strncasecmp(pkey->word, pelt->word, wordlen);
}

static int
prefix_match(struct outline_file *file, const char *word, struct result *res)
{
    struct entry x, *ep;
    x.word = (char*) word;
    x.wordlen = strlen(word);
    ep = bsearch(&x, file->index, file->count, sizeof(file->index[0]),
		 compare_prefix);
    if (ep) {
	size_t count = 1;
	struct entry *p;
	for (p = ep - 1; p > file->index && compare_prefix(&x, p) == 0; p--)
	    count++;
	for (ep++; ep < file->index + file->count
		 && compare_prefix(&x, ep) == 0; ep++)
	    count++;
	res->ep = p + 1;
	res->count = count;
	return 0;
    }
    return 1;
}


int
outline_close(dico_handle_t hp)
{
    size_t i;
    struct outline_file *file = hp;
    
    fclose(file->fp);
    free(file->name);
    free(file->info_entry);
    free(file->descr_entry);
    for (i = 0; i < file->count; i++) 
	free(file->index[i].word);
    free(file->index);
    free(file);
    return 0;
}

dico_handle_t
outline_open(const char *dbname, int argc, char **argv)
{
    FILE *fp;
    struct outline_file *file;
    dico_list_t list;
    struct entry *ep;
    int level;
    int state;
    size_t i, count;
    dico_iterator_t itr;

    if (argc != 2) {
	dico_log(L_ERR, 0, _("outline_open: wrong number of arguments"));
	return NULL;
    }
    
    fp = fopen(argv[1], "r");
    if (!fp) {
	dico_log(L_ERR, errno, _("cannot open file %s"), argv[1]);
	return NULL;
    }
    file = malloc(sizeof(*file));
    if (!file) {
	dico_log(L_ERR, 0, "not enough memory");
	fclose(fp);
	return NULL;
    }

    memset(file, 0, sizeof(*file));
    file->name = strdup(argv[1]);
    file->fp = fp;
    
    list = dico_list_create();
    if (!list) {
	dico_log(L_ERR, 0, "not enough memory");
	fclose(fp);
	free(file);
	return NULL;
    }

    state = STATE_INITIAL;
    while (ep = read_entry(file, &level)) {
	switch (state) {
	case STATE_DICT:
	    if (level == 2) {
		dico_list_append(list, ep);
		break;
	    } else if (level == 1) {
		state = STATE_INITIAL;
		/* FALL THROUGH */
	    } else {
		free(ep);
		break;
	    }
	    
	case STATE_INITIAL:
	    if (level == 1) {
		if (strcasecmp(ep->word, "info") == 0) {
		    file->info_entry = ep;
		    break;
		} else if (strcasecmp(ep->word, "description") == 0) {
		    file->descr_entry = ep;
		    break;
		} else if (strcasecmp(ep->word, "dictionary") == 0)
		    state = STATE_DICT;
	    }
	    free(ep);
	    break;
	}
    }

    file->count = count = dico_list_count(list);
    file->index = calloc(count, sizeof(file->index[0]));
    if (!file->index) {
	dico_log(L_ERR, 0, "not enough memory");
	outline_close((dico_handle_t)file);
	return NULL;
    }

    itr = dico_iterator_create(list);
    for (i = 0, ep = dico_iterator_first(itr); ep;
	 i++, ep = dico_iterator_next(itr)) {
	file->index[i] = *ep;
	free(ep);
    }
    dico_iterator_destroy(&itr);
    dico_list_destroy(&list, NULL, NULL);
    qsort(file->index, count, sizeof(file->index[0]), compare_entry);
    
    return file;
}


char *
read_buf(struct outline_file *file, struct entry *ep)
{
    size_t size;
    char *buf = malloc(ep->size + 1);
    if (!buf)
	return NULL;
    fseek(file->fp, ep->offset, SEEK_SET);
    size = fread(buf, 1, ep->size, file->fp);
    buf[size] = 0;
    return buf;
}

char *
outline_info(dico_handle_t hp)
{
    struct outline_file *file = hp;
    if (file->info_entry) 
	return read_buf(file, file->info_entry);
    return NULL;
}

char *
outline_descr(dico_handle_t hp)
{
    struct outline_file *file = hp;
    if (file->descr_entry) { 
	char *buf = read_buf(file, file->descr_entry);
	char *p = strchr(buf, '\n');
	if (p)
	    *p = 0;
	return buf;
    }
    return NULL;
}



dico_result_t
outline_match(dico_handle_t hp, const char *strat, const char *word)
{
    struct outline_file *file = hp;
    struct result *res;
    entry_match_t match = find_matcher(strat);

    if (!match)
	return NULL;
    
    res = malloc(sizeof(*res));
    if (!res)
	return NULL;
    res->file = file;
    res->type = result_match;
    if (match(file, word, res)) {
	free(res);
	res = NULL;
    }
    return res;
}

dico_result_t
outline_define(dico_handle_t hp, const char *word)
{
    struct outline_file *file = hp;
    struct result *res;
    
    res = malloc(sizeof(*res));
    if (!res)
	return NULL;
    res->file = file;
    res->type = result_define;
    if (exact_match(file, word, res)) {
	free(res);
	res = NULL;
    }
    return res;
}

static void
printdef(dico_stream_t str, struct outline_file *file, const struct entry *ep)
{
    FILE *fp = file->fp;
    size_t size = ep->size;
    char buf[128];
    
    fseek(fp, ep->offset, SEEK_SET);
    while (size) {
	size_t rdsize = size;
	if (rdsize > sizeof(buf))
	    rdsize = sizeof(buf);
	rdsize = fread(buf, 1, rdsize, fp);
	if (rdsize == 0)
	    break;
	dico_stream_write(str, buf, rdsize);
	size -= rdsize;
    }
}

int
outline_output_result (dico_result_t rp, size_t n, dico_stream_t str)
{
    struct result *res = rp;
    const struct entry *ep = res->ep + n;
    
    switch (res->type) {
    case result_match:
	dico_stream_write(str, ep->word, strlen(ep->word));
	break;
	
    case result_define:
	printdef(str, res->file, ep);
    }
    return 0;
}

size_t
outline_result_count (dico_result_t rp)
{
    struct result *res = rp;
    return res->count;
}

void
outline_free_result(dico_result_t rp)
{
    free(rp);
}

struct dico_handler_module DICO_EXPORT(outline, module) = {
    outline_init,
    outline_open,
    outline_close,
    outline_info,
    outline_descr,
    outline_match,
    outline_define,
    outline_output_result,
    outline_result_count,
    outline_free_result
};
    
