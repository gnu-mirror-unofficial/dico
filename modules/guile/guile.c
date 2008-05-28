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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <dico.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <libguile.h>


/* General-purpose eval handlers */

static SCM
eval_catch_body(void *list)
{
    return scm_primitive_eval((SCM)list);
}

static SCM
eval_catch_handler (void *data, SCM tag, SCM throw_args)
{
    scm_handle_by_message_noexit("dico", tag, throw_args);
    longjmp(*(jmp_buf*)data, 1);
}

struct scheme_exec_data {
    SCM (*handler) (void *data);
    void *data;
    SCM result;
};

static SCM
scheme_safe_exec_body (void *data)
{
    struct scheme_exec_data *ed = data;
    ed->result = ed->handler (ed->data);
    return SCM_BOOL_F;
}

static int
guile_safe_exec(SCM (*handler) (void *data), void *data, SCM *result)
{
    jmp_buf jmp_env;
    struct scheme_exec_data ed;
	
    if (setjmp(jmp_env))
	return 1;
    ed.handler = handler;
    ed.data = data;
    scm_internal_lazy_catch(SCM_BOOL_T,
			    scheme_safe_exec_body, (void*)&ed,
			    eval_catch_handler, &jmp_env);
    if (result)
	*result = ed.result;
    return 0;
}

static SCM
load_path_handler(void *data)
{
    scm_primitive_load_path((SCM)data);
    return SCM_UNDEFINED;
}

static int
guile_load(char *filename)
{
    return guile_safe_exec(load_path_handler, scm_makfrom0str(filename), NULL);
}

static void
_add_load_path(char *path)
{
    SCM scm, path_scm;
    SCM *pscm;

    path_scm = SCM_VARIABLE_REF(scm_c_lookup("%load-path"));
    for (scm = path_scm; scm != SCM_EOL; scm = SCM_CDR(scm)) {
	SCM val = SCM_CAR(scm);
	if (scm_is_string(val))
	    if (strcmp(scm_i_string_chars(val), path) == 0)
		return;
    }

    pscm = SCM_VARIABLE_LOC(scm_c_lookup("%load-path"));
    *pscm = scm_append(scm_list_3(path_scm,
				  scm_list_1(scm_makfrom0str(path)),
				  SCM_EOL));
}

static SCM
close_port_handler(void *port)
{
	scm_close_port((SCM)port);
	return SCM_UNDEFINED;
}

static void
silent_close_port(SCM port)
{
	guile_safe_exec(close_port_handler, port, NULL);
}

static void
memerr(const char *fname)
{
    dico_log(L_ERR, 0, _("%s: not enough memory"), fname);
}

static void
guile_redirect_output(char *filename)
{
    SCM port;
    char *mode = "a";
    int fd = 2;

    if (filename) {
	fd = open(filename, O_RDWR|O_CREAT|O_APPEND, 0600);
	if (fd == -1) {
	    dico_log(L_ERR, errno, 
		     _("cannot open file `%s'"), filename);
	    fd = 2;
	}
    }
    port = scm_fdes_to_port(fd, mode, scm_makfrom0str("<standard error>"));
    silent_close_port(scm_current_output_port());
    silent_close_port(scm_current_error_port());
    scm_set_current_output_port(port);
    scm_set_current_error_port(port);
}

struct guile_proc {
    char *name;
    SCM symbol;
};

int
guile_call_proc(SCM *result, struct guile_proc *proc, SCM arglist)
{
    jmp_buf jmp_env;
    SCM cell;

    if (setjmp(jmp_env)) {
	dico_log(L_NOTICE, 0,
		 _("procedure `%s' failed: see error output for details"),
		 proc->name);
	return 1;
    }
    cell = scm_cons(proc->symbol, arglist);
    *result = scm_internal_lazy_catch(SCM_BOOL_T,
				      eval_catch_body, cell,
				      eval_catch_handler, &jmp_env);
    return 0;
}

static void
rettype_error(const char *name)
{
    dico_log(L_ERR, 0, _("%s: invalid return type"), name);
}


long _guile_strategy_tag;

struct _guile_strategy
{
    const dico_strategy_t strat;
};

static SCM
_make_strategy(const dico_strategy_t strat)
{
    struct _guile_strategy *sp;

    sp = scm_gc_malloc (sizeof (struct _guile_strategy), "strategy");
    sp->strat = strat;
    SCM_RETURN_NEWSMOB(_guile_strategy_tag, sp);
}

static scm_sizet
_guile_strategy_free(SCM message_smob)
{
  struct _guile_strategy *sp =
      (struct _guile_strategy *) SCM_CDR (message_smob);
  free(sp);
  return sizeof (struct _guile_strategy);
}

