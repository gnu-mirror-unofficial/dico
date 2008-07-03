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

#include <dicod.h>

struct dicod_capa {
    const char *name;
    struct dicod_command *cmd;
    int (*init)(void *);
    void *closure;
    int enabled;
};

/* List of supported capabilities: */
static dico_list_t /* of struct dicod_capa */ capa_list;

void
dicod_capa_register(const char *name, struct dicod_command *cmd,
		    int (*init)(void*), void *closure)
{
    struct dicod_capa *cp = xmalloc(sizeof(*cp));
    cp->name = name;
    cp->cmd = cmd;
    cp->init = init;
    cp->closure = closure;
    cp->enabled = 0;
    if (!capa_list)
	capa_list = xdico_list_create();
    xdico_list_append(capa_list, cp);
}

static int
_cmp_capa_name(const void *item, const void *data)
{
    const struct dicod_capa *cp = item;
    return strcmp(cp->name, (char*)data);
}

int
dicod_capa_add(const char *name)
{
    struct dicod_capa *cp = dico_list_locate(capa_list, (void*)name, 
					     _cmp_capa_name);
    if (cp == NULL)
	return 1;
    cp->enabled = 1;
    return 0;
}

int
dicod_capa_flush()
{
    dico_iterator_t itr;
    struct dicod_capa *cp;
    
    itr = xdico_iterator_create(capa_list);
    for (cp = dico_iterator_first(itr); cp; cp = dico_iterator_next(itr)) {
	if (cp->enabled) {
	    if (cp->init && cp->init(cp->closure))
		return 1;
	    if (cp->cmd)
		dicod_add_command(cp->cmd);
	}
    }
    return 0;
}

struct iter_data {
    int (*fun)(const char*, int, void *);
    void *closure;
};

static int
_iter_helper(void *item, void *data)
{
    struct dicod_capa *cp = item;
    struct iter_data *dp = data;
    return dp->fun(cp->name, cp->enabled, dp->closure);
}
	
void
dicod_capa_iterate(int (*fun)(const char*, int, void *), void *closure)
{
    struct iter_data dat;
    dat.fun = fun;
    dat.closure = closure;
    dico_list_iterate(capa_list, _iter_helper, &dat);
}
    


