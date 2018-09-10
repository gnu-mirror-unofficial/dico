/* This file is part of GNU Dico.
   Copyright (C) 2018 Sergey Poznyakoff

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

struct virtual_database {
    char *name;
    unsigned int initialized:1;
    unsigned int is_virtual:1;
    size_t vdb_count;
    struct vdb_member vdb_memb[1];
};

struct virtual_result {
    struct virtual_database *vdb;
    dico_result_t vdres[1];
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

static struct virtual_result *
virtual_result_new(struct virtual_database *vdb)
{
    struct virtual_result *result;

    result = xzalloc(sizeof(*result)
		     + (vdb->vdb_count - 1) * sizeof(result->vdres[0]));
    result->vdb = vdb;
    return result;
}

static int virtual_free_db(dico_handle_t hp);

static int
db_name_cmp(const void *item, void *data)
{
    const dicod_database_t *db = item;
    
    if (!db->name)
	return 1;
    return strcmp(db->name, (const char*)data);
}

dicod_database_t *
find_database_all(const char *name)
{
    dico_list_comp_t oldcmp = dico_list_set_comparator(database_list,
						       db_name_cmp);
    dicod_database_t *res = dico_list_locate(database_list, (void*) name);
    dico_list_set_comparator(database_list, oldcmp);
    return res;
}

static dico_handle_t
virtual_init_db_ext(const char *dbname, int argc, char **argv, void *extra)
{
    struct virtual_database *vdb;
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
virtual_free_db(dico_handle_t hp)
{
    struct virtual_database *vdb = (struct virtual_database*)hp;
    /* DB list is associated with the dicod_dictionary_t structure and will
       be freed along with it. */
    free(vdb->name);
    free(vdb);
    return 0;
}

static int
virtual_open(dico_handle_t dp)
{
    struct virtual_database *vdb = (struct virtual_database*)dp;
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
virtual_db_flags(dico_handle_t dp)
{
    struct virtual_database *vdb = (struct virtual_database*)dp;
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
virtual_match(dico_handle_t hp, const dico_strategy_t strat, const char *word)
{
    struct virtual_database *vdb = (struct virtual_database*)hp;
    size_t i;
    size_t n = vdb->vdb_count;
    struct virtual_result *result = virtual_result_new(vdb);
    
    for (i = 0; i < n; i++) {
	if (vdb_member_ok(&vdb->vdb_memb[i])) {
	    result->vdres[i] = dicod_database_match(vdb->vdb_memb[i].db,
						    strat, word);
	} else {
	    result->vdres[i] = NULL;
	}
    } 
    return (dico_result_t) result;
}

static dico_result_t
virtual_define(dico_handle_t hp, const char *word)
{
    struct virtual_database *vdb = (struct virtual_database*)hp;
    size_t i;
    size_t n = vdb->vdb_count;
    struct virtual_result *result = virtual_result_new(vdb);
    
    for (i = 0; i < n; i++) {
	if (vdb_member_ok(&vdb->vdb_memb[i])) {
	    result->vdres[i] = dicod_database_define(vdb->vdb_memb[i].db, word);
	} else {
	    result->vdres[i] = NULL;
	}
    } 
    return (dico_result_t) result;
}

static int
virtual_output_result (dico_result_t rp, size_t n, dico_stream_t str)
{
    struct virtual_result *result = (struct virtual_result *)rp;
    size_t i = 0;

    while (1) {
	if (i == result->vdb->vdb_count)
	    return 1;
	if (result->vdres[i]) {
	    size_t s = dicod_database_result_count(result->vdb->vdb_memb[i].db,
						   result->vdres[i]);
	    if (s > n)
		break;
	    n -= s;
	}
	i++;
    }
    return dicod_database_result_output(result->vdb->vdb_memb[i].db,
					result->vdres[i],
					n, str);
}

static size_t
virtual_result_count (dico_result_t rp)
{
    struct virtual_result *result = (struct virtual_result *)rp;
    size_t i;
    size_t count = 0;
    for (i = 0; i < result->vdb->vdb_count; i++) {
	if (result->vdres[i])
	    count += dicod_database_result_count(result->vdb->vdb_memb[i].db,
						 result->vdres[i]);
    }
    return count;
}

static size_t
virtual_compare_count (dico_result_t rp)
{
    struct virtual_result *result = (struct virtual_result *)rp;
    size_t i;
    size_t count = 0;
    for (i = 0; i < result->vdb->vdb_count; i++) {
	if (result->vdres[i])
	    count += dicod_database_compare_count(result->vdb->vdb_memb[i].db,
						  result->vdres[i]);
    }
    return count;
}

static void
virtual_free_result(dico_result_t rp)
{
    struct virtual_result *result = (struct virtual_result *)rp;
    size_t i;
    for (i = 0; i < result->vdb->vdb_count; i++) {
	if (result->vdres[i])
	    dicod_database_result_free(result->vdb->vdb_memb[i].db,
				       result->vdres[i]);
    }
    free(rp);
}

static int
virtual_result_headers(dico_result_t rp, dico_assoc_list_t hdr)
{  
    struct virtual_result *result = (struct virtual_result *)rp;
    size_t i;
    int err = 0;
    for (i = 0; i < result->vdb->vdb_count; i++) {
	if (result->vdres[i]) {
	    dico_assoc_list_t rhdr =
		dicod_database_mime_header(result->vdb->vdb_memb[i].db,
					   result->vdres[i]);
	    if (rhdr) {
		struct dico_assoc *p;
		dico_iterator_t itr;

		if (dico_assoc_count(rhdr) == 0) {
		    err = 1;
		    break;
		}
		
		itr = dico_assoc_iterator(rhdr);
		for (p = dico_iterator_first(itr); p;
		     p = dico_iterator_next(itr)) {
		    if (dico_assoc_append(hdr, p->key, p->value)) {
			err = 1;
			break;
		    }
		}
		dico_iterator_destroy(&itr);
		dico_assoc_destroy(&rhdr);
	    } else
		err = 1;
	}
	if (err)
	    return err;
    }
    return 0;
}

struct dico_database_module virtual_builtin_module = {
    .dico_version        =  DICO_MODULE_VERSION,
    .dico_capabilities   =  DICO_CAPA_INIT_EXT,
    .dico_init_db_ext    =  virtual_init_db_ext,
    .dico_free_db        =  virtual_free_db,
    .dico_open           =  virtual_open,
    .dico_match          =  virtual_match,
    .dico_define         =  virtual_define,
    .dico_output_result  =  virtual_output_result,
    .dico_result_count   =  virtual_result_count,
    .dico_compare_count  =  virtual_compare_count,
    .dico_free_result    =  virtual_free_result,
    .dico_db_flags       =  virtual_db_flags,
    .dico_result_headers =  virtual_result_headers
};
