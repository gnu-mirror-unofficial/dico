/* This file is part of GNU Dico.
   Copyright (C) 2008-2019 Sergey Poznyakoff

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

#include "dictorg.h"

static char *dbdir;
static size_t compare_count;
static int sort_index;
static int trim_ws;
static int show_dictorg_entries;

static int
is_alnumspace(unsigned c)
{
    return utf8_wc_is_alnum(c) || utf8_wc_is_space(c);
}

static inline int
headword_compare(char const *a, char const *b, struct dictdb *db)
{
    return utf8_compare(a, b,
			db->flag_casesensitive
			  ? case_sensitive : case_insensitive,
			0,
			db->flag_allchars ? NULL : is_alnumspace);
}

static inline int
headword_compare_allchars(char const *a, char const *b, struct dictdb *db,
			  size_t len)
{
    return utf8_compare(a, b,
			db->flag_casesensitive
			  ? case_sensitive : case_insensitive,
			len,
			NULL);
}

static int
compare_index_entry(const void *a, const void *b, void *closure)
{
    const struct index_entry *epa = a;
    const struct index_entry *epb = b;
    compare_count++;
    return headword_compare(epa->word, epb->word, (struct dictdb *)closure);
}

static int get_db_flag(struct dictdb *db, const char *name);
    
static int register_strategies(void);

static struct dico_option init_option[] = {
    { DICO_OPTSTR(dbdir), dico_opt_string, &dbdir },
    { DICO_OPTSTR(sort), dico_opt_bool, &sort_index },
    { DICO_OPTSTR(trim-ws), dico_opt_bool, &trim_ws },
    { DICO_OPTSTR(show-dictorg-entries), dico_opt_bool,
      &show_dictorg_entries },
    { NULL }
};

static int
mod_init(int argc, char **argv)
{
    if (dico_parseopt(init_option, argc, argv, 0, NULL))
	return 1;
    if (dbdir) {
	struct stat st;
	
	if (stat(dbdir, &st)) {
	    dico_log(L_ERR, errno, _("mod_init: cannot stat `%s'"),
		     dbdir);
	    return 1;
	}
	if (!S_ISDIR(st.st_mode)) {
	    dico_log(L_ERR, 0, _("mod_init: `%s' is not a directory"),
		     dbdir);
	    return 1;
	}
	if (access(dbdir, R_OK)) {
	    dico_log(L_ERR, 0, _("mod_init: `%s' is not readable"),
		     dbdir);
	    return 1;
	}
    }

    register_strategies();
    
    return 0;
}

static void
dealloc_index_entry(struct index_entry *ent)
{
    free(ent->word);
    free(ent->orig);
}

static void
free_db(struct dictdb *db)
{
    size_t i;
    
    dico_stream_close(db->stream);
    dico_stream_destroy(&db->stream);
    for (i = 0; i < db->numwords; i++) {
	if (!db->index[i].word)
	    break;
	dealloc_index_entry(&db->index[i]);
    }
    if (db->suf_index) {
	for (i = 0; i < db->numwords; i++) {
	    if (!db->suf_index[i].word)
		break;
	    free(db->suf_index[i].word);
	}
	free(db->suf_index);
    }
    free(db->index);
    free(db->basename);
    free(db);
}

static char *
mkname(const char *fname, const char *suf)
{
    size_t len = strlen(fname) + 1 + strlen(suf) + 1;
    char *res = malloc(len);
    if (res) {
	strcpy(res, fname);
	strcat(res, ".");
	strcat(res, suf);
    }
    return res;
}

#define ISWS(c) ((c) == ' ' || (c) == '\t')

static int
b64_decode(const char *val, size_t len, size_t *presult)
{
    size_t v = 0;
    size_t  i;

    for (i = 0; i < len; i++) {
	int x = dico_base64_input(val[i]);
	if (x == -1) 
	    return 1;
	v <<= 6;
	v |= x;
    }
    if (i < len)
	v <<= (len - 1) * 6;
    *presult = v;
    return 0;
}

static int
parse_index_entry(const char *filename, size_t line,
		  dico_list_t list, char *buf, int tws)
{
    struct utf8_iterator itr;
    struct index_entry idx, *ep;
    int nfield = 0;
    int rc;
    
    memset(&itr, 0, sizeof(itr));
    memset(&idx, 0, sizeof(idx));
    
    utf8_iter_first(&itr, buf);

    rc = 0;
    /* A valid index file contains three to four columns per line:
         0 - Headword.
	 1 - Offset of the article in the dictionary file.
	 2 - Size of the article in bytes
	 3 - Optional original headword.

      Column 0 is used for searches. Column 3, if present, is used when
      returning the results of the search. If it is not supplied, column 0
      is used.
    */
    for (nfield = 0; nfield < 4; nfield++) {
	char const *start;
	char const *end;
	size_t len;

	if (nfield) {
	    /* Skip whitespace */
	    for (; !utf8_iter_end_p(&itr)
		     && utf8_iter_isascii(itr) && ISWS(*itr.curptr);
		 utf8_iter_next(&itr))
		;
	}
	
	if (utf8_iter_end_p(&itr))
	    break;

	start = itr.curptr;
	for (; !utf8_iter_end_p(&itr)
		 && !(utf8_iter_isascii(itr) && *itr.curptr == '\t');
	     utf8_iter_next(&itr))
	    ;
	end = itr.curptr;
	len = end - start;

	if (nfield == 0) {
	    if (tws) {
		/* Strip trailing whitespace. */
		while (len > 0 && start[len-1] == ' ')
		    --len;
	    }

	    idx.word = malloc(len + 1);
	    if (!idx.word) {
		DICO_LOG_MEMERR();
		return 1;
	    }

	    memcpy(idx.word, start, len);
	    idx.word[len] = 0;
	    idx.length = len;
	    idx.wordlen = utf8_strlen(idx.word);
	} else if (nfield == 3) {
	    idx.orig = malloc(len + 1);
	    if (!idx.orig) {
		DICO_LOG_MEMERR();
		return 1;
	    }
	    memcpy(idx.orig, start, len);
	    idx.orig[len] = 0;	    
	} else {
	    size_t n;
	    
	    if (b64_decode(start, len, &n)) {
		dico_log(L_ERR, 0, _("%s:%lu: invalid base64 value: `%.*s'"),
			 filename, (unsigned long) line, (int) len, start);
		rc = 1;
		break;
	    }
	    
	    if (nfield == 1)
		idx.offset = n;
	    else
		idx.size = n;
	}
    }

    if (rc == 0) {
	if (!utf8_iter_end_p(&itr)) {
	    dico_log(L_ERR, 0, _("%s:%lu: malformed entry"),
		     filename, (unsigned long) line);
	    rc = 1;	
	} else {
	    ep = malloc(sizeof(*ep));
	    if (!ep) {
		DICO_LOG_MEMERR();
		rc = 1;
	    }
	}
    }

    if (rc)
	free(idx.word);
    else {
	*ep = idx;
	dico_list_append(list, ep);
    }
    return rc;
}

