/* This file is part of GNU Dico.
   Copyright (C) 2008 Wojciech Polak

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <dico.h>
#include <string.h>
#include <errno.h>
#include <appi18n.h>
#include <Python.h>

static char *init_script;
static char *load_path;
static char *root_class = "DicoModule";

static struct dico_option init_option[] = {
    { DICO_OPTSTR(init-script), dico_opt_string, &init_script },
    { DICO_OPTSTR(load-path), dico_opt_string, &load_path },
    { DICO_OPTSTR(root-class), dico_opt_string, &root_class },
    { NULL }
};

struct _python_database {
    const char *dbname;
    int argc;
    char **argv;
    PyThreadState *py_ths;
    PyObject *py_instance;
};

typedef struct {
    PyObject_HEAD;
    dico_strategy_t strat;
} PyStrategy;


static inline PyObject *
_ro (PyObject *obj)
{
    Py_INCREF (obj);
    return obj;
}

static PyObject *
strat_select_method (PyObject *self, PyObject *args)
{
    PyStrategy *py_strat = (PyStrategy *)self;
    char *key = NULL;
    char *word = NULL;

    if (!PyArg_ParseTuple (args, "ss", &word, &key))
	return _ro (Py_False);

    return _ro (py_strat->strat->sel (DICO_SELECT_RUN, key, word,
				      py_strat->strat->closure)
		? Py_True : Py_False);
}

static PyMethodDef strategy_methods[] = {
    { "select", strat_select_method, METH_VARARGS,
      "Return True if KEY matches WORD as per strategy selector STRAT." },
    { NULL, NULL, 0, NULL }
};

static void 
_PyStrategy_dealloc (PyObject *self)
{
}

static PyObject *
_PyStrategy_getattr (PyObject *self, char *name)
{
    PyStrategy *py_strat = (PyStrategy *)self;
    dico_strategy_t strat = py_strat->strat;

    if (strcmp (name, "name") == 0) {
	/* Return the name of the strategy STRAT. */
	return PyString_FromString (strat->name);
    }
    else if (strcmp (name, "descr") == 0) {
	/* Return a textual description of the strategy STRAT. */
	return PyString_FromString (strat->descr);
    }
    else if (strcmp (name, "has_selector") == 0) {
	/* Return True if STRAT has a selector. */
	return _ro (strat->sel ? Py_True : Py_False);
    }
    else if (strcmp (name, "is_default") == 0) {
	/* Return True if STRAT is a default strategy. */
	return _ro (dico_strategy_is_default_p (strat) ? Py_True : Py_False);
    }
    return Py_FindMethod (strategy_methods, self, name);
}

static PyObject *
_PyStrategy_repr (PyObject *self)
{
    PyStrategy *py_strat = (PyStrategy *)self;
    char buf[80];
    snprintf (buf, sizeof buf, "<DicoStrategy %s>", py_strat->strat->name);
    return PyString_FromString (buf);
}

static PyObject *
_PyStrategy_str (PyObject *self)
{
    PyStrategy *py_strat = (PyStrategy *)self;
    return PyString_FromString (py_strat->strat->name);
}

static PyTypeObject PyStrategyType = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "DicoStrategy",      /* tp_name */
    sizeof (PyStrategy), /* tp_basicsize */
    0,                   /* tp_itemsize; */
    _PyStrategy_dealloc, /* tp_dealloc; */
    NULL,                /* tp_print; */
    _PyStrategy_getattr, /* tp_getattr; __getattr__ */
    NULL,                /* tp_setattr; __setattr__ */
    NULL,                /* tp_compare; __cmp__ */
    _PyStrategy_repr,    /* tp_repr; __repr__ */
    NULL,                /* tp_as_number */
    NULL,                /* tp_as_sequence */
    NULL,                /* tp_as_mapping */
    NULL,                /* tp_hash; __hash__ */
    NULL,                /* tp_call; __call__ */
    _PyStrategy_str,     /* tp_str; __str__ */
};

static dico_stream_t dico_stream_output;
static dico_stream_t dico_stream_log;

