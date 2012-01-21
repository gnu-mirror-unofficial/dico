/* This file is part of GNU Dico.
   Copyright (C) 2012 Sergey Poznyakoff

   GNU Dico is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Dico is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dico.h>
#include <errno.h>
#include <appi18n.h>
#include <wn.h>

struct wndb {
    char *dbname;
    int pos;
    int search;
    int sense;
};

static int
_dico_display_message(char *msg)
{
    dico_log(L_ERR, 0, "WordNet: %s", msg);
    return -1;
}

static int
wn_free_db (dico_handle_t hp)
{
    struct wndb *wndb = (struct wndb *)hp;
    free(wndb->dbname);
    free(wndb);
    return 0;
}

static void wn_register_strategies(void);
static void wn_free_result(dico_result_t rp);

static int
wn_init(int argc, char **argv)
{
    char *wnsearchdir = NULL;
    char *wnhome = NULL;

    struct dico_option init_option[] = {
	{ DICO_OPTSTR(wnsearchdir), dico_opt_string, &wnsearchdir },
	{ DICO_OPTSTR(wnhome), dico_opt_string, &wnhome },
	{ NULL }
    };

    if (dico_parseopt(init_option, argc, argv, 0, NULL))
	return 1;

    display_message = _dico_display_message;
    /* FIXME: Control lengths! libWN does not bother to check boundaries */
    if (wnhome)
	setenv("WNHOME", wnhome, 1);
    if (wnsearchdir)
	setenv("WNSEARCHDIR", wnsearchdir, 1);
    if (wninit()) {
	dico_log(L_ERR, 0, _("cannot open wordnet database"));
	return 1;
    }

    wn_register_strategies();

    return 0;
}

static const char *pos_choice[] = {
    "all",
    "noun",
    "verb",
    "adj",
    "adjective",
    "adv",
    "adverb",
    "satellite",
    "adjsat",
    NULL
};
static int pos_trans[] = {
    ALL_POS,
    NOUN,
    VERB,
    ADJ,
    ADJ,
    ADV,
    ADV,
    ADJSAT,
    ADJSAT
};

static const char *search_choice[] = {
    "overview",
    "antptr",      
    "hyperptr",    
    "hypoptr",     
    "entailptr",   
    "simptr",      
    "ismemberptr", 
    "isstuffptr",  
    "ispartptr",   
    "hasmemberptr",
    "hasstuffptr", 
    "haspartptr",  
    "meronym",
    "holonym",
    "causeto",
    "pplptr",
    "seealsoptr",
    "pertptr",
    "attribute",
    "verbgroup",
    "derivation",      
    "classification",  
    "class",           
    "syns",
    "freq",            
    "frames",          
    "coords",
    "relatives",
    "hmeronym",
    "hholonym",
    "wngrep",
    NULL
};
    
static int search_trans[] = {
    OVERVIEW,
    ANTPTR,      
    HYPERPTR,    
    HYPOPTR,     
    ENTAILPTR,   
    SIMPTR,      
    ISMEMBERPTR, 
    ISSTUFFPTR,  
    ISPARTPTR,   
    HASMEMBERPTR,
    HASSTUFFPTR, 
    HASPARTPTR,  
    MERONYM,
    HOLONYM,
    CAUSETO,
    PPLPTR,
    SEEALSOPTR,
    PERTPTR,
    ATTRIBUTE,
    VERBGROUP,
    DERIVATION,      
    CLASSIFICATION,  
    CLASS,           
    SYNS,
    FREQ,            
    FRAMES,          
    COORDS,
    RELATIVES,
    HMERONYM,
    HHOLONYM,
    WNGREP
};

