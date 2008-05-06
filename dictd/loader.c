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
    return lt_dladdsearchdir(item);
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
	logmsg(L_ERR, 0, _("cannot load module %s: %s"), argv[0],
	       lt_dlerror());
	return 1;
    }

    pmod = (struct dico_handler_module *) lt_dlsym(handle, "module");
    if (!pmod) {
	lt_dlclose(handle);
	logmsg(L_ERR, 0, _("%s: faulty module"), argv[0]);
	return 1;
    }

    if (pmod->module_init && pmod->module_init(argc, argv)) {
	lt_dlclose(handle);
	logmsg(L_ERR, 0, _("%s: initialization failed"), argv[0]);
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
	
    if (rc = dico_argcv_get(hptr->command, NULL, NULL, &argc, &argv)) {
	logmsg(L_ERR, rc, _("cannot parse command line `%s'"), hptr->command);
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
	    logmsg(L_ERR, 0, _("cannot open module `%s'"), dp->command);
	    return 1;
	}
    }
    return 0;
}

int
dictd_database_get_strats(dictd_database_t *dp)
{
    dictd_handler_t *hptr = dp->handler;
    int rc;
    
    if (hptr->module->module_strats) 
	rc = hptr->module->module_strats(dp->mod, &dp->stratc, dp->stratv);
    else {
	dp->stratc = 1;
	dp->stratv = xcalloc(2, sizeof(dp->stratv[0]));
	dp->stratv[0] = xstrdup("exact");
	dp->stratv[1] = NULL;
	rc = 0;
    }
    return rc;
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
