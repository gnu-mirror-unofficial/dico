/* This file is part of Gjdcit
   Copyright (C) 2003,2004,2007,2008 Sergey Poznyakoff
  
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
#include <string.h>

dico_assoc_list_t
dico_assoc_create()
{
    return dico_list_create();
}

void
dico_assoc_add(dico_assoc_list_t assoc, const char *key, const char *value)
{
    struct dico_assoc *a;
    size_t size = sizeof(*a) + strlen(key) + strlen(value) + 2;
    a = xmalloc(size);
    a->key = (char*)(a + 1);
    strcpy(a->key, key);
    a->value = a->key + strlen(a->key) + 1;
    strcpy(a->value, value);
    dico_list_append(assoc, a);
}

static int
assoc_key_cmp(const void *item, const void *data)
{
    const struct dico_assoc *aptr = item;
    const char *sptr = data;
    return strcmp(aptr->key, sptr);
}

const char *
dico_assoc_find(dico_assoc_list_t assoc, const char *key)
{
    return dico_list_locate(assoc, (void*)key, assoc_key_cmp);
}

void
dico_assoc_remove(dico_assoc_list_t assoc, const char *key)
{
    dico_list_remove(assoc, (void*)key, assoc_key_cmp);
}

static int
assoc_free(void *item, void *data)
{
    free(item);
    return 0;
}

void
dico_assoc_destroy(dico_assoc_list_t *passoc)
{
    return dico_list_destroy(passoc, assoc_free, NULL);
}

