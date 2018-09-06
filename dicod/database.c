/* This file is part of GNU Dico.
   Copyright (C) 1998-2018 Sergey Poznyakoff

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

int
dicod_database_init(dicod_database_t *db)
{
    dicod_module_instance_t *inst = db->instance;

    if (inst->module->dico_capabilities & DICO_CAPA_NODB) {
	dico_log(L_ERR, 0, _("cannot initialize database `%s': "
			     "module `%s' does not support databases"),
		 db->command, inst->ident);
	return 1;
    }
    
    if (inst->module->dico_init_db) {
	db->mod_handle = inst->module->dico_init_db(db->name,
						    db->argc,
						    db->argv,
	                                            db->extra);
	if (!db->mod_handle) {
	    dico_log(L_ERR, 0, _("cannot initialize database `%s'"),
		     db->command);
	    return 1;
	}
    }
    return 0;
}

int
dicod_database_open(dicod_database_t *db)
{
    dicod_module_instance_t *inst = db->instance;

    if (inst->module->dico_open) {
	if (inst->module->dico_open(db->mod_handle)) {
	    dico_log(L_ERR, 0, _("cannot open database `%s'"), db->command);
	    return 1;
	}
    }

    if (!db->mime_headers
	&& inst->module->dico_version > 2
	&& inst->module->dico_db_mime_header) {
	char *str = inst->module->dico_db_mime_header(db->mod_handle);
	if (str) {
	    if (dico_header_parse(&db->mime_headers, str)) {
		dico_log(L_WARN, errno,
			 "database %s: can't parse mime headers \"%s\"",
			 db->name,
			 str);
	    }
	    free(str);
	}
    }

    return 0;
}

int
dicod_database_close(dicod_database_t *db)
{
    int rc = 0;
    
    if (db->mod_handle) {
	dicod_module_instance_t *inst = db->instance;
	if (inst->module->dico_close) 
	    rc = inst->module->dico_close(db->mod_handle);
    }
    return rc;
}

int
dicod_database_deinit(dicod_database_t *db)
{
    int rc = 0;
    
    if (db->mod_handle) {
	dicod_module_instance_t *inst = db->instance;
	if (inst->module->dico_free_db) {
	    rc = inst->module->dico_free_db(db->mod_handle);
	    db->mod_handle = NULL;
	}
    }
    return rc;
}

char *
dicod_database_get_descr(dicod_database_t *db)
{
    if (db->descr)
	return db->descr;
    else {
	dicod_module_instance_t *inst = db->instance;
	if (inst->module->dico_db_descr)
	    return inst->module->dico_db_descr(db->mod_handle);
    }
    return NULL;
}

void
dicod_database_free_descr(dicod_database_t *db, char *descr)
{
    if (descr && descr != db->descr)
	free(descr);
}

char *
dicod_database_get_info(dicod_database_t *db)
{
    if (db->info)
	return db->info;
    else {
	dicod_module_instance_t *inst = db->instance;
	if (inst->module->dico_db_info)
	    return inst->module->dico_db_info(db->mod_handle);
    }
    return NULL;
}

void
dicod_database_free_info(dicod_database_t *db, char *info)
{
    if (info && info != db->info)
	free(info);
}

void
dicod_database_get_languages(dicod_database_t *db, dico_list_t dlist[])
{
    if (!(db->flags & DICO_DBF_LANG)) {
	dicod_module_instance_t *inst = db->instance;
	if (inst->module->dico_db_lang) {
	    /* FIXME: Return code? */
	    inst->module->dico_db_lang(db->mod_handle, db->langlist);
	    if (db->langlist[0] || db->langlist[1]) {		
		if (!db->langlist[0])
		    db->langlist[0] = dicod_langlist_copy(db->langlist[1]);
		else if (!db->langlist[1])
		    db->langlist[1] = dicod_langlist_copy(db->langlist[0]);
	    }
	    if (dicod_any_lang_list_p(db->langlist[0]))
		dico_list_destroy(&db->langlist[0]);
	    if (dicod_any_lang_list_p(db->langlist[1]))
		dico_list_destroy(&db->langlist[1]);
	}
	db->flags |= DICO_DBF_LANG;
    }
    dlist[0] = db->langlist[0];
    dlist[1] = db->langlist[1];
}

dico_result_t
dicod_database_match(dicod_database_t *db, const dico_strategy_t strat,
		     const char *word)
{
    struct dico_database_module *mod = db->instance->module;
    return mod->dico_match(db->mod_handle, strat, word);
}

dico_result_t
dicod_database_define(dicod_database_t *db, const char *word)
{
    struct dico_database_module *mod = db->instance->module;
    return mod->dico_define(db->mod_handle, word);
}

// FIXME: dico_result_t should contain a pointer to db
size_t
dicod_database_result_count(dicod_database_t *db, dico_result_t res)
{
    return db->instance->module->dico_result_count(res);
}

// FIXME: See above
size_t
dicod_database_compare_count(dicod_database_t *db, dico_result_t res)
{
    struct dico_database_module *mod = db->instance->module;
    return mod->dico_compare_count ? mod->dico_compare_count(res) : 0;
}

// FIXME: See above
void
dicod_database_result_free(dicod_database_t *db, dico_result_t res)
{
    struct dico_database_module *mod = db->instance->module;
    mod->dico_free_result(res);
}

// FIXME: See above
int
dicod_database_result_output(dicod_database_t *db, dico_result_t res,
			     size_t n, dico_stream_t str)
{
    struct dico_database_module *mod = db->instance->module;
    return mod->dico_output_result(res, n, str);
}

dico_assoc_list_t
dicod_database_mime_header(dicod_database_t *db, dico_result_t res)
{
    dico_assoc_list_t hdr = NULL;
    struct dico_database_module *mod = db->instance->module;

    if (db->mime_headers)
	hdr = dico_assoc_dup(db->mime_headers);
    else
	dico_header_parse(&hdr, NULL);
    
    if (mod->dico_result_headers)
	mod->dico_result_headers(res, hdr);

    return hdr;
}

int
dicod_database_flags(dicod_database_t const *db)
{
    struct dico_database_module *mod = db->instance->module;
    if (mod->dico_version > 2 && mod->dico_db_flags)
	return mod->dico_db_flags(db->mod_handle) & DICO_DBF_MASK;
    return DICO_DBF_DEFAULT;
}
    
