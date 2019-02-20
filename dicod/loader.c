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

/* Load path */
static int
_add_load_dir (void *item, void *unused)
{
    char *str = item;
    return lt_dladdsearchdir(str);
}

void
dicod_loader_init(void)
{
    lt_dlinit();
    dico_list_iterate(prepend_load_path, _add_load_dir, NULL);
    lt_dladdsearchdir(DICO_MODDIR);
    dico_list_iterate(module_load_path, _add_load_dir, NULL);
}

#define MODULE_ASSERT(cond)					\
    if (!(cond)) {						\
	dico_log(L_ERR, 0, _("%s: faulty module: (%s) failed"), \
		 argv[0],					\
		 #cond);					\
	return 1;						\
    }

static int
module_init(dicod_module_instance_t *inst, struct dico_database_module *pmod,
	    int argc, char **argv)
{
    MODULE_ASSERT(pmod);
    MODULE_ASSERT(pmod->dico_version <= DICO_MODULE_VERSION);
    if (pmod->dico_capabilities & DICO_CAPA_NODB) {
	MODULE_ASSERT(pmod->dico_init);
    } else {
	if (pmod->dico_capabilities & DICO_CAPA_INIT_EXT) {
	    MODULE_ASSERT(pmod->dico_init_db_ext);
	} else {
	    MODULE_ASSERT(pmod->dico_init_db);
	}
	MODULE_ASSERT(pmod->dico_free_db);
	MODULE_ASSERT(pmod->dico_match);
	MODULE_ASSERT(pmod->dico_define);
	MODULE_ASSERT(pmod->dico_output_result);
	MODULE_ASSERT(pmod->dico_result_count);
	MODULE_ASSERT(pmod->dico_free_result);
    }

    if (pmod->dico_init && pmod->dico_init(argc, argv)) {
	dico_log(L_ERR, 0, _("%s: initialization failed"), argv[0]);
	return 1;
    }

    inst->module = pmod;
    return 0;
}

static int
module_load(dicod_module_instance_t *inst, int argc, char **argv)
{
    lt_dlhandle handle = NULL;
    lt_dladvise advise = NULL;
    struct dico_database_module *pmod;

    if (inst->handle) {
	dico_log(L_ERR, 0, _("module %s already loaded"), argv[0]);
	return 1;
    }
    if (inst->module)
	/* FIXME: Built-in module */
	return 0;

    if (!lt_dladvise_init(&advise) && !lt_dladvise_ext(&advise)
	&& !lt_dladvise_global(&advise))
	handle = lt_dlopenadvise(argv[0], advise);
    lt_dladvise_destroy(&advise);

    if (!handle) {
	dico_log(L_ERR, 0, _("cannot load module %s: %s"), argv[0],
		 lt_dlerror());
	return 1;
    }

    pmod = (struct dico_database_module *) lt_dlsym(handle, "module");

    if (module_init(inst, pmod, argc, argv)) {
	lt_dlclose(handle);
	return 1;
    }

    inst->handle = handle;

    return 0;
}

int
dicod_load_module(dicod_module_instance_t *inst)
{
    struct wordsplit ws;
    int rc;

    if (wordsplit(inst->command, &ws, WRDSF_DEFFLAGS)) {
	dico_log(L_ERR, 0, _("cannot parse command line `%s': %s"),
		 inst->command, wordsplit_strerror (&ws));
	return 1;
    }
    rc = module_load(inst, ws.ws_wordc, ws.ws_wordv);
    wordsplit_free(&ws);
    return rc;
}

static void
builtin_module_load(char const *ident, struct dico_database_module *pmod)
{
    dicod_module_instance_t *inst = xzalloc(sizeof(*inst));
    char *argv[2];
    inst->ident = xstrdup(ident);
    inst->command = xstrdup(ident);
    argv[0] = inst->command;
    argv[1] = NULL;
    if (module_init(inst, pmod, 1, argv)) {
	dico_log(L_CRIT, 0,
		 "INTERNAL ERROR: failed to load built-in module %s",
		 ident);
	dico_log(L_CRIT, 0, "Please, report");
	abort();
    }
    inst->handle = NULL;
    xdico_list_append(modinst_list, inst);
}

void
dicod_builtin_module_init(void)
{
    builtin_module_load("virtual", &virtual_builtin_module);
}

static int
_dup_lang_item(void *item, void *data)
{
    char *lang = item;
    dico_list_t dst = data;
    xdico_list_append(dst, xstrdup(lang));
    return 0;
}

dico_list_t
dicod_langlist_copy(dico_list_t src)
{
    dico_list_t dst = xdico_list_create();
    dico_list_set_free_item(dst, dicod_free_item, NULL);
    dico_list_iterate(src, _dup_lang_item, dst);
    return dst;
}

int
dicod_any_lang_list_p(dico_list_t list)
{
    return list == NULL
	   || dico_list_count(list) ==  0
	   || (dico_list_count(list) == 1
	       && strcmp (dico_list_item(list, 0), "*") == 0);
}

static char nomatch[] = "552 No match";
static size_t nomatch_len = (sizeof(nomatch)-1);


typedef void (*outproc_t)(dicod_db_result_t *res,
			  const char *word, dico_stream_t stream,
			  void *data,
			  size_t count);

void
dicod_word_first(dico_stream_t stream, const char *word,
		 const dico_strategy_t strat,
		 const char *begfmt, const char *endmsg,
		 outproc_t proc, void *data, const char *tid)
{
    dicod_database_t *db;
    dico_iterator_t itr;

    begin_timing(tid);

    if (strat && stratcl_check_word(strat->stratcl, word)) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
	return;
    }

    itr = xdico_list_iterator(database_list);
    for (db = dico_iterator_first(itr); db; db = dico_iterator_next(itr)) {
	if (database_is_visible(db) && !database_is_virtual(db)) {
	    dicod_db_result_t *res = strat
		? dicod_database_match(db, strat, word)
		: dicod_database_define(db, word);
	    size_t count;

	    if (!res)
		continue;
	    count = dicod_db_result_count(res);

	    if (count) {
		if (strat)
		    current_stat.matches = count;
		else
		    current_stat.defines = count;
		current_stat.compares = dicod_db_result_compare_count(res);
		stream_printf(stream, begfmt, (unsigned long) count);
		proc(res, word, stream, data, count);
		stream_writez(stream, (char*) endmsg);
		report_current_timing(stream, tid);
		dico_stream_write(stream, "\n", 1);
		access_log_status(begfmt, endmsg);
		dicod_db_result_free(res);
		break;
	    } else
		dicod_db_result_free(res);
	}
    }
    dico_iterator_destroy(&itr);
    if (!db) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
    }
}