static int
free_index_entry(void *item, void *data)
{
    struct index_entry *ep = item;
    dealloc_index_entry(ep);
    free(ep);
    return 0;
}

static int
read_index(struct dictdb *db, const char *idxname, int tws)
{
    struct stat st;
    int rc;
    dico_list_t list;
    dico_stream_t stream;
    
    if (stat(idxname, &st)) {
	dico_log(L_ERR, errno, _("open_index: cannot stat `%s'"), idxname);
	return 1;
    }
    if (!S_ISREG(st.st_mode)) {
	dico_log(L_ERR, 0, _("open_index: `%s' is not a regular file"),
		 idxname);
	return 1;
    }


    stream = dico_mapfile_stream_create(idxname, DICO_STREAM_READ);
    if (!stream) {
	dico_log(L_ERR, errno,
		 _("cannot create stream `%s'"), idxname);
	return 1;
    }
    rc = dico_stream_open(stream);
    if (rc) {
	dico_log(L_ERR, 0,
		 _("cannot open stream `%s': %s"),
		 idxname, dico_stream_strerror(stream, rc));
	dico_stream_destroy(&stream);
    }

    list = dico_list_create();
    if (!list) {
	DICO_LOG_MEMERR();
	rc = 1;
    } else {
	dico_iterator_t itr;
	size_t i;
	struct index_entry *ep;
	char *buf = NULL;
	size_t bufsize = 0;
	size_t rdsize;

	rc = 0;

	i = 0;
	while (!dico_stream_getline(stream, &buf, &bufsize, &rdsize)) {
	    i++;
	    dico_trim_nl(buf);
	    rc = parse_index_entry(idxname, i, list, buf, tws);
	    if (rc)
		break;
	}
	free(buf);
	if (rc) {
	    dico_list_set_free_item(list, free_index_entry, NULL);
	} else {
	    db->numwords = dico_list_count(list);
	    db->index = calloc(db->numwords, sizeof(db->index[0]));
	    itr = dico_list_iterator(list);
	    for (i = 0, ep = dico_iterator_first(itr);
		 ep;
		 i++, ep = dico_iterator_next(itr)) {
		db->index[i] = *ep;
		free(ep);
	    }
	    dico_iterator_destroy(&itr);
	}
	dico_list_destroy(&list);
    }

    dico_stream_close(stream);
    dico_stream_destroy(&stream);
    return rc;
}

