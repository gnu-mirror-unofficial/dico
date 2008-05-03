/* This file is part of Gjdcit
   Copyright (C) 2003,2004,2007,2008 Sergey Poznyakoff
  
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <gjdict.h>
#include <string.h>

dict_assoc_list_t
dict_assoc_create()
{
    return dict_list_create();
}

void
dict_assoc_add(dict_assoc_list_t assoc, const char *key, const char *value)
{
    struct dict_assoc *a;
    size_t size = sizeof(*a) + strlen(key) + strlen(value) + 2;
    a = xmalloc(size);
    a->key = (char*)(a + 1);
    strcpy(a->key, key);
    a->value = a->key + strlen(a->key) + 1;
    strcpy(a->value, value);
    dict_list_append(assoc, a);
}

static int
assoc_key_cmp(const void *item, const void *data)
{
    const struct dict_assoc *aptr = item;
    const char *sptr = data;
    return strcmp(aptr->key, sptr);
}

const char *
dict_assoc_find(dict_assoc_list_t assoc, const char *key)
{
    return dict_list_locate(assoc, (void*)key, assoc_key_cmp);
}

void
dict_assoc_remove(dict_assoc_list_t assoc, const char *key)
{
    dict_list_remove(assoc, (void*)key, assoc_key_cmp);
}

static int
assoc_free(void *item, void *data)
{
    free(item);
    return 0;
}

void
dict_assoc_destroy(dict_assoc_list_t *passoc)
{
    return dict_list_destroy(passoc, assoc_free, NULL);
}