static int
_guile_strategy_print(SCM message_smob, SCM port, scm_print_state * pstate)
{
    struct _guile_strategy *sp =
	(struct _guile_strategy *) SCM_CDR (message_smob);
    scm_puts("#<strategy ", port);
    scm_puts(sp->strat->name, port);
    scm_puts(" [", port);
    scm_puts(sp->strat->descr, port);
    scm_puts("]>", port);
    return 1;
}

SCM
_guile_strategy_create(SCM owner, const dico_strategy_t strat)
{
    struct _guile_strategy *sp;

    sp = scm_gc_malloc(sizeof(struct _guile_strategy), "strategy");
    sp->strat = strat;
    SCM_RETURN_NEWSMOB(_guile_strategy_tag, sp);
}

static void
_guile_init_strategy()
{
    _guile_strategy_tag = scm_make_smob_type("strategy",
					     sizeof (struct _guile_strategy));
    scm_set_smob_free(_guile_strategy_tag, _guile_strategy_free);
    scm_set_smob_print(_guile_strategy_tag, _guile_strategy_print);
}

#define CELL_IS_STRAT(s) \
    (!SCM_IMP(s) && SCM_CELL_TYPE(s) == _guile_strategy_tag)

SCM_DEFINE(scm_dico_strat_selector_p, "dico-strat-selector?", 1, 0, 0,
	   (SCM STRAT),
	   "Return true if STRAT has a selector.")
#define FUNC_NAME s_scm_dico_strat_selector_p
{
    struct _guile_strategy *sp;
    
    SCM_ASSERT(CELL_IS_STRAT(STRAT), STRAT, SCM_ARG1, FUNC_NAME);
    sp = (struct _guile_strategy *) SCM_CDR(STRAT);
    return sp->strat->sel ? SCM_BOOL_T : SCM_BOOL_F;
}
#undef FUNC_NAME

SCM_DEFINE(scm_dico_strat_select_p, "dico-strat-select?", 3, 0, 0,
	   (SCM STRAT, SCM WORD, SCM KEY),
	   "Return true if KEY matches WORD as per strategy selector STRAT.")
#define FUNC_NAME s_scm_dico_strat_select_p
{
    struct _guile_strategy *sp;
    
    SCM_ASSERT(CELL_IS_STRAT(STRAT), STRAT, SCM_ARG1, FUNC_NAME);
    SCM_ASSERT(scm_is_string(WORD), WORD, SCM_ARG2, FUNC_NAME);
    SCM_ASSERT(scm_is_string(KEY), KEY, SCM_ARG3, FUNC_NAME);
    sp = (struct _guile_strategy *) SCM_CDR(STRAT);
    return sp->strat->sel(DICO_SELECT_RUN,
			  scm_i_string_chars(WORD),
			  scm_i_string_chars(KEY),
			  sp->strat->closure) ? SCM_BOOL_T : SCM_BOOL_F;
}
#undef FUNC_NAME

SCM_DEFINE(scm_dico_strat_name, "dico-strat-name", 1, 0, 0,
	   (SCM STRAT),
	   "Return name of the strategy STRAT.")
#define FUNC_NAME s_scm_dico_strat_name
{
    struct _guile_strategy *sp;
    
    SCM_ASSERT(CELL_IS_STRAT(STRAT), STRAT, SCM_ARG1, FUNC_NAME);
    sp = (struct _guile_strategy *) SCM_CDR(STRAT);
    return scm_makfrom0str(sp->strat->name);
}
#undef FUNC_NAME

SCM_DEFINE(scm_dico_strat_description, "dico-strat-descriprion", 1, 0, 0,
	   (SCM STRAT),
	   "Return a textual description of the strategy STRAT.")
#define FUNC_NAME s_scm_dico_strat_description
{
    struct _guile_strategy *sp;
    
    SCM_ASSERT(CELL_IS_STRAT(STRAT), STRAT, SCM_ARG1, FUNC_NAME);
    sp = (struct _guile_strategy *) SCM_CDR(STRAT);
    return scm_makfrom0str(sp->strat->descr);
}
#undef FUNC_NAME


static long scm_tc16_dico_port;
struct _guile_dico_port {
    dico_stream_t str;
};