static PyObject *
_capture_stdout (PyObject *self, PyObject *args)
{
    char *buf = "";
    if (!PyArg_ParseTuple (args, "s", &buf))
	return NULL;
    if (dico_stream_output)
      dico_stream_write (dico_stream_output, buf, strlen (buf));
    return _ro (Py_None);
}

static PyObject *
_capture_stderr (PyObject *self, PyObject *args)
{
    char *buf = "";
    if (!PyArg_ParseTuple (args, "s", &buf))
	return NULL;
    if (dico_stream_log)
      dico_stream_write (dico_stream_log, buf, strlen (buf));
    return _ro (Py_None);
}

static PyMethodDef capture_stdout_method[] =
{
    { "write", _capture_stdout, 1 },
    { NULL, NULL, 0, NULL }
};
static PyMethodDef capture_stderr_method[] =
{
    { "write", _capture_stderr, 1 },
    { NULL, NULL, 0, NULL }
};

static int
_python_selector (int cmd, const char *word, const char *dict_word,
		  void *closure)
{
    PyObject *py_args, *py_res;

    py_args = PyTuple_New (3);
    PyTuple_SetItem (py_args, 0, PyInt_FromLong (cmd));
    PyTuple_SetItem (py_args, 1, PyString_FromString (word));
    PyTuple_SetItem (py_args, 2, PyString_FromString (dict_word));

    if (closure && PyCallable_Check (closure)) {
	py_res = PyObject_CallObject (closure, py_args);
	if (py_res) {
	    if (PyBool_Check (py_res))
		return py_res == Py_True ? 1 : 0;
	}
	else if (PyErr_Occurred ())
	    PyErr_Print ();
    }
    return 0;
}

static PyObject *
dico_register_strat (PyObject *self, PyObject *args)
{
    struct dico_strategy strat;
    char *name = NULL;
    char *descr = NULL;
    char *fnc = NULL;

    if (!PyArg_ParseTuple (args, "ss|s", &name, &descr, &fnc))
        return NULL;

    strat.name = name;
    strat.descr = descr;
    if (fnc) {
	strat.sel = NULL;
	strat.closure = NULL;
    }
    else {
	strat.sel = _python_selector;
	strat.closure = fnc;
    }
    dico_strategy_add (&strat);

    return _ro (Py_None);
}

static PyMethodDef dico_methods[] = {
    { "register_strat", dico_register_strat, METH_VARARGS,
      "Register a new strategy." },
    { NULL, NULL, 0, NULL }
};


static PyObject *
_argv_to_tuple (int argc, char **argv)
{
    int i;
    PyObject *py_args = PyTuple_New (argc);
    for (i = 0; argc; argc--, argv++, i++) {
	PyTuple_SetItem (py_args, i, PyString_FromString (*argv));
    }
    return py_args;
}

static int
mod_init (int argc, char **argv)
{
    if (dico_parseopt (init_option, argc, argv, 0, NULL))
	return 1;

    if (!Py_IsInitialized ())
	Py_Initialize ();

    dico_stream_log = dico_log_stream_create (L_ERR);
    return 0;
}

static void
insert_load_path (const char *dir)
{
    PyObject *py_sys, *py_path, *py_dirstr;
    const char *p;

    py_sys = PyImport_ImportModule ("sys");
    py_path = PyObject_GetAttrString (py_sys, "path");
    p = dir + strlen (dir);
    do {
	size_t len;
	for (len = 0; p > dir && p[-1] != ':'; p--, len++)
	    ;
	py_dirstr = PyString_FromStringAndSize (p, len);
	if (PySequence_Index (py_path, py_dirstr) == -1) {
	    PyObject *py_list;
	    PyErr_Clear ();
	    py_list = Py_BuildValue ("[O]", py_dirstr);
	    PyList_SetSlice (py_path, 0, 0, py_list);
	    Py_DECREF (py_list);
	}
	Py_DECREF (py_dirstr);
    } while (p-- > dir);
    Py_DECREF (py_path);
    Py_DECREF (py_sys);
}

