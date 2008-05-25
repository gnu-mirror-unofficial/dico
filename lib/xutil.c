/* This file is part of Gjdcit
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
#include <xdico.h>
#include <xalloc.h>
#include <string.h>
#include <errno.h>

void
trimnl(char *buf, size_t len)
{
    if (len > 1 && buf[--len] == '\n') {
	buf[len] = 0;
	if (len > 1 && buf[--len] == '\r')
	    buf[len] = 0;
    }
}


/* X-versions of dico library calls */

char *
make_full_file_name(const char *dir, const char *file)
{
    char *s = dico_full_file_name(dir, file);
    if (!s)
	xalloc_die();
    return s;
}

dico_list_t
xdico_list_create()
{
    dico_list_t p = dico_list_create();
    if (!p)
	xalloc_die();
    return p;
}

dico_iterator_t
xdico_iterator_create(dico_list_t list)
{
    dico_iterator_t p = dico_iterator_create(list); 
    if (!p && errno == ENOMEM)
	xalloc_die();
    return p;
}

void
xdico_list_append(struct list *list, void *data)
{
    if (dico_list_append(list, data) && errno == ENOMEM)
	xalloc_die();
}

void
xdico_list_prepend(struct list *list, void *data)
{
    if (dico_list_prepend(list, data) && errno == ENOMEM)
	xalloc_die();
}

dico_assoc_list_t
xdico_assoc_create()
{
    dico_assoc_list_t p = dico_assoc_create();
    if (!p)
	xalloc_die();
    return p;
}

void
xdico_assoc_add(dico_assoc_list_t assoc, const char *key, const char *value)
{
    if (dico_assoc_add(assoc, key, value) && errno == EINVAL)
	xalloc_die();
}