static int
open_index(struct dictdb *db, int tws)
{
    char *idxname = mkname(db->basename, "index");
    int rc;
    
    if (!idxname) {
	DICO_LOG_MEMERR();
	return 1;
    }

    rc = read_index(db, idxname, tws);
    free(idxname);
    return rc;
}

static int
mod_free_db(dico_handle_t hp)
{
    struct dictdb *db = (struct dictdb *) hp;
    free_db(db);
    return 0;
}

static int
open_stream(struct dictdb *db)
{
    char *name;
    static char *suff[] = {
	"dict.dz", "dict"
    };
    int i;
    int rc;
    dico_stream_t str;
    
    for (i = 0; i < DICO_ARRAY_SIZE(suff); i++) {
	name = mkname(db->basename, suff[i]);
	if (access(name, R_OK) == 0) {
	    str = dict_stream_create(name, 0);
	    if (!str) {
		dico_log(L_ERR, errno,
			 _("cannot create stream `%s'"),
			 name);
		free(name);
		continue;
	    }
	    rc = dico_stream_open(str);
	    if (rc) {
		dico_log(L_ERR, 0,
			 _("cannot open stream `%s': %s"),
			 name, dico_stream_strerror(str, rc));
		free(name);
		continue;
	    }
	    free(name);
	    db->stream = str;
	    return 0;
	}
    }
    dico_log(L_ERR, 0, _("cannot open stream for dictionary `%s'"),
	     db->basename);
    return 1;
}

static dico_handle_t
mod_init_db(const char *dbname, int argc, char **argv)
{
    struct dictdb *db;
    char *filename = NULL;
    int sort_option = sort_index;
    int trimws_option = trim_ws;
    int show_dictorg_option = show_dictorg_entries;
    
    struct dico_option option[] = {
	{ DICO_OPTSTR(sort), dico_opt_bool, &sort_option },
	{ DICO_OPTSTR(database), dico_opt_const_string, &filename },
	{ DICO_OPTSTR(trim-ws), dico_opt_bool, &trimws_option },
	{ DICO_OPTSTR(show-dictorg-entries), dico_opt_bool,
		      &show_dictorg_option },
	{ NULL }
    };
	
    if (dico_parseopt(option, argc, argv, 0, NULL))
	return NULL;

    if (!filename) {
	dico_log(L_ERR, 0,
		 _("mod_init_db(%s): database name not given"),
		 argv[0]);
	return NULL;
    }
    
    if (filename[0] != '/') {
	if (dbdir) {
	    filename = dico_full_file_name(dbdir, filename);
	} else {
	    dico_log(L_ERR, 0,
		     _("mod_init_db: `%s' is not an absolute file name"),
		     filename);
	    return NULL;
	}
    } else
	filename = strdup(filename);

    if (!filename) {
	DICO_LOG_MEMERR();
	return NULL;
    }
    
    db = calloc(1, sizeof(*db));
    if (!db) {
	free(filename);
	DICO_LOG_MEMERR();
	return NULL;
    }

    db->dbname = dbname;
    db->basename = filename;
    db->show_dictorg_entries = show_dictorg_option;
    db->flag_allchars = 1;
    
    if (open_index(db, trimws_option)) {
	free_db(db);
	return NULL;
    }

    if (open_stream(db)) {
	free_db(db);
	return NULL;
    }
    
    db->flag_allchars = get_db_flag(db, DICTORG_FLAG_ALLCHARS);
    db->flag_casesensitive = get_db_flag(db, DICTORG_FLAG_CASESENSITIVE);
    db->flag_utf8 = get_db_flag(db, DICTORG_FLAG_UTF8);
    db->flag_8bit = get_db_flag(db, DICTORG_FLAG_8BIT_NEW);
    if (get_db_flag(db, DICTORG_FLAG_8BIT_OLD)) {
	dico_log(L_ERR, 0,
		 _("mod_init_db(%s): index files in old 8-bit format are not supported"),
		 argv[0]);
	free_db(db);
	return NULL;
    }

    if (sort_option) {
	/* Sort index entries */
	dico_sort(db->index, db->numwords, sizeof(db->index[0]),
		  compare_index_entry, db);
    }
    
    return (dico_handle_t)db;
}