static SCM
_make_dico_port(dico_stream_t str)
{
    struct _guile_dico_port *dp;
    SCM port;
    scm_port *pt;

    dp = scm_gc_malloc (sizeof (struct _guile_dico_port), "dico-port");
    dp->str = str;
    port = scm_cell(scm_tc16_dico_port, 0);
    pt = scm_add_to_port_table(port);
    SCM_SETPTAB_ENTRY(port, pt);
    pt->rw_random = 0;
    SCM_SET_CELL_TYPE(port,
		      (scm_tc16_dico_port | SCM_OPN | SCM_WRTNG | SCM_BUF0));
    SCM_SETSTREAM(port, dp);
    return port;
}

#define DICO_PORT(x) ((struct _guile_dico_port *) SCM_STREAM (x))

static SCM
_dico_port_mark(SCM port)
{
    return SCM_BOOL_F;
}

static void
_dico_port_flush(SCM port)
{
    struct _guile_dico_port *dp = DICO_PORT(port);
    if (dp && dp->str)
	dico_stream_flush(dp->str);
}

static int
_dico_port_close(SCM port)
{
    struct _guile_dico_port *dp = DICO_PORT(port);

    if (dp) {
	_dico_port_flush(port);
	SCM_SETSTREAM(port, NULL);
	free(dp);
    }
    return 0;
}

static scm_sizet
_dico_port_free(SCM port)
{
    _dico_port_close(port);
    return sizeof(struct _guile_dico_port);
}

static int
_dico_port_fill_input(SCM port)
{
    return EOF;
}

static void
_dico_port_write(SCM port, const void *data, size_t size)
{
    struct _guile_dico_port *dp = DICO_PORT(port);
    dico_stream_write(dp->str, data, size);
}

static off_t
_dico_port_seek (SCM port, off_t offset, int whence)
{
    struct _guile_dico_port *dp = DICO_PORT(port);
    return dico_stream_seek(dp->str, offset, whence);
}

static int
_dico_port_print(SCM exp, SCM port, scm_print_state *pstate)
{
    scm_puts ("#<Dico port>", port);
    return 1;
}

static void
_guile_init_dico_port()
{
    scm_tc16_dico_port = scm_make_port_type("dico-port",
					    _dico_port_fill_input,
					    _dico_port_write);
    scm_set_port_mark (scm_tc16_dico_port, _dico_port_mark);
    scm_set_port_free (scm_tc16_dico_port, _dico_port_free);
    scm_set_port_print (scm_tc16_dico_port, _dico_port_print);
    scm_set_port_flush (scm_tc16_dico_port, _dico_port_flush);
    scm_set_port_close (scm_tc16_dico_port, _dico_port_close);
    scm_set_port_seek (scm_tc16_dico_port, _dico_port_seek);
}    


static void
_guile_init_funcs (void)
{
    scm_c_define_gsubr(s_scm_dico_strat_selector_p, 1, 0, 0,
		       scm_dico_strat_selector_p);
    scm_c_define_gsubr(s_scm_dico_strat_select_p, 3, 0, 0,
		       scm_dico_strat_select_p);
    scm_c_define_gsubr(s_scm_dico_strat_name, 1, 0, 0,
		       scm_dico_strat_name);
    scm_c_define_gsubr(s_scm_dico_strat_description, 1, 0, 0,
		       scm_dico_strat_description);
}


static int guile_debug;

static char *guile_init_script;
static char *guile_exit_script;
static char *guile_outfile;

enum guile_proc_ind {
    open_proc,
    close_proc,
    info_proc,
    descr_proc,
    match_proc,
    define_proc,
    output_proc,
    result_count_proc,
    compare_count_proc,
    free_result_proc
};

#define MAX_PROC (free_result_proc+1)

static char *guile_proc_name[] = {
    "open",
    "close",
    "info",
    "descr",
    "match",
    "define",
    "output",
    "result_count",
    "compare_count",
    "free_result"
};

struct guile_proc guile_proc[MAX_PROC];

struct _guile_database {
    const char *dbname;
    SCM handle;
};

static int
set_load_path(struct dico_option *opt, const char *val)
{
    char *p;
    char *tmp = strdup(val);
    if (!tmp)
	return 1;
    for (p = strtok(tmp, ":"); p; p = strtok(NULL, ":")) 
	_add_load_path(p);
    free(tmp);
    return 0;
}