static dico_handle_t
wn_init_db(const char *dbname, int argc, char **argv)
{
    struct wndb *wndb;
    long sense = ALLSENSES;
    int pos = 0;
    int search = 0;

    struct dico_option init_db_option[] = {
	{ DICO_OPTSTR(pos), dico_opt_enum, &pos, { enumstr: pos_choice } },
	{ DICO_OPTSTR(search), dico_opt_enum, &search, { enumstr: search_choice } },
	{ DICO_OPTSTR(sense), dico_opt_long, &sense },
	{ NULL }
    };

    if (dico_parseopt(init_db_option, argc, argv, 0, NULL))
	return NULL;
    
    wndb = calloc(1, sizeof(*wndb));
    if (!wndb) {
	dico_log(L_ERR, ENOMEM, "wn_init_db");
	return NULL;
    }
    wndb->dbname = strdup(dbname);
    if (!wndb->dbname) {
	dico_log(L_ERR, ENOMEM, "wn_init_db");
	free(wndb);
	return NULL;
    }
    wndb->pos = pos_trans[pos];
    wndb->search = search_trans[search];
    wndb->sense = sense;

    return (dico_handle_t)wndb;
}
    
static char *
wn_descr(dico_handle_t hp)
{
    return strdup("WordNet dictionary");
}

static char *
wn_info(dico_handle_t hp)
{
    return strdup(license);
}

static int
wn_lang(dico_handle_t hp, dico_list_t list[2])
{
    if ((list[0] = dico_list_create()) == NULL)
	return -1;
    if ((list[1] = dico_list_create()) == NULL) {
	dico_list_destroy(&list[0]);
	return -1;
    }
    dico_list_append(list[0], strdup("en"));
    dico_list_append(list[1], strdup("en"));
    return 0;
}

struct wordbuf {
    char *word;
    size_t len;
    size_t size;
};
#define WORDBUFINC 16
#define INIT_WORDBUF { NULL, 0, 0 }
#define wordbuf_start(wb) ((wb)->len=0)

static int
wordbuf_expand(struct wordbuf *wb, size_t len)
{
    if (len >= wb->size) {
	size_t size = ((len + WORDBUFINC - 1) /  WORDBUFINC ) * WORDBUFINC;
	char *newword = realloc(wb->word, size);
	if (!newword) {
	    dico_log(L_ERR, ENOMEM, "wordbuf_expand");
	    return 1;
	}
	wb->word = newword;
	wb->size = size;
    }
    return 0;
}

static int
wordbuf_grow(struct wordbuf *wb, int c)
{
    if (wordbuf_expand(wb, wb->len + 1))
	return 1;
    wb->word[wb->len++] = c;
    return 0;
}

static int
wordbuf_finish(struct wordbuf *wb)
{
    if (wordbuf_expand(wb, wb->len + 1))
	return 1;
    wb->word[wb->len] = 0;
    return 0;
}

static void
wordbuf_reverse(struct wordbuf *wb)
{
    int i, j;

    if (wb->len == 0)
	return;
    for (i = 0, j = wb->len - 1; i < j; i++, j--) {
	int c = wb->word[j];
	wb->word[j] = wb->word[i];
	wb->word[i] = c;
    }
}


enum result_type {
    result_match,
    result_define
};

struct result {
    enum result_type type;
    size_t compare_count;
    union {
	struct {
	    dico_list_t list;
	    dico_iterator_t itr;
	} match;
	struct {
	    SynsetPtr synset[NUMPARTS];
	    SynsetPtr curss;
	    int i;
	} defn;
    } v;
};

static int
free_string (void *item, void *data)
{
    free(item);
    return 0;
}

static int
compare_words(const void *a, void *b)
{
    return utf8_strcasecmp((char*)a, (char*)b);
}

static struct result *
wn_create_match_result()
{
    struct result *res;

    res = calloc(1, sizeof(*res));
    if (!res) {
	dico_log(L_ERR, ENOMEM, "wn_create_match_result");
	return NULL;
    }
    res->type = result_match;
    res->v.match.list = dico_list_create();
    if (!res) {
	dico_log(L_ERR, ENOMEM, "wn_create_match_result");
	free(res);
	return NULL;
    }
    dico_list_set_free_item(res->v.match.list, free_string, NULL);
    dico_list_set_comparator(res->v.match.list, compare_words);
    dico_list_set_flags(res->v.match.list, DICO_LIST_COMPARE_TAIL);
    return res;
}