static void
revert_word(char *dst, const char *src, size_t len)
{
    struct utf8_iterator itr;
    char *p = dst + len;

    *p = 0;
    for (utf8_iter_init(&itr, (char *)src, len);
	 !utf8_iter_end_p(&itr);
	 utf8_iter_next(&itr)) {
	p -= itr.curwidth;
	if (p < dst)
	    break;
	memcpy(p, itr.curptr, itr.curwidth);
    }
}

static int
compare_rev_entry(const void *a, const void *b, void *closure)
{
    struct rev_entry const *epa = a;
    struct rev_entry const *epb = b;
    return headword_compare_allchars(epa->word, epb->word, closure, 0);
}

static int
init_suffix_index(struct dictdb *db)
{
    if (!db->suf_index) {
	size_t i;
	
	db->suf_index = calloc(db->numwords, sizeof(db->suf_index[0]));
	if (!db->suf_index) 
	    return 1;
	for (i = 0; i < db->numwords; i++) {
	    char *p = malloc(db->index[i].length + 1);
	    if (!p) {
		while (i > 0)
		    free(db->suf_index[--i].word);
		free(db->suf_index);
		return 1;
	    }
	    revert_word(p, db->index[i].word, db->index[i].length);
	    db->suf_index[i].word = p;
	    db->suf_index[i].ptr = &db->index[i];
	}
        dico_sort(db->suf_index, db->numwords, sizeof(db->suf_index[0]),
		  compare_rev_entry, db);
    }
    return 0;
}    


static int exact_match(struct dictdb *, const char *, struct result *);
static int prefix_match(struct dictdb *, const char *, struct result *);
static int suffix_match(struct dictdb *, const char *, struct result *);

#define RESERVED_WORD(db, word)					    \
    (!(db)->show_dictorg_entries				    \
     && ((strlen(word) >= DICTORG_ENTRY_PREFIX_LEN		    \
	 && memcmp(word, DICTORG_ENTRY_PREFIX,			    \
		  DICTORG_ENTRY_PREFIX_LEN) == 0)		    \
	|| (strlen(word) >= DICTORG_ALT_ENTRY_PREFIX_LEN	    \
	&& memcmp(word, DICTORG_ALT_ENTRY_PREFIX,		    \
		      DICTORG_ALT_ENTRY_PREFIX_LEN) == 0)))	    \


static struct strategy_def strat_tab[] = {
    { { "exact", "Match words exactly" }, exact_match },
    { { "prefix", "Match word prefixes" }, prefix_match },
    { { "suffix", "Match word suffixes" }, suffix_match }
};

static entry_match_t
find_matcher(const char *strat)
{
    int i;
    for (i = 0; i < DICO_ARRAY_SIZE(strat_tab); i++) 
	if (strcmp(strat, strat_tab[i].strat.name) == 0)
	    return strat_tab[i].match;
    return NULL;
}

static int
register_strategies(void)
{
    int i;
    for (i = 0; i < DICO_ARRAY_SIZE(strat_tab); i++) 
	dico_strategy_add(&strat_tab[i].strat);
    return 0;
}

static int
compare_entry_ptr(const void *a, const void *b, void *closure)
{
    const struct index_entry *epa = *(const struct index_entry **)a;
    const struct index_entry *epb = *(const struct index_entry **)b;
    struct dictdb *db = closure;
    return headword_compare_allchars(epa->word, epb->word, db, 0);
}