static dico_handle_t
mod_init_db (const char *dbname, int argc, char **argv)
{
    int pindex;
    struct _python_database *db;
    PyObject *py_err, *py_name, *py_module, *py_class;
    PyThreadState *py_ths;

    if (dico_parseopt (init_option, argc, argv, DICO_PARSEOPT_PERMUTE,
		       &pindex))
	return NULL;

    if (!init_script)
	return NULL;

    argv += pindex;
    argc -= pindex;

    db = malloc (sizeof (*db));
    if (!db) {
	dico_log (L_ERR, 0, _("%s: not enough memory"), "mod_init_db");
	return NULL;
    }
    db->dbname = dbname;
    db->argc = argc;
    db->argv = argv;

    py_ths = Py_NewInterpreter ();
    if (!py_ths) {
	dico_log (L_ERR, 0,
		  _("mod_init_db: cannot create new interpreter: %s"), 
		  init_script);
	return NULL;
    }

    PyThreadState_Swap (py_ths);
    db->py_ths = py_ths;

    Py_InitModule ("dico", dico_methods);

    PyRun_SimpleString ("import sys");
    if (load_path)
	insert_load_path (load_path);
    insert_load_path ("");

    py_err = Py_InitModule ("stderr", capture_stderr_method);
    if (py_err)
	PySys_SetObject ("stderr", py_err);

    py_name = PyString_FromString (init_script);
    py_module = PyImport_Import (py_name);
    Py_DECREF (py_name);

    if (!py_module) {
	dico_log (L_ERR, 0, _("mod_init_db: cannot load init script: %s"), 
		  init_script);
	if (PyErr_Occurred ())
	    PyErr_Print ();
	return NULL;
    }

    py_class = PyObject_GetAttrString (py_module, root_class);
    if (py_class && PyClass_Check (py_class)) {
	PyObject *py_instance = PyInstance_New (py_class,
						_argv_to_tuple (argc, argv),
						NULL);
	if (py_instance && PyInstance_Check (py_instance))
	    db->py_instance = py_instance;
	else if (PyErr_Occurred ()) {
	    PyErr_Print ();
	    return NULL;
	}
    }
    else {
	dico_log (L_ERR, 0, _("mod_init_db: cannot create class instance: %s"),
		  root_class);
	if (PyErr_Occurred ())
	    PyErr_Print ();
	return NULL;
    }
    return (dico_handle_t)db;
}

static int
mod_free_db (dico_handle_t hp)
{
    struct _python_database *db = (struct _python_database *)hp;

    PyThreadState_Swap (NULL);
    PyThreadState_Delete (db->py_ths);

    free (db);
    return 0;
}

static int
mod_open (dico_handle_t hp)
{
    PyObject *py_args, *py_value, *py_fnc, *py_res;
    struct _python_database *db = (struct _python_database *)hp;

    PyThreadState_Swap (db->py_ths);

    py_value = PyString_FromString (db->dbname);
    py_args = PyTuple_New (1);
    PyTuple_SetItem (py_args, 0, py_value);

    py_fnc = PyObject_GetAttrString (db->py_instance, "open");
    if (py_fnc && PyCallable_Check (py_fnc)) {
	py_res = PyObject_CallObject (py_fnc, py_args);
	Py_DECREF (py_args);
	Py_DECREF (py_fnc);
	if (py_res && PyBool_Check (py_res) && py_res == Py_False) {
	    return 1;
	}
	else if (PyErr_Occurred ()) {
	    PyErr_Print ();
	    return 1;
	}
    }
    return 0;
}

static int
mod_close (dico_handle_t hp)
{
    PyObject *py_fnc, *py_res;
    struct _python_database *db = (struct _python_database *)hp;

    PyThreadState_Swap (db->py_ths);

    py_fnc = PyObject_GetAttrString (db->py_instance, "close");
    if (py_fnc && PyCallable_Check (py_fnc)) {
	py_res = PyObject_CallObject (py_fnc, NULL);
	Py_DECREF (py_fnc);
	if (py_res && PyBool_Check (py_res) && py_res == Py_False) {
	    return 1;
	}
	else if (PyErr_Occurred ()) {
	    PyErr_Print ();
	    return 1;
	}
    }
    return 0;
}