void
dicod_word_all(dico_stream_t stream, const char *word,
	       const dico_strategy_t strat,
	       const char *begfmt, const char *endmsg,
	       outproc_t proc, void *data, const char *tid)
{
    dicod_database_t *db;
    dico_iterator_t itr;
    dico_list_t reslist = xdico_list_create();
    size_t total = 0;
    dicod_db_result_t *rp;

    begin_timing(tid);

    if (strat && stratcl_check_word(strat->stratcl, word)) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
	return;
    }

    itr = xdico_list_iterator(database_list);
    for (db = dico_iterator_first(itr); db; db = dico_iterator_next(itr)) {
	if (database_is_visible(db) && !database_is_virtual(db)) {
	    dicod_db_result_t *res = strat
		? dicod_database_match(db, strat, word)
		: dicod_database_define(db, word);
	    size_t count;

	    if (!res)
		continue;
	    count = dicod_db_result_count(res);
	    if (!count) {
		dicod_db_result_free(res);
		continue;
	    }

	    total += count;
	    current_stat.compares += dicod_db_result_compare_count(res);
	    xdico_list_append(reslist, res);
	}
    }

    dico_iterator_destroy(&itr);

    if (total == 0) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
    } else {
	itr = xdico_list_iterator(reslist);

	if (strat)
	    current_stat.matches = total;
	else
	    current_stat.defines = total;
	stream_printf(stream, begfmt, (unsigned long) total);
	for (rp = dico_iterator_first(itr); rp; rp = dico_iterator_next(itr)) {
	    proc(rp, word, stream, data, dicod_db_result_count(rp));
	    dicod_db_result_free(rp);
	}
	stream_writez(stream, (char*) endmsg);
	report_current_timing(stream, tid);
	dico_stream_write(stream, "\n", 1);
	dico_iterator_destroy(&itr);
	access_log_status(begfmt, endmsg);
    }
    dico_list_destroy(&reslist);
}

static void
print_matches(dicod_db_result_t *res,
	      const char *word,
	      dico_stream_t stream, void *data, size_t count)
{
    size_t i;
    dico_stream_t ostr = data;

    for (i = 0; i < count; i++) {
	dicod_database_t *db = dicod_db_result_db(res, i, result_db_visible);
	stream_writez(ostr, db->name);
	dico_stream_write(ostr, " \"", 2);
	dicod_db_result_output(res, i, ostr);
	dico_stream_write(ostr, "\"\n", 2);
    }
}

void
dicod_match_word_db(dicod_database_t *db, dico_stream_t stream,
		    const dico_strategy_t strat, const char *word)
{
    dicod_db_result_t *res;
    size_t count;

    begin_timing("match");

    res = dicod_database_match(db, strat, word);

    if (!res) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
	return;
    }

    count = dicod_db_result_count(res);
    if (count == 0) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
    } else {
	dico_stream_t ostr;

	current_stat.matches = count;
	current_stat.compares = dicod_db_result_compare_count(res);
	stream_printf(stream, "152 %lu matches found: list follows\n",
		      (unsigned long) count);
	ostr = dicod_ostream_create(stream, NULL);
	print_matches(res, word, stream, ostr, count);
	total_bytes_out += dico_stream_bytes_out(ostr);
	dico_stream_close(ostr);
	dico_stream_destroy(&ostr);
	dico_stream_write(stream, ".\n", 2);
	stream_writez(stream, "250 Command complete");
	report_current_timing(stream, "match");
	dico_stream_write(stream, "\n", 1);
	access_log_status("152", "250");
    }

    dicod_db_result_free(res);
}