static int
uniq_comp(const void *a, const void *b, void *closure)
{
    const struct index_entry *epa = a;
    const struct index_entry *epb = b;
    struct dictdb *db = closure;
    /* Prefer original headword over the indexed one. */
    return headword_compare(epa->orig ? epa->orig : epa->word,
			    epb->orig ? epb->orig : epb->word, db);
}

static int
common_match(struct dictdb *db, const char *word,
	     int (*compare)(const void *, const void *, void *),
	     int unique, struct result *res)
{
    struct index_entry x, *ep;
    
    x.word = (char*) word;
    x.length = strlen(word);
    x.wordlen = utf8_strlen(word);
    compare_count = 0;
    ep = dico_bsearch(&x, db->index, db->numwords, sizeof(db->index[0]),
		      compare, db);
    if (ep) {
	res->type = result_match;
	res->db = db;
	res->list = dico_list_create();
	if (!res->list) {
	    DICO_LOG_MEMERR();
	    return 0;
	}
	res->itr = NULL;
	if (unique) {
	    dico_list_set_comparator(res->list, uniq_comp, db);
	    dico_list_set_flags(res->list, DICO_LIST_COMPARE_TAIL);
	}
	for (; ep < db->index + db->numwords
		 && compare(&x, ep, db) == 0; ep++)
	    if (!RESERVED_WORD(db, ep->word))
		dico_list_append(res->list, ep);
	res->compare_count = compare_count;
	return 0;
    }
    return 1;
}


static int
exact_match(struct dictdb *db, const char *word, struct result *res)
{
    return common_match(db, word, compare_index_entry, 0, res);
}

static int
compare_prefix(const void *a, const void *b, void *closure)
{
    const struct index_entry *pkey = a;
    const struct index_entry *pelt = b;
    struct dictdb *db = closure;
    size_t wordlen = pkey->wordlen;
    compare_count++;
    if (pelt->wordlen < wordlen)
	return -1;
    return headword_compare_allchars(pkey->word, pelt->word, db, wordlen);
}

static int
prefix_match(struct dictdb *db, const char *word, struct result *res)
{
    return common_match(db, word, compare_prefix, 1, res);
}

static int
compare_rev_prefix(const void *a, const void *b, void *closure)
{
    const struct rev_entry *pkey = a;
    const struct rev_entry *pelt = b;
    struct dictdb *db = closure;
    size_t wordlen = pkey->ptr->wordlen;
    if (pelt->ptr->wordlen < wordlen)
	wordlen = pelt->ptr->wordlen;
    compare_count++;
    return headword_compare_allchars(pkey->word, pelt->word, db, wordlen);
}

static int
suffix_match(struct dictdb *db, const char *word, struct result *res)
{
    struct rev_entry x, *ep;
    struct index_entry ent;
    int rc;
    
    if (init_suffix_index(db)) {
	DICO_LOG_MEMERR();
	return 1;
    }
    
    ent.length = strlen(word);
    x.word = malloc(ent.length + 1);
    if (!x.word) {
	DICO_LOG_MEMERR();
	return 1;
    }
    ent.wordlen = utf8_strlen(word);

    revert_word(x.word, word, ent.length);
    x.ptr = &ent;
    
    compare_count = 0;
    ep = dico_bsearch(&x, db->suf_index, db->numwords, sizeof(db->suf_index[0]),
		      compare_rev_prefix, db);
    if (ep) {
	struct rev_entry *p;
	struct index_entry **tmp;
	size_t i;
	size_t count = 0;
	dico_list_t list;

	for (p = ep;
	     p < db->suf_index + db->numwords
		 && compare_rev_prefix(&x, p, db) == 0; p++)
	    count++;

	tmp = calloc(count, sizeof(*tmp));
	if (!tmp) {
	    DICO_LOG_MEMERR();
	    free(x.word);
	    return 1;
	} 

	for (i = 0; i < count; i++) 
	    if (!RESERVED_WORD(db, ep[i].ptr->word)) 
		tmp[i] = ep[i].ptr;
	
	count = i;
	dico_sort(tmp, count, sizeof(tmp[0]), compare_entry_ptr, db);

	list = dico_list_create();
	if (!list) {
	    DICO_LOG_MEMERR();
	    free(x.word);
	    free(tmp);
	    return 1;
	}
	dico_list_set_comparator(list, uniq_comp, db);
	dico_list_set_flags(list, DICO_LIST_COMPARE_TAIL);
	for (i = 0; i < count; i++) 
	    dico_list_append(list, tmp[i]);
     
	free(tmp);
	res->type = result_match;
	res->list = list;
	res->itr = NULL;
	res->compare_count = compare_count;
	rc = 0;
    } else 
	rc = 1;
    free(x.word);
    return rc;
}