static int
wn_match_result_add(struct result *res, const char *hw)
{
    int rc;
    char *s = strdup(hw);

    if (!s) {
	dico_log(L_ERR, ENOMEM, "wn_result_add_key");
	return -1;
    }
    rc = dico_list_insert_sorted(res->v.match.list, s);
    if (rc) {
	free(s);
	if (rc != EEXIST) {
	    dico_log(L_ERR, ENOMEM, "wn_foreach_db");
	    return -1;
	}
    }
    return 0;
}

static void
skipeol(FILE *fp)
{
    int c;
    while ((c = getc(fp)) != '\n' && c != EOF)
	;
}

static void
skipheader(FILE *fp)
{
    int c;

    while ((c = getc(fp)) == ' ')
	skipeol(fp);
    ungetc(c, fp);
}

static int
getword(FILE *fp, struct wordbuf *wb)
{
    int c;
    size_t i;

    wordbuf_start(wb);
    while ((c = getc(fp)) != EOF) {
	if (c == ' ')
	    break;
	if (wordbuf_grow(wb, c))
	    return -1;
    }
    if (wb->len == 0 && c == EOF)
	return -1;
    if (wordbuf_finish(wb))
	return -1;
    for (i = 0; wb->word[i]; i++)
	if (wb->word[i] == '_')
	    wb->word[i] = ' ';
    return 0;
}

static int
lineback(FILE *fp, struct wordbuf *wb)
{
    int c, i;
    
    wordbuf_start(wb);
    while (fseek(fp, -2, SEEK_CUR) == 0) {
	if ((c = getc(fp)) == '\n')
	    break;
	if (wordbuf_grow(wb, c))
	    return -1;
    }
    if (wordbuf_finish(wb))
	return -1;
    wordbuf_reverse(wb);
    for (i = 0; wb->word[i]; i++) {
	if (wb->word[i] == ' ')
	    break;
	else if (wb->word[i] == '_')
	    wb->word[i] = ' ';
    }
    return 0;
}

static void
wn_foreach_db(int dbn, struct dico_key *key, struct result *res)
{
    FILE *fp = indexfps[dbn];
    struct wordbuf wb = INIT_WORDBUF;
    
    fseek(fp, 0, SEEK_SET);
    for (skipheader(fp); getword(fp, &wb) == 0; skipeol(fp)) {
	res->compare_count++;
	if (dico_key_match(key, wb.word)) {
	    if (wn_match_result_add(res, wb.word))
		break;
	}
    }
    free(wb.word);
}

static struct result *
wn_foreach(struct wndb *wndb, const dico_strategy_t strat, const char *word)
{
    struct result *res = wn_create_match_result();
    struct dico_key key;

    if (dico_key_init(&key, strat, word)) {
	dico_log(L_ERR, 0, _("wn_foreach: key initialization failed"));
	wn_free_result((dico_result_t) res);
	return NULL;
    }

    if (wndb->pos == ALL_POS) {
	int i;

	for (i = 1; i <= NUMPARTS; i++)
	    wn_foreach_db(i, &key, res);
    } else
	wn_foreach_db(wndb->pos, &key, res);
    
    dico_key_deinit(&key);
    
    if (dico_list_count(res->v.match.list) == 0) {
	wn_free_result((dico_result_t) res);
	return NULL;
    }
    return res;
}

static off_t
wn_bsearch(FILE *fp, void *key, int (*cmp)(const char *a, void *b))
{
    long top, mid, bot, diff;
    struct wordbuf wb = INIT_WORDBUF;
    off_t last_match = -1;
    
    fseek(fp, 0L, SEEK_END);
    top = 0;
    bot = ftell(fp);
    mid = (bot - top) / 2;

    do {
	fseek(fp, mid - 1, SEEK_SET);
	if (mid != 1)
	    skipeol(fp);
	if (getword(fp, &wb))
	    break;
	if (cmp(wb.word, key) < 0) {
	    top = mid;
	    diff = (bot - top) / 2;
	    mid = top + diff;
	} else if (cmp(wb.word, key) > 0) {
	    bot = mid;
	    diff = (bot - top) / 2;
	    mid = top + diff;
	} else {
	    /* Move back until first non-matching headword is found */
	    do
		last_match = ftell(fp);
	    while (lineback(fp, &wb) == 0 && cmp(wb.word, key) == 0);
	    break;
	}
    } while (diff);

    free(wb.word);
    return last_match;
}

