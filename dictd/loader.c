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

#include <dictd.h>


/* Module functions */

/* List of loaded database modules */
dico_list_t /* of dictd_module_t */ module_list;

static int
cmp_module_ident(const void *item, const void *data)
{
    const dictd_module_t *mod = item;
    if (!mod->ident)
	return 1;
    return strcmp(mod->ident, (const char*)data);
}

static dictd_module_t *
find_module(const char *ident)
{
    return dico_list_locate(module_list, ident, cmp_module_ident);
}


/* Load path */
static int
_add_load_dir (void *item, void *unused)
{
    char *str = *(char**)item;
    return lt_dladdsearchdir(str);
}

void
dictd_loader_init()
{
    lt_dlinit();
    lt_dladdsearchdir(DICO_MODDIR);
    dico_list_iterate(module_load_path, _add_load_dir, NULL);
}

#define MODULE_ASSERT(cond)					\
    if (!(cond)) {						\
	lt_dlclose(handle);					\
	dico_log(L_ERR, 0, _("%s: faulty module: (%s) failed"), \
		 argv[0],					\
	         #cond);					\
	return 1;						\
    }

static int
dictd_load_module0(dictd_module_instance_t *inst, int argc, char **argv)
{
    lt_dlhandle handle;
    dictd_module_t *module = find_module(argv[0]);
    struct dico_handler_module *pmod;    
    dico_instance_t inst_handle;
    
    if (module) {
	pmod = module->hmod;
	if (!(pmod->capabilities & DICO_CAPA_MULTI_INSTANCE)) {
	    dico_log(L_ERR, 0,
		     _("cannot load module %s twice: "
		       "module does not support multiple instances"),
		     argv[0]);
	    return 1;
	}
    } else {	     
	handle = lt_dlopenext(argv[0]);
	if (!handle) {
	    dico_log(L_ERR, 0, _("cannot load module %s: %s"), argv[0],
		     lt_dlerror());
	    return 1;
	}

	pmod = (struct dico_handler_module *) lt_dlsym(handle, "module");
	MODULE_ASSERT(pmod);
	MODULE_ASSERT(pmod->version <= DICO_MODULE_VERSION);
	MODULE_ASSERT(pmod->module_init
		      || !(pmod->capabilities & DICO_CAPA_MULTI_INSTANCE));
	MODULE_ASSERT(pmod->module_init_db);
	MODULE_ASSERT(pmod->module_free_db);
	MODULE_ASSERT(pmod->module_match);
	MODULE_ASSERT(pmod->module_define);
	MODULE_ASSERT(pmod->module_output_result);
	MODULE_ASSERT(pmod->module_result_count);
	MODULE_ASSERT(pmod->module_free_result);

	if (pmod->module_open || pmod->module_close)
	    MODULE_ASSERT(pmod->module_open && pmod->module_close);
    }
    
    if (pmod->module_init && pmod->module_init(argc, argv, &inst_handle)) {
	if (!module) 
	    lt_dlclose(handle);
	dico_log(L_ERR, 0, _("%s: initialization failed"), argv[0]);
	return 1;
    }

    if (!module) {
	module = xmalloc(sizeof(*module));
	module->ident = xstrdup(argv[0]);
	module->hmod = pmod;
	module->handle = handle;
	module->refcount = 0;
    }
    module->refcount++;
    
    inst->module = module;
    if (pmod->capabilities & DICO_CAPA_MULTI_INSTANCE)
	inst->inst_handle = inst_handle;
    return 0;
}

int
dictd_load_module(dictd_module_instance_t *inst)
{
    int argc;
    char **argv;
    int rc;
	
    if (dico_argcv_get(inst->command, NULL, NULL, &argc, &argv)) {
	dico_log(L_ERR, rc, _("cannot parse command line `%s'"),
		 inst->command);
	return 1;
    }

    rc = dictd_load_module0(inst, argc, argv);

    dico_argcv_free(argc, argv);
    
    return rc;
}

int
dictd_init_database(dictd_database_t *dp)
{
    dictd_module_instance_t *inst = dp->instance;

    if (inst->module->hmod->module_init_db) {
	dp->mod_handle = inst->module->hmod->module_init_db(inst->inst_handle,
							    dp->name,
							    dp->argc,
							    dp->argv);
	if (!dp->mod_handle) {
	    dico_log(L_ERR, 0, _("cannot initialize database `%s'"),
		     dp->command);
	    return 1;
	}
    }
    return 0;
}

int
dictd_open_database(dictd_database_t *dp)
{
    dictd_module_instance_t *inst = dp->instance;

    if (inst->module->hmod->module_open) {
	if (inst->module->hmod->module_open(dp->mod_handle)) {
	    dico_log(L_ERR, 0, _("cannot open database `%s'"),
		     dp->command);
	    return 1;
	}
    }
    return 0;
}

int
dictd_close_database(dictd_database_t *dp)
{
    int rc;
    
    if (dp->mod_handle) {
	dictd_module_instance_t *inst = dp->instance;
	if (inst->module->hmod->module_close) 
	    rc = inst->module->hmod->module_close(dp->mod_handle);
    } else
	rc = 0;
    return rc;
}

/* FIXME: Unused so far */
int
dictd_free_database(dictd_database_t *dp)
{
    int rc;
    
    if (dp->mod_handle) {
	dictd_module_instance_t *inst = dp->instance;
	if (inst->module->hmod->module_free_db) {
	    rc = inst->module->hmod->module_free_db(dp->mod_handle);
	    dp->mod_handle = NULL;
	}
    } else
	rc = 0;
    return rc;
}

char *
dictd_get_database_descr(dictd_database_t *db)
{
    if (db->descr)
	return db->descr;
    else {
	dictd_module_instance_t *inst = db->instance;
	if (inst->module->hmod->module_db_descr)
	    return inst->module->hmod->module_db_descr(db->mod_handle);
    }
    return NULL;
}

void
dictd_free_database_descr(dictd_database_t *db, char *descr)
{
    if (descr && descr != db->descr)
	free(descr);
}

char *
dictd_get_database_info(dictd_database_t *db)
{
    if (db->info)
	return db->info;
    else {
	dictd_module_instance_t *inst = db->instance;
	if (inst->module->hmod->module_db_info)
	    return inst->module->hmod->module_db_info(db->mod_handle);
    }
    return NULL;
}

void
dictd_free_database_info(dictd_database_t *db, char *info)
{
    if (info && info != db->info)
	free(info);
}


static char nomatch[] = "552 No match";
static size_t nomatch_len = (sizeof(nomatch)-1);


typedef void (*outproc_t)(dictd_database_t *db, dico_result_t res,
			  const char *word, dico_stream_t stream,
			  size_t count);

void
dictd_word_first(dico_stream_t stream, const char *word,
		 const dico_strategy_t strat,
		 const char *begfmt, const char *endmsg,
		 outproc_t proc, const char *tid)
{
    dictd_database_t *db;
    dico_iterator_t itr;

    begin_timing(tid);
    itr = xdico_iterator_create(database_list);
    for (db = dico_iterator_first(itr); db; db = dico_iterator_next(itr)) {
	if (database_visible_p(db)) {
	    struct dico_handler_module *mp = db->instance->module->hmod;
	    dico_result_t res = strat ?
		            mp->module_match(db->mod_handle, strat, word) :
		            mp->module_define(db->mod_handle, word);
	    size_t count;
	    
	    if (!res)
		continue;
	    count = mp->module_result_count(res);
	    
	    if (count) {
		if (strat)
		    current_stat.matches = count;
		else
		    current_stat.defines = count;
		if (mp->module_compare_count)
		    current_stat.compares = mp->module_compare_count(res);
		stream_printf(stream, begfmt, (unsigned long) count);
		proc(db, res, word, stream, count);
		stream_writez(stream, (char*) endmsg);
		report_current_timing(stream, tid);
		dico_stream_write(stream, "\r\n", 2);
		access_log_status(begfmt, endmsg);
	    }
	    
	    mp->module_free_result(res);
	    break;
	}
    }
    dico_iterator_destroy(&itr);
    if (!db) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
    }
}