static char *
find_db_entry(struct dictdb *db, const char *name)
{
    struct index_entry x;
    struct index_entry *ep;
    char *buf;
    int rc;
    
    x.word = (char*) name;
    x.length = strlen(name);
    x.wordlen = utf8_strlen(name);
    ep = dico_bsearch(&x, db->index, db->numwords, sizeof(db->index[0]),
		      compare_index_entry, db);
    if (!ep)
	return NULL;
    buf = malloc(ep->size + 1);
    if (!buf) {
	DICO_LOG_MEMERR();
	return NULL;
    }
    dico_stream_seek(db->stream, ep->offset, DICO_SEEK_SET);
    rc = dico_stream_read(db->stream, buf, ep->size, NULL);
    if (rc) {
	dico_log(L_ERR, 0, _("%s: read error: %s"), db->basename,
		 dico_stream_strerror(db->stream, rc));
	free(buf);
	buf = NULL;
    } else 
	buf[ep->size] = 0;
    return buf;
}

static int
get_db_flag(struct dictdb *db, const char *name)
{
    struct index_entry x;
    
    x.word = (char*) name;
    x.length = strlen(name);
    x.wordlen = utf8_strlen(name);
    return dico_bsearch(&x, db->index, db->numwords, sizeof(db->index[0]),
			compare_index_entry, db) != NULL;
}

static char *
mod_info(dico_handle_t hp)
{
    struct dictdb *db = (struct dictdb *) hp;
    return find_db_entry(db, DICTORG_INFO_ENTRY_NAME);
}

static char *
mod_descr(dico_handle_t hp)
{
    struct dictdb *db = (struct dictdb *) hp;
    char *ptr = find_db_entry(db, DICTORG_SHORT_ENTRY_NAME);
    if (ptr) {
	size_t len = dico_trim_nl(ptr);
	if (len >= sizeof(DICTORG_SHORT_ENTRY_NAME)
	    && memcmp(ptr, DICTORG_SHORT_ENTRY_NAME"\n",
		      sizeof(DICTORG_SHORT_ENTRY_NAME)) == 0) {
	    int i;

	    for (i = sizeof(DICTORG_SHORT_ENTRY_NAME); ptr[i]; i++)
		if (!isspace(ptr[i]))
		    break;
	    
	    memmove(ptr, ptr + i, len - i + 1);
	}
    }
    return ptr;
}

static char *
mod_mime_header(dico_handle_t hp)
{
    struct dictdb *db = (struct dictdb *) hp;
    return find_db_entry(db, DICTORG_ENTRY_MIME_HEADER);
}


static dico_result_t
_match_simple(struct dictdb *db, entry_match_t match, const char *word)
{
    struct result *res;
    
    res = malloc(sizeof(*res));
    if (!res)
	return NULL;
    res->db = db;
    if (match(db, word, res)) {
	free(res);
	res = NULL;
    } else
	res->compare_count = compare_count;
    return (dico_result_t) res;
}

static dico_result_t
_match_all(struct dictdb *db, dico_strategy_t strat, const char *word)
{
    dico_list_t list;
    size_t count, i;
    struct result *res;
    struct dico_key key;
    
    list = dico_list_create();

    if (!list) {
	DICO_LOG_MEMERR();
	return NULL;
    }

    dico_list_set_comparator(list, uniq_comp, db);
    dico_list_set_flags(list, DICO_LIST_COMPARE_TAIL);

    if (dico_key_init(&key, strat, word)) {
	dico_log(L_ERR, 0, _("_match_all: key initialization failed"));
	return NULL;
    }
    
    for (i = 0; i < db->numwords; i++) 
	if (!RESERVED_WORD(db, db->index[i].word)
	    && dico_key_match(&key, db->index[i].word)) 
	    dico_list_append(list, &db->index[i]);

    dico_key_deinit(&key);
    
    compare_count = db->numwords;
	
    count = dico_list_count(list);
    if (count == 0) {
	dico_list_destroy(&list);
	return NULL;
    }

    res = malloc(sizeof(*res));
    if (!res)
	return NULL;
    res->db = db;
    res->type = result_match;
    res->list = list;
    res->itr = NULL;
    res->compare_count = compare_count;
    return (dico_result_t) res;
}