struct dico_option init_option[] = {
    { DICO_OPTSTR(debug), dico_opt_bool, &guile_debug },
    { DICO_OPTSTR(init-script), dico_opt_string, &guile_init_script },
    { DICO_OPTSTR(exit-script), dico_opt_string, &guile_exit_script },
    { DICO_OPTSTR(load-path), dico_opt_null, NULL, { 0 }, set_load_path },
    { DICO_OPTSTR(outfile), dico_opt_string, &guile_outfile },
    { DICO_OPTSTR(open), dico_opt_string, &guile_proc[open_proc].name },
    { DICO_OPTSTR(close), dico_opt_string, &guile_proc[close_proc].name },
    { DICO_OPTSTR(info), dico_opt_string, &guile_proc[info_proc].name },
    { DICO_OPTSTR(descr), dico_opt_string, &guile_proc[descr_proc].name },
    { DICO_OPTSTR(match), dico_opt_string, &guile_proc[match_proc].name },
    { DICO_OPTSTR(define), dico_opt_string, &guile_proc[define_proc].name },
    { DICO_OPTSTR(output), dico_opt_string, &guile_proc[output_proc].name },
    { DICO_OPTSTR(result-count), dico_opt_string,
      &guile_proc[result_count_proc].name },
    { DICO_OPTSTR(compare-count), dico_opt_string,
      &guile_proc[compare_count_proc].name },
    { DICO_OPTSTR(free-result), dico_opt_string,
      &guile_proc[free_result_proc].name },
    { NULL }
};

int
mod_init(int argc, char **argv)
{
    int i;
    
    if (dico_parseopt(init_option, argc, argv))
	return 1;

    scm_init_guile();
    scm_load_goops();

    _guile_init_strategy();
    _guile_init_dico_port();
    _guile_init_funcs();
    if (guile_debug) {
	SCM_DEVAL_P = 1;
	SCM_BACKTRACE_P = 1;
	SCM_RECORD_POSITIONS_P = 1;
	SCM_RESET_DEBUG_MODE;
    }

    if (guile_outfile)
	guile_redirect_output(guile_outfile);
    
    if (guile_init_script && guile_load(guile_init_script)) {
	dico_log(L_ERR, 0, _("mod_init: cannot load init script %s"), 
		 guile_init_script);
    }

    for (i = 0; i < MAX_PROC; i++) {
	if (!guile_proc[i].name) {
	    switch (i) {
	    case open_proc:
	    case match_proc:
	    case define_proc:
	    case output_proc:
	    case result_count_proc:
		dico_log(L_ERR, 0,
			 _("%s: faulty guile module - missing `%s'"),
			 argv[0], guile_proc_name[i]);
		return 1;
	    default:
		break;
	    }
	} else {
	    SCM procsym = SCM_VARIABLE_REF(scm_c_lookup(guile_proc[i].name));
	    if (scm_procedure_p(procsym) != SCM_BOOL_T) {
		dico_log(L_ERR, 0,
			 _("%s is not a procedure object"),
			 guile_proc[i].name);
		return 1;
	    }
	    guile_proc[i].symbol = procsym;
	}
    }

    return 0;
}
    
int
mod_close(dico_handle_t hp)
{
    struct _guile_database *db = (struct _guile_database *)hp;
    SCM res;

    if (guile_proc[close_proc].name)
	guile_call_proc(&res, &guile_proc[close_proc],
			scm_list_2(scm_cons(SCM_IM_QUOTE,
					    scm_makfrom0str(db->dbname)),
				   scm_cons(SCM_IM_QUOTE, db->handle)));
    scm_gc_unprotect_object(db->handle);

    free(db);
    return 0;
}

static SCM
argv_to_scm(int argc, char **argv)
{
    SCM scm_first = SCM_EOL, scm_last;

    for (; argc; argc--, argv++) {
	SCM new = scm_cons(scm_makfrom0str(*argv), SCM_EOL);
	if (scm_first == SCM_EOL) 
	    scm_last = scm_first = new;
	else {
	    SCM_SETCDR(scm_last, new);
	    scm_last = new;
	}
    }
    return scm_first;
}
 
dico_handle_t
mod_open(const char *dbname, int argc, char **argv)
{
    struct _guile_database *db;

    db = malloc(sizeof(*db));
    if (!db) {
	memerr("mod_open");
	return NULL;
    }
    db->dbname = dbname;
    if (guile_call_proc(&db->handle, &guile_proc[open_proc],
			scm_list_2(scm_cons(SCM_IM_QUOTE,
					    scm_makfrom0str(db->dbname)),
			           scm_cons(SCM_IM_QUOTE,
					    argv_to_scm(argc, argv))))) {
	free(db);
	return NULL;
    }
    scm_gc_protect_object(db->handle);
    return (dico_handle_t)db;
}

static char *
mod_get_text(struct _guile_database *db, int n)
{
    if (guile_proc[n].name) {
	SCM res;
	
	if (guile_call_proc(&res, &guile_proc[n],
			    scm_list_2(scm_cons(SCM_IM_QUOTE,
						scm_makfrom0str(db->dbname)),
				       scm_cons(SCM_IM_QUOTE, db->handle))))
	    return NULL;
	if (scm_is_string(res)) 
	    return strdup(scm_i_string_chars(res));
	else {
	    rettype_error(guile_proc[n].name);
	    return NULL;
	}
    }
    return NULL;
}    