void
dicod_match_word_first(dico_stream_t stream,
		       const dico_strategy_t strat, const char *word)
{
    dico_stream_t ostr = dicod_ostream_create(stream, NULL);
    dicod_word_first(stream, word, strat,
		     "152 %lu matches found: list follows\n",
		     ".\n250 Command complete",
		     print_matches, ostr, "match");
    total_bytes_out += dico_stream_bytes_out(ostr);
    dico_stream_close(ostr);
    dico_stream_destroy(&ostr);
}

void
dicod_match_word_all(dico_stream_t stream,
		     const dico_strategy_t strat, const char *word)
{
    dico_stream_t ostr = dicod_ostream_create(stream, NULL);
    dicod_word_all(stream, word, strat,
		   "152 %lu matches found: list follows\n",
		   ".\n250 Command complete",
		   print_matches, ostr, "match");
    total_bytes_out += dico_stream_bytes_out(ostr);
    dico_stream_close(ostr);
    dico_stream_destroy(&ostr);
}

//FIXME
void
dicod_database_print_definitions(char const *word,
				 dicod_db_result_t *res, size_t count,
				 dico_stream_t stream)
{
    size_t i;

    for (i = 0; i < count; i++) {
	dico_stream_t ostr;
	dico_assoc_list_t hdr;
	dicod_database_t *db = dicod_db_result_db(res, i, result_db_visible);
	char *descr = dicod_database_get_descr(db);
	stream_printf(stream, "151 \"%s\" %s \"%s\"\n",
		      word, db->name, descr ? descr : "");
	dicod_database_free_descr(db, descr);
	
	hdr = dicod_db_result_mime_header(res, i);
	ostr = dicod_ostream_create(stream, hdr);
	dicod_db_result_output(res, i, ostr);
	total_bytes_out += dico_stream_bytes_out(ostr);
	dico_stream_close(ostr);
	dico_stream_destroy(&ostr);
	dico_stream_write(stream, "\n.\n", 3);
	dico_assoc_destroy(&hdr);
    }
}

static void
print_definitions(dicod_db_result_t *res,
		  const char *word,
		  dico_stream_t stream, void *data, size_t count)
{
    dicod_database_print_definitions(word, res, count, stream);
}

void
dicod_define_word_db(dicod_database_t *db, dico_stream_t stream,
		     const char *word)
{
    dicod_db_result_t *res;
    size_t count;

    begin_timing("define");

    res = dicod_database_define(db, word);
    if (!res) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
	return;
    }

    count = dicod_db_result_count(res);
    if (count == 0) {
	access_log_status(nomatch, nomatch);
	dico_stream_writeln(stream, nomatch, nomatch_len);
    } else {
	current_stat.defines = count;
	current_stat.compares = dicod_db_result_compare_count(res);
	stream_printf(stream, "150 %lu definitions found: list follows\n",
		      (unsigned long) count);
	print_definitions(res, word, stream, NULL, count);
	stream_writez(stream, "250 Command complete");
	report_current_timing(stream, "define");
	dico_stream_write(stream, "\n", 1);
	access_log_status("150", "250");
    }

    dicod_db_result_free(res);
}

void
dicod_define_word_first(dico_stream_t stream, const char *word)
{
    dicod_word_first(stream, word, NULL,
		     "150 %lu definitions found: list follows\n",
		     "250 Command complete",
		     print_definitions, NULL, "define");
}

void
dicod_define_word_all(dico_stream_t stream, const char *word)
{
    dicod_word_all(stream, word, NULL,
		   "150 %lu definitions found: list follows\n",
		   "250 Command complete",
		   print_definitions, NULL, "define");
}

int
dicod_module_test(int argc, char **argv)
{
    int i;
    dicod_module_instance_t inst;
    struct dico_database_module *pmod;

    /* Split arguments in two parts: unit test arguments and
       optional module initialization arguments */
    for (i = 0; i < argc; i++)
	if (strcmp(argv[i], "--") == 0) {
	    break;
	}

    if (i == 0)
	dico_die(EX_UNAVAILABLE, L_ERR, 0, _("no module name"));

    memset(&inst, 0, sizeof(inst));
    if (i < argc) {
	argv[i] = argv[0];
	if (module_load(&inst, argc - i, argv + i))
	    return 1;
	argv[i] = NULL;
    } else {
	char *null_argv[2];
	null_argv[0] = argv[0];
	null_argv[1] = NULL;
	if (module_load(&inst, 1, null_argv))
	    return 1;
    }

    pmod = inst.module;
    if (pmod->dico_version <= 1 || !pmod->dico_run_test)
	dico_die(EX_UNAVAILABLE, L_ERR, 0,
		 _("module does not define dico_run_test function"));
    return pmod->dico_run_test(i, argv);
}
