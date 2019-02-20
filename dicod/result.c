/* This file is part of GNU Dico.
   Copyright (C) 1998-2019 Sergey Poznyakoff

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

#include <dicod.h>

enum {
    DBRF_NONE = 0,
    DBRF_RCOUNT = 0x01,
    DBRF_CCOUNT = 0x02
};

dicod_db_result_t *
dicod_db_result_alloc(dicod_database_t *db, dico_result_t res)
{
    dicod_db_result_t *dbr;
    if (!res)
	return NULL;
    dbr = xmalloc(sizeof(*dbr));
    dbr->flags = DBRF_NONE;
    dbr->db = db;
    dbr->res = res;
    return dbr;
}

static inline struct dico_database_module *
dicod_db_result_module(dicod_db_result_t *dbr)
{
    return dbr->db->instance->module;
}

void
dicod_db_result_free(dicod_db_result_t *dbr)
{
    dicod_db_result_module(dbr)->dico_free_result(dbr->res);
    free(dbr);
}

size_t
dicod_db_result_count(dicod_db_result_t *dbr)
{
    if (!(dbr->flags & DBRF_RCOUNT)) {
	dbr->rcount = dicod_db_result_module(dbr)->dico_result_count(dbr->res);
	dbr->flags |= DBRF_RCOUNT;
    }
    return dbr->rcount;
}

size_t
dicod_db_result_compare_count(dicod_db_result_t *dbr)
{
    if (!(dbr->flags & DBRF_CCOUNT)) {
	dbr->ccount = dicod_db_result_module(dbr)->dico_compare_count(dbr->res);
	dbr->flags |= DBRF_CCOUNT;
    }
    return dbr->ccount;
}

int
dicod_db_result_output(dicod_db_result_t *dbr, size_t n, dico_stream_t str)
{
    return dicod_db_result_module(dbr)->dico_output_result(dbr->res, n, str);
}

dico_assoc_list_t
dicod_db_result_mime_header(dicod_db_result_t *dbr, size_t n)
{
    dico_assoc_list_t hdr = NULL;
    struct dico_database_module *mod = dicod_db_result_module(dbr);
    dicod_database_t *db = dicod_db_result_db(dbr, n, result_db_all);
    
    if (db->mime_headers)
	hdr = dico_assoc_dup(db->mime_headers);
    else
	dico_header_parse(&hdr, NULL);
    
    if (mod->dico_result_headers) {
	dico_assoc_list_t tmp = dico_assoc_dup(hdr);
	if (mod->dico_result_headers(dbr->res, tmp) == 0) {
	    dico_assoc_destroy(&hdr);
	    hdr = tmp;
	} else {
	    dico_assoc_destroy(&tmp);
	}
    }

    return hdr;
}

dicod_database_t *
dicod_db_result_db(dicod_db_result_t *dbr, size_t n, int flag)
{
    dicod_database_t *db = NULL;
    struct dico_database_module *mod = dicod_db_result_module(dbr);
    if (mod->dico_result_db) {
	db = mod->dico_result_db(dbr->res, n);
	if (db && flag == result_db_visible && !database_is_visible(db))
	    db = dbr->db;
    }
    return db ? db : dbr->db;
}



    
