/* This file is part of GNU Dico.
   Copyright (C) 2018-2019 Sergey Poznyakoff

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

struct dico_handle_struct {
    char *name;
    unsigned int initialized:1;
    unsigned int is_virtual:1;
    size_t vdb_count;
    struct vdb_member vdb_memb[1];
};

struct dico_result_struct {
    struct dico_handle_struct *vdb;
    dicod_db_result_t *vdres[1];
};

int
vdb_free(void *item, void *data)
{
    struct vdb_member *memb = item;
    free(memb->name);
    free(memb);
    return 0;
}

dico_list_t
vdb_list_create(void)
{
    dico_list_t lst = xdico_list_create();
    dico_list_set_free_item(lst, vdb_free, NULL);
    return lst;
}

static struct dico_result_struct *
virtual_result_new(struct dico_handle_struct *vdb)
{
    struct dico_result_struct *result;

    result = xzalloc(sizeof(*result)
		     + (vdb->vdb_count - 1) * sizeof(result->vdres[0]));
    result->vdb = vdb;
    return result;
}

static int virtual_free_db(dico_handle_t hp);

static int
db_name_cmp(const void *item, const void *data, void *closure)
{
    const dicod_database_t *db = item;
    
    if (!db->name)
	return 1;
    return strcmp(db->name, (const char*)data);
}

dicod_database_t *
find_database_all(const char *name)
{
    dicod_database_t *res;
    dico_list_comp_t oldcmp = dico_list_get_comparator(database_list);
    void *olddata = dico_list_get_comparator_data(database_list);
    dico_list_set_comparator(database_list, db_name_cmp, NULL);
    res = dico_list_locate(database_list, (void*) name);
    dico_list_set_comparator(database_list, oldcmp, olddata);
    return res;
}

static dico_handle_t
virtual_init_db_ext(const char *dbname, int argc, char **argv, void *extra)
{
    struct dico_handle_struct *vdb;
    dico_iterator_t itr;
    struct vdb_member *mp;
    int err = 0;
    dico_list_t dblist = extra;
    size_t dbcount = dico_list_count(dblist);
    size_t i = 0;
    
    vdb = xzalloc(sizeof(*vdb) + (dbcount - 1) * sizeof(vdb->vdb_memb[0]));
    vdb->name = xstrdup(dbname);
    vdb->vdb_count = dbcount;

    itr = xdico_list_iterator(dblist);
    for (mp = dico_iterator_first(itr); mp; mp = dico_iterator_next(itr)) {
	dicod_database_t *db = find_database_all(mp->name);
	if (db) {
	    vdb->vdb_memb[i] = *mp;
	    i++;
	} else {
	    dico_log(LOG_ERR, 0, "no such database: %s", mp->name);
	    err = 1;
	}
    }
    dico_iterator_destroy(&itr);
    vdb->vdb_count = i;
    if (err) {
	virtual_free_db((dico_handle_t)vdb);
	vdb = NULL;
    }
    return (dico_handle_t)vdb;
}

static int
virtual_free_db(dico_handle_t vdb)
{
    /* DB list is associated with the dicod_dictionary_t structure and will
       be freed along with it. */
    free(vdb->name);
    free(vdb);
    return 0;
}

static int
virtual_open(dico_handle_t vdb)
{
    if (!vdb->initialized) {
	size_t i, j;
	size_t n = vdb->vdb_count;
	int is_virtual = 1;
	
	for (i = j = 0; i < n; i++) {
	    dicod_database_t *db = find_database_all(vdb->vdb_memb[i].name);
	    if (db) {
		vdb->vdb_memb[i].db = db;
		if (i > j)
		    vdb->vdb_memb[j] = vdb->vdb_memb[i];
		if (vdb->vdb_memb[j].cond != cond_any)
		    is_virtual = 0;
		j++;
	    }
	}
	vdb->vdb_count = j;
	vdb->initialized = 1;
	vdb->is_virtual = is_virtual;
    }
    return 0;
}

static int
virtual_db_flags(dico_handle_t vdb)
{
    return vdb->is_virtual ? DICO_DBF_VIRTUAL : DICO_DBF_DEFAULT; 
}

static int
vdb_member_ok(struct vdb_member const *memb)
{
    int res = 0;

    if (database_session_visibility(memb->db) == 0)
	return 0;

    switch (memb->cond) {
    case cond_any:
	res = 1;
	break;
    case cond_mime:
	res = option_mime;
	break;
    case cond_nomime:
	res = !option_mime;
	break;
    }
    return res;
}
	