char *
mod_info(dico_handle_t hp)
{
    struct _guile_database *db = (struct _guile_database *)hp;
    return mod_get_text(db, info_proc);
}

char *
mod_descr(dico_handle_t hp)
{
    struct _guile_database *db = (struct _guile_database *)hp;
    return mod_get_text(db, descr_proc);
}



dico_result_t
mod_match(dico_handle_t hp, const dico_strategy_t strat, const char *word)
{
    struct _guile_database *db = (struct _guile_database *)hp;
    SCM scm_strat = _make_strategy(strat);
    SCM res;

    if (strat->sel
	&& strat->sel(DICO_SELECT_BEGIN, word, NULL, strat->closure)) {
	dico_log(L_ERR, 0, _("mod_match: initial select failed"));
	return NULL;
    }
    
    if (guile_call_proc(&res, &guile_proc[match_proc],
			scm_list_4(scm_cons(SCM_IM_QUOTE,
					    scm_makfrom0str(db->dbname)),
				   scm_cons(SCM_IM_QUOTE, db->handle),
				   scm_cons(SCM_IM_QUOTE, scm_strat),
				   scm_cons(SCM_IM_QUOTE,
					    scm_makfrom0str(word)))))
	return NULL;

    if (strat->sel)
	strat->sel(DICO_SELECT_END, word, NULL, strat->closure);

    if (res == SCM_BOOL_F || res == SCM_EOL)
	return NULL;
    scm_gc_protect_object(res);
    return (dico_result_t)res;
}

dico_result_t
mod_define(dico_handle_t hp, const char *word)
{
    struct _guile_database *db = (struct _guile_database *)hp;
    SCM res;
	
    if (guile_call_proc(&res, &guile_proc[define_proc],
			scm_list_3(scm_cons(SCM_IM_QUOTE,
					    scm_makfrom0str(db->dbname)),
				   scm_cons(SCM_IM_QUOTE, db->handle),
				   scm_cons(SCM_IM_QUOTE,
					    scm_makfrom0str(word)))))
	return NULL;

    if (res == SCM_BOOL_F || res == SCM_EOL)
	return NULL;
    scm_gc_protect_object(res);
    return (dico_result_t)res;
}

int
mod_output_result (dico_result_t rp, size_t n, dico_stream_t str)
{
    int rc;
    SCM handle = (SCM)rp;
    SCM res;
    SCM oport = scm_current_output_port();
    SCM port = _make_dico_port(str);
    
    scm_set_current_output_port(port);
    
    rc = guile_call_proc(&res, &guile_proc[output_proc],
			 scm_list_2(scm_cons(SCM_IM_QUOTE, handle),
				    scm_cons(SCM_IM_QUOTE,
					     scm_from_int(n))));
    scm_set_current_output_port(oport);
    _dico_port_close(port);
    if (rc)
	return 1;
    return 0;
}

size_t
mod_result_count (dico_result_t rp)
{
    SCM handle = (SCM)rp;
    SCM res;
    
    if (guile_call_proc(&res, &guile_proc[result_count_proc],
			scm_list_1(scm_cons(SCM_IM_QUOTE, handle))))
	return 0;
    if (scm_is_integer(res)) 
	return scm_to_int32(res);
    else
	rettype_error(guile_proc[result_count_proc].name);
    return 0;
}

size_t
mod_compare_count (dico_result_t rp)
{
    SCM handle = (SCM)rp;
    if (guile_proc[compare_count_proc].name) {
	SCM res;
	
	if (guile_call_proc(&res, &guile_proc[compare_count_proc],
			    scm_list_1(scm_cons(SCM_IM_QUOTE, handle))))
	    return 0;
	if (scm_is_integer(res)) 
	    return scm_to_int32(res);
	else 
	    rettype_error(guile_proc[compare_count_proc].name);
    }
    return 0;
}

void
mod_free_result(dico_result_t rp)
{
    SCM handle = (SCM)rp;
    if (guile_proc[free_result_proc].name) {
	SCM res;
	
	guile_call_proc(&res, &guile_proc[free_result_proc],
			scm_list_1(scm_cons(SCM_IM_QUOTE, handle)));
    }
    scm_gc_unprotect_object(handle);
}

struct dico_handler_module DICO_EXPORT(guile, module) = {
    DICO_MODULE_VERSION,
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