struct dbres {
    dictd_database_t *db;
    dico_result_t res;
    size_t count;
};

void
dictd_word_all(dico_stream_t stream, const char *word,
	       const dico_strategy_t strat,
	       const char *begfmt, const char *endmsg,
	       outproc_t proc, const char *tid)
{
    dictd_database_t *db;
    dico_iterator_t itr;
    dico_list_t reslist = xdico_list_create();
    size_t total = 0;
    struct dbres *rp;

    begin_timing(tid);

    itr = xdico_iterator_create(database_list);
    for (db = dico_iterator_first(itr); db; db = dico_iterator_next(itr)) {
	if (database_visible_p(db)) {
	    struct dico_handler_module *mp = db->instance->module->hmod;
	    dico_result_t res = strat ?
		              mp->module_match(db->mod_handle, strat, word) :
		              mp->module_define(db->mod_handle, word);
	    size_t count;
	
	    if (!res)
		continue;
	    count = mp->module_result_count(res);
	    if (!count) {
		mp->module_free_result(res);
		continue;
	    }

	    total += count;
	    if (mp->module_compare_count)
		current_stat.compares += mp->module_compare_count(res);
	    rp = xmalloc(sizeof(*rp));
	    rp->db = db;
	    rp->res = res;
	    rp->count = count;
	    xdico_list_append(reslist, rp);
	}
    }

    dico_iterator_destroy(&itr);

    if (total == 0) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
    } else {
	itr = xdico_iterator_create(reslist);
	
	if (strat)
	    current_stat.matches = total;
	else
	    current_stat.defines = total;
	stream_printf(stream, begfmt, (unsigned long) total);
	for (rp = dico_iterator_first(itr); rp; rp = dico_iterator_next(itr)) {
	    proc(rp->db, rp->res, word, stream, rp->count);
	    rp->db->instance->module->hmod->module_free_result(rp->res);
	    free(rp);
	}
	stream_writez(stream, (char*) endmsg);
	report_current_timing(stream, tid);
	dico_stream_write(stream, "\r\n", 2);
	dico_iterator_destroy(&itr);
	access_log_status(begfmt, endmsg);
    }
    dico_list_destroy(&reslist, NULL, NULL);
}