static dico_result_t
virtual_match(dico_handle_t vdb, const dico_strategy_t strat, const char *word)
{
    size_t i;
    size_t n = vdb->vdb_count;
    struct dico_result_struct *result = virtual_result_new(vdb);
    
    for (i = 0; i < n; i++) {
	if (vdb_member_ok(&vdb->vdb_memb[i])) {
	    result->vdres[i] = dicod_database_match(vdb->vdb_memb[i].db,
						    strat, word);
	} else {
	    result->vdres[i] = NULL;
	}
    } 
    return result;
}

static dico_result_t
virtual_define(dico_handle_t vdb, const char *word)
{
    size_t i;
    size_t n = vdb->vdb_count;
    struct dico_result_struct *result = virtual_result_new(vdb);
    
    for (i = 0; i < n; i++) {
	if (vdb_member_ok(&vdb->vdb_memb[i])) {
	    result->vdres[i] = dicod_database_define(vdb->vdb_memb[i].db, word);
	} else {
	    result->vdres[i] = NULL;
	}
    } 
    return result;
}

static int
vdres_locate(dico_result_t result, size_t *pn)
{
    size_t i = 0;

    while (1) {
	if (i == result->vdb->vdb_count)
	    return 1;
	if (result->vdres[i]) {
	    size_t s = dicod_db_result_count(result->vdres[i]);
	    if (s > *pn)
		break;
	    *pn -= s;
	}
	i++;
    }
    return i;
}

static int
virtual_output_result(dico_result_t result, size_t n, dico_stream_t str)
{
    size_t i = vdres_locate(result, &n);
    return dicod_db_result_output(result->vdres[i], n, str);
}

static size_t
virtual_result_count (dico_result_t result)
{
    size_t i;
    size_t count = 0;
    for (i = 0; i < result->vdb->vdb_count; i++) {
	if (result->vdres[i])
	    count += dicod_db_result_count(result->vdres[i]);
    }
    return count;
}

static size_t
virtual_compare_count (dico_result_t result)
{
    size_t i;
    size_t count = 0;
    for (i = 0; i < result->vdb->vdb_count; i++) {
	if (result->vdres[i])
	    count += dicod_db_result_compare_count(result->vdres[i]);
    }
    return count;
}

static void
virtual_free_result(dico_result_t result)
{
    size_t i;
    for (i = 0; i < result->vdb->vdb_count; i++) {
	if (result->vdres[i]) {
	    dicod_db_result_free(result->vdres[i]);
	    result->vdres[i] = NULL;
	}
    }
    free(result);
}

static char *
virtual_descr(dico_handle_t vdb)
{
    size_t i;
    size_t n = vdb->vdb_count;
    char *prev = NULL;
    
    for (i = 0; i < n; i++) {
	if (vdb_member_ok(&vdb->vdb_memb[i])) {
	    char *p = dicod_database_get_descr(vdb->vdb_memb[i].db);
	    if (prev && strcmp(prev, p)) {
		/* descriptions differ; too bad */
		free(prev);
		prev = NULL;
		break;
	    }
	    prev = xstrdup(p);
	    dicod_database_free_descr(vdb->vdb_memb[i].db, p);
	}
    }

    return prev;
}

static char *
virtual_info(dico_handle_t vdb)
{
    size_t i;
    size_t n = vdb->vdb_count;
    char *prev = NULL;
    
    for (i = 0; i < n; i++) {
	if (vdb_member_ok(&vdb->vdb_memb[i])) {
	    char *p = dicod_database_get_info(vdb->vdb_memb[i].db);
	    if (prev && strcmp(prev, p)) {
		/* descriptions differ; too bad */
		free(prev);
		prev = NULL;
		break;
	    }
	    prev = xstrdup(p);
	    dicod_database_free_info(vdb->vdb_memb[i].db, p);
	}
    }

    return prev;
}

static dicod_database_t *
virtual_result_db(dico_result_t result, size_t n)
{
    size_t i = vdres_locate(result, &n);
    return dicod_db_result_db(result->vdres[i], n, result_db_all);
}

struct dico_database_module virtual_builtin_module = {
    .dico_version        =  DICO_MODULE_VERSION,
    .dico_capabilities   =  DICO_CAPA_INIT_EXT,
    .dico_init_db_ext    =  virtual_init_db_ext,
    .dico_free_db        =  virtual_free_db,
    .dico_open           =  virtual_open,
    .dico_match          =  virtual_match,
    .dico_define         =  virtual_define,
    .dico_result_count   =  virtual_result_count,
    .dico_compare_count  =  virtual_compare_count,
    .dico_free_result    =  virtual_free_result,
    .dico_db_flags       =  virtual_db_flags,
    .dico_output_result  =  virtual_output_result,
    .dico_db_info        =  virtual_info,
    .dico_db_descr       =  virtual_descr,
    .dico_result_db      =  virtual_result_db
};