static struct result *
wn_exact_match(struct wndb *db, const char *hw)
{
    struct result *res;
    int i, d;

    for (i = 1; (d = is_defined((char*)hw, i)) == 0 && i <= NUMPARTS; i++);

    if (!d)
	return 0;
    res = wn_create_match_result();
    dico_list_append(res->v.match.list, strdup(hw));
    return res;
}

struct prefix {
    const char *str;
    size_t len;
};

static int
cmp_pref(const char *hw, void *key)
{
    struct prefix *pref = key;
    return strncasecmp(hw, pref->str, pref->len);
}

static struct result *
wn_prefix_match(struct wndb *db, const char *hw)
{
    struct result *res;
    int i;
    struct prefix pfx;
    struct wordbuf wb = INIT_WORDBUF;
    
    res = wn_create_match_result();
    if (!res)
	return NULL;
    pfx.str = hw;
    pfx.len = strlen(hw);

    for (i = 1; i <= NUMPARTS; i++) {
	FILE *fp = indexfps[i];
	off_t off;

	off = wn_bsearch(fp, &pfx, cmp_pref);
	if (off != -1) {
	    fseek(fp, off, SEEK_SET);
	    for (;getword(fp, &wb) == 0 && cmp_pref(wb.word, &pfx) == 0;
		 skipeol(fp)) {
		if (wn_match_result_add(res, wb.word))
		    break;
	    }
	}
    }
    free(wb.word);
    if (dico_list_count(res->v.match.list) == 0) {
	wn_free_result((dico_result_t) res);
	return NULL;
    }
    return res;
}

typedef struct result *(*wn_matcher_t)(struct wndb *db, const char *hw);

struct strategy_def {
    struct dico_strategy strat;
    wn_matcher_t matcher;
};

static struct strategy_def stratdef[] = {
    { { "exact", "Match words exactly" }, wn_exact_match },
    { { "prefix", "Match word prefixes" }, wn_prefix_match },
};

static wn_matcher_t
wn_find_matcher(const char *strat)
{
    int i;
    for (i = 0; i < DICO_ARRAY_SIZE(stratdef); i++) 
	if (strcmp(strat, stratdef[i].strat.name) == 0)
	    return stratdef[i].matcher;
    return NULL;
}

static void
wn_register_strategies(void)
{
    int i;
    for (i = 0; i < DICO_ARRAY_SIZE(stratdef); i++) 
	dico_strategy_add(&stratdef[i].strat);
}

static dico_result_t
wn_match(dico_handle_t hp, const dico_strategy_t strat, const char *word)
{
    struct wndb *wndb = (struct wndb *)hp;

    if (strat->sel)
	return (dico_result_t) wn_foreach(wndb, strat, word);
    else {
	wn_matcher_t match = wn_find_matcher(strat->name);
	if (match)
	    return (dico_result_t) match(wndb, word);
    }
    return NULL;
}

#define ISSPACE(c) ((c) == ' ' || (c) == '\t')

static dico_result_t
wn_define(dico_handle_t hp, const char *word)
{
    struct wndb *wndb = (struct wndb *)hp;
    struct result *res;
    int i;
    int found = 0;
    char *copy = NULL;
    
    res = calloc(1, sizeof(*res));
    if (!res) {
	dico_log(L_ERR, ENOMEM, "wn_define");
	return NULL;
    }

    if (word[strcspn(word, " \t")]) {
	char *p, *q;
	
	copy = malloc(strlen(word) + 1);
	if (!copy) {
	    dico_log(L_ERR, ENOMEM, "wn_define");
	    free(res);
	    return NULL;
	}
	for (p = copy, q = word; *q; ) {
	    if (ISSPACE(*q)) {
		*p++ = '_';
		do
		    q++;
		while (*q && ISSPACE(*q));
	    } else
		*p++ = *q++;
	}
	*p = 0;
	word = copy;
    }
    
    res->type = result_define;
    if (wndb->pos == ALL_POS) {
	for (i = 0; i < NUMPARTS; i++) {
	    if (res->v.defn.synset[i] =
		findtheinfo_ds((char*)word, i + 1, wndb->search, wndb->sense))
		found++;
	}
    } else 
	if (res->v.defn.synset[i] =
	    findtheinfo_ds((char*)word, i + 1, wndb->search, wndb->sense))
	    found++;

    free(copy);
    
    if (!found) {
	free(res);
	return NULL;
    }
    return (dico_result_t)res;
}

