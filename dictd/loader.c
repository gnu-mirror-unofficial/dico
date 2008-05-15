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

static int
dictd_load_module0(dictd_handler_t *hptr, int argc, char **argv)
{
    lt_dlhandle handle;
    struct dico_handler_module *pmod;    
	
    handle = lt_dlopenext(argv[0]);
    if (!handle) {
	dico_log(L_ERR, 0, _("cannot load module %s: %s"), argv[0],
	       lt_dlerror());
	return 1;
    }

    pmod = (struct dico_handler_module *) lt_dlsym(handle, "module");
    if (!pmod) {
	lt_dlclose(handle);
	dico_log(L_ERR, 0, _("%s: faulty module"), argv[0]);
	return 1;
    }

    if (!pmod->module_match
	|| !pmod->module_match_all
	|| !pmod->module_define
	|| !pmod->module_output_result
	|| !pmod->module_result_count
	|| !pmod->module_free_result) {
	lt_dlclose(handle);
	dico_log(L_ERR, 0, _("%s: faulty module"), argv[0]);
	return 1;
    }
    
    if (pmod->module_init && pmod->module_init(argc, argv)) {
	lt_dlclose(handle);
	dico_log(L_ERR, 0, _("%s: initialization failed"), argv[0]);
	return 1;
    }

    hptr->handle = handle;
    hptr->module = pmod;
    return 0;
}

int
dictd_load_module(dictd_handler_t *hptr)
{
    int argc;
    char **argv;
    int rc;
	
    if ((rc = dico_argcv_get(hptr->command, NULL, NULL, &argc, &argv))) {
	dico_log(L_ERR, rc, _("cannot parse command line `%s'"), hptr->command);
	return 1;
    }

    rc = dictd_load_module0(hptr, argc, argv);

    dico_argcv_free(argc, argv);
    
    return rc;
}

int
dictd_open_database_handler(dictd_database_t *dp)
{
    dictd_handler_t *hptr = dp->handler;

    if (hptr->module->module_open) {
	dp->mod = hptr->module->module_open(dp->name, dp->argc, dp->argv);
	if (!dp->mod) {
	    dico_log(L_ERR, 0, _("cannot open module `%s'"), dp->command);
	    return 1;
	}
    }
    return 0;
}

int
dictd_close_database_handler(dictd_database_t *dp)
{
    int rc;
    
    if (dp->mod) {
	dictd_handler_t *hptr = dp->handler;
	if (hptr->module->module_close) 
	    rc = hptr->module->module_close(dp->mod);
	dp->mod = NULL;
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
	dictd_handler_t *hptr = db->handler;
	if (hptr->module->module_db_descr)
	    return hptr->module->module_db_descr(db->mod);
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
	dictd_handler_t *hptr = db->handler;
	if (hptr->module->module_db_info)
	    return hptr->module->module_db_info(db->mod);
    }
    return NULL;
}

void
dictd_free_database_info(dictd_database_t *db, char *info)
{
    if (info && info != db->info)
	free(info);
}


void
print_timing(dico_stream_t stream, const char *name)
{
    if (timing_option) {
	xdico_timer_t t = timer_stop(name);
	dico_stream_write(stream, " [", 2);
	timer_format_time(stream, timer_get_real(t));
	dico_stream_write(stream, "r ", 2);
	timer_format_time(stream, timer_get_user(t));
	dico_stream_write(stream, "u ", 2);
	timer_format_time(stream, timer_get_system(t));
	dico_stream_write(stream, "s]", 2);
    }
}



static char nomatch[] = "552 No match";
static size_t nomatch_len = (sizeof(nomatch)-1);


typedef void (*outproc_t)(dictd_database_t *db, dico_result_t res,
			  const char *word, dico_stream_t stream,
			  size_t count);

#define MATCH(db, strat, word)                                               \
    (strat->sel ?                                                            \
         mp->module_match_all(db->mod, word, strat->sel, strat->closure) :   \
         mp->module_match(db->mod, strat->name, word)) 

void
dictd_word_first(dico_stream_t stream, const char *word,
		 const dico_strategy_t *strat,
		 const char *begfmt, const char *endmsg,
		 outproc_t proc, const char *tid)
{
    dictd_database_t *db;
    dico_iterator_t itr;

    if (timing_option)
	timer_start(tid);
    itr = xdico_iterator_create(database_list);
    for (db = dico_iterator_first(itr); db; db = dico_iterator_next(itr)) {
	if (database_visible_p(db)) {
	    struct dico_handler_module *mp = db->handler->module;
	    dico_result_t res = strat ? MATCH(db, strat, word) :
 	 	                        mp->module_define(db->mod, word);
	    size_t count;
	    
	    if (!res)
		continue;
	    count = mp->module_result_count(res);
	    
	    if (count) {
		stream_printf(stream, begfmt, (unsigned long) count);
		proc(db, res, word, stream, count);
		stream_writez(stream, (char*) endmsg);
		print_timing(stream, tid);
		dico_stream_write(stream, "\r\n", 2);
	    }
	    
	    mp->module_free_result(res);
	    break;
	}
    }
    dico_iterator_destroy(&itr);
    if (!db)
	dico_stream_writeln(stream, nomatch, nomatch_len);
}

