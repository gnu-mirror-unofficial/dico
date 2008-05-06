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

#ifndef __dico_list_h
#define __dico_list_h

#include <sys/types.h>
#include <stdlib.h>

/* Lists */
typedef struct list *dico_list_t;
typedef struct iterator *dico_iterator_t;

typedef int (*dico_list_iterator_t)(void *item, void *data);
typedef int (*dico_list_comp_t)(const void *, const void *);

dico_list_t dico_list_create(void);
void dico_list_destroy(dico_list_t *list, dico_list_iterator_t free, void *data);
void dico_list_iterate(dico_list_t list, dico_list_iterator_t itr, void *data);
void *dico_list_item(dico_list_t list, size_t n);
size_t dico_list_count(dico_list_t list);
int dico_list_append(dico_list_t list, void *data);
int dico_list_prepend(dico_list_t list, void *data);
int dico_list_insert_sorted(dico_list_t list, void *data, dico_list_comp_t cmp);
dico_list_t  dico_list_intersect(dico_list_t a, dico_list_t b,
				 dico_list_comp_t cmp);

#define dico_list_push dico_list_prepend
void *dico_list_pop(dico_list_t list);

void *dico_list_locate(dico_list_t list, void *data, dico_list_comp_t cmp);
void *dico_list_remove(dico_list_t list, void *data, dico_list_comp_t cmp);

void *dico_iterator_current(dico_iterator_t itr);
dico_iterator_t dico_iterator_create(dico_list_t list);
void dico_iterator_destroy(dico_iterator_t *ip);
void *dico_iterator_first(dico_iterator_t ip);
void *dico_iterator_next(dico_iterator_t ip);

void *dico_iterator_remove_current(dico_iterator_t ip);
void dico_iterator_set_data(dico_iterator_t ip, void *data);

#endif