static void
format_word(const char *word, dico_stream_t str)
{
    for (;;) {
	size_t len = strcspn(word, "_");
	dico_stream_write(str, word, len);
	if (word[len] == 0)
	    break;
	dico_stream_write(str, " ", 1);
	word += len + 1;
    }
}

static void
format_synset(SynsetPtr sp, dico_stream_t str)
{
    int i;
    
    /* Print words: */
    for (i = 0; i < sp->wcount; i++) {
	if (i)
	    dico_stream_write(str, ", ", 2);
	format_word(sp->words[i], str);
    }

    /* Print pos */
    dico_stream_write(str, "; ", 2);
    dico_stream_write(str, sp->pos, strlen(sp->pos));
    dico_stream_write(str, ".\n\n", 3);

    /* Print definition */
    dico_stream_write(str, sp->defn, strlen(sp->defn));
    dico_stream_write(str, "\n", 1);
}

int
wn_output_result (dico_result_t rp, size_t n, dico_stream_t str)
{
    struct result *res = (struct result *) rp;
    char *word;
    
    switch (res->type) {
    case result_match:
	if (!res->v.match.itr) {
	    res->v.match.itr = dico_list_iterator(res->v.match.list);
	    if (!res->v.match.itr)
		return 1;
	}
	word = dico_iterator_item(res->v.match.itr, n);
	dico_stream_write(str, word, strlen(word));
	break;

    case result_define:
	if (!res->v.defn.curss) {
	    while (res->v.defn.curss == NULL && res->v.defn.i < NUMPARTS)
		res->v.defn.curss = res->v.defn.synset[res->v.defn.i++];
	    if (!res->v.defn.curss)
		return 1;
	}
	format_synset(res->v.defn.curss, str);
        res->v.defn.curss = res->v.defn.curss->nextss;
	break;
	    
    default:
	return 1;
    }
    return 0;
}

static size_t
wn_result_count (dico_result_t rp)
{
    struct result *res = (struct result *) rp;
    size_t count = 0;
    int i;
    
    switch (res->type) {
    case result_match:
	count = dico_list_count(res->v.match.list);
	break;

    case result_define:
	for (i = 0; i < NUMPARTS; i++) {
	    SynsetPtr sp;
	    for (sp = res->v.defn.synset[i]; sp; sp = sp->nextss)
		count++;
	}
	break;
    }
    return count;
}

static size_t
wn_compare_count (dico_result_t rp)
{
    struct result *res = (struct result *) rp;
    return res->compare_count;
}

static void
wn_free_result(dico_result_t rp)
{
    struct result *res = (struct result *) rp;
    int i;
    
    switch (res->type) {
    case result_match:
	dico_list_destroy(&res->v.match.list);
	dico_iterator_destroy(&res->v.match.itr);
	break;

    case result_define:
	for (i = 0; i < NUMPARTS; i++) 
	    if (res->v.defn.synset[i])
		free_syns(res->v.defn.synset[i]);
    }
    free(res);
}


struct dico_database_module DICO_EXPORT(wordnet, module) = {
    DICO_MODULE_VERSION,
    DICO_CAPA_NONE,
    wn_init,
    wn_init_db,
    wn_free_db,
    NULL,
    NULL,
    wn_info,
    wn_descr,
    wn_lang,
    wn_match,
    wn_define,
    wn_output_result,
    wn_result_count,
    wn_compare_count,
    wn_free_result
};