struct dbres {
    dictd_database_t *db;
    dico_result_t res;
    size_t count;
};

void
dictd_word_all(dico_stream_t stream, const char *word,
	       const dico_strategy_t *strat,
	       const char *begfmt, const char *endmsg,
	       outproc_t proc, const char *tid)
{
    dictd_database_t *db;
    dico_iterator_t itr;
    dico_list_t reslist = xdico_list_create();
    size_t total = 0;
    struct dbres *rp;

    if (timing_option)
	timer_start(tid);

    itr = xdico_iterator_create(database_list);
    for (db = dico_iterator_first(itr); db; db = dico_iterator_next(itr)) {
	if (database_visible_p(db)) {
	    struct dico_handler_module *mp = db->handler->module;
	    dico_result_t res = strat ? MATCH(db, strat, word) :
		                        mp->module_define(db->mod, word);
	    size_t count;
	
	    if (!res)
		continue;
	    count = mp->module_result_count(res);
	    if (!count) {
		mp->module_free_result(res);
		continue;
	    }

	    rp = xmalloc(sizeof(*rp));
	    total += count;
	    rp->db = db;
	    rp->res = res;
	    rp->count = count;
	    xdico_list_append(reslist, rp);
	}
    }

    dico_iterator_destroy(&itr);

    if (total == 0)
	dico_stream_writeln(stream, nomatch, nomatch_len);
    else {
	itr = xdico_iterator_create(reslist);
	
	stream_printf(stream, begfmt, (unsigned long) total);
	for (rp = dico_iterator_first(itr); rp; rp = dico_iterator_next(itr)) {
	    proc(rp->db, rp->res, word, stream, rp->count);
	    rp->db->handler->module->module_free_result(rp->res);
	    free(rp);
	}
	stream_writez(stream, (char*) endmsg);
	print_timing(stream, tid);
	dico_stream_write(stream, "\r\n", 2);
	dico_iterator_destroy(&itr);
    }
    dico_list_destroy(&reslist, NULL, NULL);
}

static void
print_matches(dictd_database_t *db, dico_result_t res,
	      const char *word,
	      dico_stream_t stream, size_t count)
{
    size_t i;
    struct dico_handler_module *mp = db->handler->module;
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
		    const dico_strategy_t *strat, const char *word)
{
    struct dico_handler_module *mp = db->handler->module;
    dico_result_t res;
    size_t count;
    
    if (timing_option)
	timer_start("match");
    res = MATCH(db, strat, word);
    
    if (!res) {
	dico_stream_writeln(stream, nomatch, nomatch_len);
	return;
    }

    count = mp->module_result_count(res);

    if (count == 0) 
	dico_stream_writeln(stream, nomatch, nomatch_len);
    else {
	stream_printf(stream, "152 %lu matches found: list follows\r\n",
		      (unsigned long) count);
	print_matches(db, res, word, stream, count);
	dico_stream_write(stream, ".\r\n", 3);
	stream_writez(stream, "250 Command complete");
	print_timing(stream, "match");
	dico_stream_write(stream, "\r\n", 2);
    }
    
    mp->module_free_result(res);
}

void
dictd_match_word_first(dico_stream_t stream,
		       const dico_strategy_t *strat, const char *word)
{
    dictd_word_first(stream, word, strat,
		     "152 %lu matches found: list follows\r\n",
		     ".\r\n250 Command complete",
		     print_matches, "match");
}

void
dictd_match_word_all(dico_stream_t stream,
		     const dico_strategy_t *strat, const char *word)
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
    struct dico_handler_module *mp = db->handler->module;
    for (i = 0; i < count; i++) {
	dico_stream_t ostr;
	stream_printf(stream, "151 \"%s\" %s \"%s\"\r\n",
		      word, db->name, descr);
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
    struct dico_handler_module *mp = db->handler->module;
    dico_result_t res;
    size_t count;
    
    if (timing_option)
	timer_start("define");

    res = mp->module_define(db->mod, word);
    if (!res) {
	dico_stream_writeln(stream, nomatch, nomatch_len);
	return;
    }

    count = mp->module_result_count(res);

    if (count == 0) 
	dico_stream_writeln(stream, nomatch, nomatch_len);
    else {
	stream_printf(stream, "150 %lu definitions found: list follows\r\n",
		      (unsigned long) count);
	print_definitions(db, res, word, stream, count);
	stream_writez(stream, "250 Command complete");
	print_timing(stream, "define");
	dico_stream_write(stream, "\r\n", 2);
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
	    