static dico_result_t
mod_match(dico_handle_t hp, const dico_strategy_t strat, const char *word)
{
    struct dictdb *db = (struct dictdb *) hp;
    entry_match_t match;

    if (RESERVED_WORD(db, word))
	return NULL;

    match = find_matcher(strat->name);
    if (match)
	return _match_simple(db, match, word);
    else if (strat->sel) 
	return _match_all(db, strat, word);
    return NULL;
}

static dico_result_t
mod_define(dico_handle_t hp, const char *word)
{
    struct dictdb *db = (struct dictdb *) hp;
    struct result res, *rp;
    int rc;

    if (RESERVED_WORD(db, word))
	return NULL;
    
    rc = common_match(db, word, compare_index_entry, 0, &res);
    if (rc)
	return NULL;
    rp = malloc(sizeof(*rp));
    if (!rp) {
	DICO_LOG_MEMERR();
	dico_list_destroy(&res.list);
	return NULL;
    }
    *rp = res;
    rp->type = result_define;
    return (dico_result_t) rp;
}

static void
printdef(dico_stream_t str, struct dictdb *db, const struct index_entry *ep)
{
    size_t size = ep->size;
    char buf[128];
    int rc;
    
    if (dico_stream_seek(db->stream, ep->offset, SEEK_SET) < 0) {
	dico_log(L_ERR, 0, _("%s: seek error: %s"), db->basename,
		 dico_stream_strerror(db->stream,
				      dico_stream_last_error(db->stream)));
	return;
    }
    while (size) {
	size_t rdsize = size;
	if (rdsize > sizeof(buf))
	    rdsize = sizeof(buf);
	if ((rc = dico_stream_read(db->stream, buf, rdsize, NULL))) {
	    dico_log(L_ERR, 0, _("%s: read error: %s"), db->basename,
		     dico_stream_strerror(db->stream, rc));
	    break;
	}
	dico_stream_write(str, buf, rdsize);
	size -= rdsize;
    }
}

static int
mod_output_result (dico_result_t rp, size_t n, dico_stream_t str)
{
    struct result *res = (struct result *) rp;
    const struct index_entry *ep;

    if (!res->itr) {
	res->itr = dico_list_iterator(res->list);
	if (!res->itr)
	    return 1;
    }
    ep = dico_iterator_item(res->itr, n);
    
    
    switch (res->type) {
    case result_match: {
	char *headword = ep->orig ? ep->orig : ep->word;
	dico_stream_write(str, headword, strlen(headword));
	break;
    }
	
    case result_define:
	printdef(str, res->db, ep);
    }
    return 0;
}

static size_t
mod_result_count (dico_result_t rp)
{
    struct result *res = (struct result *) rp;
    return dico_list_count(res->list);
}

static size_t
mod_compare_count (dico_result_t rp)
{
    struct result *res = (struct result *) rp;
    return res->compare_count;
}

static void
mod_free_result(dico_result_t rp)
{
    struct result *res = (struct result *) rp;
    dico_iterator_destroy(&res->itr);
    dico_list_destroy(&res->list);
    free(rp);
}

struct dico_database_module DICO_EXPORT(dictorg, module) = {
    .dico_version = DICO_MODULE_VERSION,
    .dico_capabilities = DICO_CAPA_NONE,
    .dico_init = mod_init,
    .dico_init_db = mod_init_db,
    .dico_free_db = mod_free_db,
    .dico_db_info = mod_info,
    .dico_db_descr = mod_descr,
    .dico_match = mod_match,
    .dico_define = mod_define,
    .dico_output_result = mod_output_result,
    .dico_result_count = mod_result_count,
    .dico_compare_count = mod_compare_count,
    .dico_free_result = mod_free_result,
    .dico_db_mime_header = mod_mime_header
};
