/* This file is part of GNU Dico.
   Copyright (C) 2008 Sergey Poznyakoff

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

const char *dico_markup_type = "none";
dico_list_t dico_markup_list;

static int
cmp_markup_name(const void *item, const void *data)
{
    return strcasecmp((char*)item, (char*)data);
}

const char *
dico_markup_lookup(const char *name)
{
    return dico_list_locate(dico_markup_list, (void *)name, cmp_markup_name);
}
	
int
dico_markup_register(const char *name)
{
    if (!dico_markup_list) {
	dico_markup_list = dico_list_create();
	if (!dico_markup_list)
	    return 1;
    }

    if (!dico_markup_lookup(name)) {
	char *s = strdup(name);
	if (!s)
	    return 1;
	return dico_list_append(dico_markup_list, s);
    }
    return 0;
}

