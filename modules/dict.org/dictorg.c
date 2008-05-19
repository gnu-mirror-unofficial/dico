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

#include "dictorg.h"

static char *dbdir;

static void
memerr(const char *fname)
{
    dico_log(L_ERR, 0, _("%s: not enough memory"), fname);
}

int
mod_init(int argc, char **argv)
{
    if (argc) {
	struct stat st;
	
	if (argc != 2) {
	    dico_log(L_ERR, 0, _("mod_init: wrong number of arguments"));
	    return 1;
	}
	if (stat(argv[1], &st)) {
	    dico_log(L_ERR, errno, _("mod_init: cannot stat `%s'"),
		     argv[1]);
	    return 1;
	}
	if (!S_ISDIR(st.st_mode)) {
	    dico_log(L_ERR, 0, _("mod_init: `%s' is not a directory"),
		     argv[1]);
	    return 1;
	}
	if (access(argv[1], R_OK)) {
	    dico_log(L_ERR, 0, _("mod_init: `%s' is not readable"),
		     argv[1]);
	    return 1;
	}
	dbdir = strdup(argv[1]);
    }
    return 0;
}

void
free_db(struct dictdb *db)
{
    size_t i;
    /* FIXME */
    for (i = 0; i < db->numwords; i++) {
	if (!db->index[i].word)
	    break;
	free(db->index[i].word);
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

int
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
		  dico_list_t list, char *buf)
{
    struct utf8_iterator itr;
    struct index_entry idx, *ep;
    int nfield = 0;
    int rc;
    
    memset(&itr, 0, sizeof(itr));
    
    utf8_iter_first(&itr, (unsigned char *)buf);

    rc = 0;
    for (nfield = 0; nfield < 3; nfield++) {
	unsigned char *start, *end;
	size_t len;
	
	/* Skip whitespace */
	for (; !utf8_iter_end_p(&itr)
		 && utf8_iter_isascii(itr) && ISWS(*itr.curptr);
	     utf8_iter_next(&itr))
	    ;
	
	if (utf8_iter_end_p(&itr))
	    break;

	start = itr.curptr;
	for (; !utf8_iter_end_p(&itr)
		 && !(utf8_iter_isascii(itr) && ISWS(*itr.curptr));
	     utf8_iter_next(&itr))
	    ;
	end = itr.curptr;
	len = end - start;

	if (nfield == 0) {
	    idx.word = malloc(len + 1);
	    if (!idx.word) {
		memerr("parse_index_entry");
		return 1;
	    }
	    memcpy(idx.word, start, len);
	    idx.word[len] = 0;
	    idx.length = len;
	    idx.wordlen = utf8_strlen(idx.word);
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
		memerr("parse_index_entry");
		rc = 1;
	    }
	}
    }

    if (rc)
	free(idx.word);
    else 
	*ep = idx;
    return rc;
}

static int
free_index_entry(void *item, void *data)
{
    struct index_entry *ep = item;
    free(ep->word);
    free(ep);
    return 0;
}

static int
read_index(struct dictdb *db, const char *idxname)
{
    struct stat st;
    FILE *fp;
    char buf[512]; /* FIXME: fixed size */
    int rc;
    dico_list_t list;
    
    if (stat(idxname, &st)) {
	dico_log(L_ERR, errno, _("open_index: cannot stat `%s'"), idxname);
	return 1;
    }
    if (!S_ISREG(st.st_mode)) {
	dico_log(L_ERR, 0, _("open_index: `%s' is not a regular file"),
		 idxname);
	return 1;
    }
    fp = fopen(idxname, "r");
    if (!fp) {
	dico_log(L_ERR, errno, _("open_index: cannot open `%s'"), idxname);
	return 1;
    }

    list = dico_list_create();
    if (!list) {
	memerr("read_index");
	rc = 1;
    } else {
	dico_list_iterator_t free_fun = NULL;
	dico_iterator_t itr;
	size_t i;
	struct index_entry *ep;
	
	rc = 0;

	i = 0;
	while (fgets(buf, sizeof(buf), fp)) {
	    i++;
	    dico_trim_nl(buf);
	    rc = parse_index_entry(idxname, i, list, buf);
	    if (rc)
		break;
	}
	if (rc) {
	    free_fun = free_index_entry;
	} else {
	    db->numwords = dico_list_count(list);
	    db->index = calloc(db->numwords, sizeof(db->index[0]));
	    itr = dico_iterator_create(list);
	    for (i = 0, ep = dico_iterator_first(itr);
		 ep;
		 i++, ep = dico_iterator_next(itr)) {
		db->index[i] = *ep;
		free(ep);
	    }
	    dico_iterator_destroy(&itr);
	}
	dico_list_destroy(&list, free_index_entry, NULL);
    }

    fclose(fp);
    return rc;
}

static int
open_index(struct dictdb *db)
{
    char *idxname = mkname(db->basename, "index");
    int rc;
    
    if (!idxname) {
	memerr("open_index");
	return 1;
    }

    rc = read_index(db, idxname);
    free(idxname);
    return rc;
}

int
mod_close(dico_handle_t hp)
{
    struct dictdb *db = (struct dictdb *) hp;
    free_db(db);
    return 0;
}

dico_handle_t
mod_open(const char *dbname, int argc, char **argv)
{
    struct dictdb *db;
    char *filename;
    
    if (argc != 2) {
	dico_log(L_ERR, 0, _("mod_open: wrong number of arguments"));
	return NULL;
    }

    if (argv[1][0] != '/') {
	if (dbdir) {
	    filename = dico_full_file_name(dbdir, argv[1]);
	} else {
	    dico_log(L_ERR, 0,
		     _("mod_open: `%s' is not an absolute file name"),
		     argv[1]);
	    return NULL;
	}
    } else
	filename = strdup(argv[1]);

    if (!filename) {
	memerr("mod_open");
	return NULL;
    }
    
    db = malloc(sizeof(*db));
    if (!db) {
	free(filename);
	memerr("mod_open");
	return NULL;
    }
    memset(db, 0, sizeof(*db));

    db->dbname = dbname;
    db->basename = filename;

    if (open_index(db)) {
	free_db(db);
	return NULL;
    }
    
    return (dico_handle_t)db;
}

char *
mod_info(dico_handle_t hp)
{
    /* FIXME */
    return NULL;
}

char *
mod_descr(dico_handle_t hp)
{
    /* FIXME */
    return NULL;
}

dico_result_t
mod_match(dico_handle_t hp, const dico_strategy_t strat, const char *word)
{
    /* FIXME */
    return NULL;
}

dico_result_t
mod_define(dico_handle_t hp, const char *word)
{
    /* FIXME */
    return NULL;
}

int
mod_output_result (dico_result_t rp, size_t n, dico_stream_t str)
{
    /* FIXME */
    return 1;
}

size_t
mod_result_count (dico_result_t rp)
{
    /* FIXME */
    return 0;
}

size_t
mod_compare_count (dico_result_t rp)
{
    /* FIXME */
    return 0;
}

void
mod_free_result(dico_result_t rp)
{
    /* FIXME */
}

struct dico_handler_module DICO_EXPORT(dictorg, module) = {
    mod_init,
    mod_open,
    mod_close,
    mod_info,
    mod_descr,
    mod_match,
    mod_define,
    mod_output_result,
    mod_result_count,
    mod_compare_count,
    mod_free_result
};