static char *
_mod_get_text (PyObject *py_instance, const char *method)
{
    PyObject *py_fnc, *py_res;

    if (!py_instance)
	return NULL;
    else if (!PyObject_HasAttrString (py_instance, method))
	return NULL;
    
    py_fnc = PyObject_GetAttrString (py_instance, method);
    if (py_fnc && PyCallable_Check (py_fnc)) {
	py_res = PyObject_CallObject (py_fnc, NULL);
	Py_DECREF (py_fnc);
	if (py_res && PyString_Check (py_res)) {
	    char *text = strdup (PyString_AsString (py_res));
	    Py_DECREF (py_res);
	    return text;
	}
	else if (PyErr_Occurred ())
	    PyErr_Print ();
    }
    return NULL;
}

static char *
mod_info (dico_handle_t hp)
{
    struct _python_database *db = (struct _python_database *)hp;

    PyThreadState_Swap (db->py_ths);
    return _mod_get_text (db->py_instance, "info");
}

static char *
mod_descr (dico_handle_t hp)
{
    struct _python_database *db = (struct _python_database *)hp;

    PyThreadState_Swap (db->py_ths);
    return _mod_get_text (db->py_instance, "descr");
}

struct python_result {
    struct _python_database *db;
    PyObject *result;
};

static dico_result_t
_make_python_result (struct _python_database *db, PyObject *res)
{
    struct python_result *rp = malloc (sizeof (*rp));
    if (rp) {
	rp->db = db;
	rp->result = res;
    }
    return (dico_result_t)rp;
}

static dico_result_t
mod_match (dico_handle_t hp, const dico_strategy_t strat, const char *word)
{
    PyStrategy *py_strat;
    PyObject *py_args, *py_fnc, *py_res;
    struct _python_database *db = (struct _python_database *)hp;

    PyThreadState_Swap (db->py_ths);

    if (strat->sel
	&& strat->sel (DICO_SELECT_BEGIN, word, NULL, strat->closure)) {
      dico_log (L_ERR, 0, _("mod_match: initial select failed"));
      return NULL;
    }
    
    py_strat = PyObject_NEW (PyStrategy, &PyStrategyType);
    if (py_strat) {
	py_strat->strat = strat;

	py_args = PyTuple_New (2);
	PyTuple_SetItem (py_args, 0, (PyObject *)py_strat);
	PyTuple_SetItem (py_args, 1, PyString_FromString (word));
	py_fnc = PyObject_GetAttrString (db->py_instance, "match_word");
	if (py_fnc && PyCallable_Check (py_fnc)) {
	    py_res = PyObject_CallObject (py_fnc, py_args);
	    Py_DECREF (py_args);
	    Py_DECREF (py_fnc);
	    if (py_res) {
		if (PyBool_Check (py_res) && py_res == Py_False)
		    return NULL;
		else {
		    if (strat->sel) {
			strat->sel (DICO_SELECT_END, word, NULL,
				    strat->closure);
		    }
		    return _make_python_result (db, py_res);
		}
	    }
	    else if (PyErr_Occurred ())
		PyErr_Print ();
	}
    }
    return NULL;
}

static dico_result_t
mod_define (dico_handle_t hp, const char *word)
{
    PyObject *py_args, *py_fnc, *py_res;
    struct _python_database *db = (struct _python_database *)hp;

    PyThreadState_Swap (db->py_ths);

    py_args = PyTuple_New (1);
    PyTuple_SetItem (py_args, 0, PyString_FromString (word));

    py_fnc = PyObject_GetAttrString (db->py_instance, "define_word");
    if (py_fnc && PyCallable_Check (py_fnc)) {
	py_res = PyObject_CallObject (py_fnc, py_args);
	Py_DECREF (py_args);
	Py_DECREF (py_fnc);
	if (py_res) {
	    if (PyBool_Check (py_res) && py_res == Py_False)
		return NULL;
	    else
		return _make_python_result (db, py_res);
	}
	else if (PyErr_Occurred ())
	    PyErr_Print ();
    }
    return NULL;
}