static void
print_matches(dictd_database_t *db, dico_result_t res,
	      const char *word,
	      dico_stream_t stream, size_t count)
{
    size_t i;
    struct dico_handler_module *mp = db->instance->module->hmod;
    dico_stream_t ostr = dictd_ostream_create(stream, db->content_type,
	                                      db->content_transfer_encoding);

    for (i = 0; i < count; i++) {
	stream_writez(ostr, db->name);
	dico_stream_write(ostr, " \"", 2);
	mp->module_output_result(res, i, ostr);
	dico_stream_write(ostr, "\"\r\n", 3);
    }
    dico_stream_close(ostr);
    dico_stream_destroy(&ostr);
}

void
dictd_match_word_db(dictd_database_t *db, dico_stream_t stream,
		    const dico_strategy_t strat, const char *word)
{
    struct dico_handler_module *mp = db->instance->module->hmod;
    dico_result_t res;
    size_t count;
    
    begin_timing("match");
    res = mp->module_match(db->mod_handle, strat, word);
    
    if (!res) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
	return;
    }

    count = mp->module_result_count(res);
    if (count == 0) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
    } else {
	current_stat.matches = count;
	if (mp->module_compare_count)
	    current_stat.compares = mp->module_compare_count(res);
	stream_printf(stream, "152 %lu matches found: list follows\r\n",
		      (unsigned long) count);
	print_matches(db, res, word, stream, count);
	dico_stream_write(stream, ".\r\n", 3);
	stream_writez(stream, "250 Command complete");
	report_current_timing(stream, "match");
	dico_stream_write(stream, "\r\n", 2);
	access_log_status("152", "250");
    }
    
    mp->module_free_result(res);
}

void
dictd_match_word_first(dico_stream_t stream,
		       const dico_strategy_t strat, const char *word)
{
    dictd_word_first(stream, word, strat,
		     "152 %lu matches found: list follows\r\n",
		     ".\r\n250 Command complete",
		     print_matches, "match");
}

void
dictd_match_word_all(dico_stream_t stream,
		     const dico_strategy_t strat, const char *word)
{
    dictd_word_all(stream, word, strat,
		   "152 %lu matches found: list follows\r\n",
		   ".\r\n250 Command complete",
		   print_matches, "match");
}



static void
print_definitions(dictd_database_t *db, dico_result_t res,
		  const char *word,
		  dico_stream_t stream, size_t count)
{
    size_t i;
    char *descr = dictd_get_database_descr(db);
    struct dico_handler_module *mp = db->instance->module->hmod;
    for (i = 0; i < count; i++) {
	dico_stream_t ostr;
	stream_printf(stream, "151 \"%s\" %s \"%s\"\r\n",
		      word, db->name, descr ? descr : "");
	ostr = dictd_ostream_create(stream, db->content_type,
				    db->content_transfer_encoding);
	mp->module_output_result(res, i, ostr);
	dico_stream_close(ostr);
	dico_stream_destroy(&ostr);
	dico_stream_write(stream, "\r\n.\r\n", 5);
    }
    dictd_free_database_descr(db, descr);
}

void
dictd_define_word_db(dictd_database_t *db, dico_stream_t stream,
		     const char *word)
{
    struct dico_handler_module *mp = db->instance->module->hmod;
    dico_result_t res;
    size_t count;
    
    begin_timing("define");

    res = mp->module_define(db->mod_handle, word);
    if (!res) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
	return;
    }

    count = mp->module_result_count(res);
    if (count == 0) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
    } else {
	current_stat.defines = count;
	if (mp->module_compare_count)
	    current_stat.compares = mp->module_compare_count(res);
	stream_printf(stream, "150 %lu definitions found: list follows\r\n",
		      (unsigned long) count);
	print_definitions(db, res, word, stream, count);
	stream_writez(stream, "250 Command complete");
	report_current_timing(stream, "define");
	dico_stream_write(stream, "\r\n", 2);
	access_log_status("150", "250");
    }
    
    mp->module_free_result(res);
}

void
dictd_define_word_first(dico_stream_t stream, const char *word)
{
    dictd_word_first(stream, word, NULL,
		     "150 %lu definitions found: list follows\r\n",
		     "250 Command complete",
		     print_definitions, "define");
}

void
dictd_define_word_all(dico_stream_t stream, const char *word)
{
    dictd_word_all(stream, word, NULL,
		   "150 %lu definitions found: list follows\r\n",
		   "250 Command complete",
		   print_definitions, "define");
}
	    