static int
mod_output_result (dico_result_t rp, size_t n, dico_stream_t str)
{
    PyObject *py_args, *py_fnc, *py_res, *py_out_orig, *py_out;
    struct python_result *gres = (struct python_result *)rp;
    struct _python_database *db = (struct _python_database *)gres->db;

    PyThreadState_Swap (db->py_ths);

    dico_stream_output = str;

    py_out_orig = PySys_GetObject ("stdout");
    py_out = Py_InitModule ("stdout", capture_stdout_method);
    if (py_out)
	PySys_SetObject ("stdout", py_out);

    py_args = PyTuple_New (2);
    PyTuple_SetItem (py_args, 0, gres->result);
    Py_INCREF (gres->result);
    PyTuple_SetItem (py_args, 1, PyLong_FromLong (n));

    py_fnc = PyObject_GetAttrString (db->py_instance, "output");
    if (py_fnc && PyCallable_Check (py_fnc)) {
	py_res = PyObject_CallObject (py_fnc, py_args);
	Py_DECREF (py_args);
	Py_DECREF (py_fnc);
	if (PyErr_Occurred ())
	    PyErr_Print ();
    }

    if (py_out)
	PySys_SetObject ("stdout", py_out_orig);

    dico_stream_output = NULL;
    return 0;
}

static size_t
_mod_get_size_t (PyObject *py_instance, PyObject *py_args, const char *method)
{
    PyObject *py_fnc, *py_res;

    if (!py_instance)
	return 0;

    py_fnc = PyObject_GetAttrString (py_instance, method);
    if (py_fnc && PyCallable_Check (py_fnc)) {
	py_res = PyObject_CallObject (py_fnc, py_args);
	Py_DECREF (py_fnc);
	if (py_res && PyInt_Check (py_res)) {
	    size_t s = (size_t)PyInt_AsSsize_t (py_res);
	    Py_DECREF (py_res);
	    return s;
	}
	else if (PyErr_Occurred ())
	    PyErr_Print ();
    }
    return 0;
}

static size_t
mod_result_count (dico_result_t rp)
{
    PyObject *py_args;
    struct python_result *gres = (struct python_result *)rp;
    struct _python_database *db = (struct _python_database *)gres->db;
    size_t ret;

    PyThreadState_Swap (db->py_ths);

    py_args = PyTuple_New (1);
    PyTuple_SetItem (py_args, 0, gres->result);
    Py_INCREF (gres->result);
    ret = _mod_get_size_t (db->py_instance, py_args, "result_count");
    Py_DECREF (py_args);
    return ret;
}

static size_t
mod_compare_count (dico_result_t rp)
{
    PyObject *py_args;
    struct python_result *gres = (struct python_result *)rp;
    struct _python_database *db = (struct _python_database *)gres->db;
    size_t ret;

    PyThreadState_Swap (db->py_ths);
    if (!PyObject_HasAttrString (db->py_instance, "compare_count"))
	return 0;

    py_args = PyTuple_New (1);
    PyTuple_SetItem (py_args, 0, gres->result);
    Py_INCREF (gres->result);
    ret = _mod_get_size_t (db->py_instance, py_args, "compare_count");
    Py_DECREF (py_args);
    return ret;
}

static void
mod_free_result (dico_result_t rp)
{
    PyObject *py_args, *py_fnc;
    struct python_result *gres = (struct python_result *)rp;
    struct _python_database *db = (struct _python_database *)gres->db;

    PyThreadState_Swap (db->py_ths);
    if (!PyObject_HasAttrString (db->py_instance, "free_result"))
	return;

    py_args = PyTuple_New (1);
    PyTuple_SetItem (py_args, 0, gres->result);
    Py_INCREF (gres->result);
    py_fnc = PyObject_GetAttrString (db->py_instance, "free_result");
    if (py_fnc && PyCallable_Check (py_fnc)) {
	PyObject_CallObject (py_fnc, py_args);
	Py_DECREF (py_args);
	Py_DECREF (py_fnc);
	if (PyErr_Occurred ())
	    PyErr_Print ();
    }
    Py_DECREF (gres->result);
    free (gres);
}

struct dico_database_module DICO_EXPORT(python, module) = {
    DICO_MODULE_VERSION,
    DICO_CAPA_NONE,
    mod_init,
    mod_init_db,
    mod_free_db,
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
