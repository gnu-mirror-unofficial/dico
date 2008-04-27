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
#include <sys/types.h>
#include <stdlib.h>

struct list_entry {
    struct list_entry *next;
    void *data;
};

struct list {
    size_t count;
    struct list_entry *head, *tail;
    struct iterator *itr;
};

struct iterator {
    struct iterator *next;
    dict_list_t *list;
    struct list_entry *cur;
    int advanced;
};

struct list *
dict_list_create()
{
    struct list *p = xmalloc(sizeof(*p));
    p->head = p->tail = NULL;
    p->itr = NULL;
    return p;
}

void
dict_list_destroy(struct list **plist, dict_list_iterator_t user_free,
		  void *data)
{
    struct list_entry *p;

    if (!*plist)
	return;
    
    p = (*plist)->head;
    while (p) {
	struct list_entry *next = p->next;
	if (user_free)
	    user_free(p->data, data);
	free(p);
	p = next;
    }
    free(*plist);
    *plist = NULL;
}

void *
dict_iterator_current(dict_iterator_t *ip)
{
    if (!ip)
	return NULL;
    return ip->cur ? ip->cur->data : NULL;
}

static void
dict_iterator_attach(dict_iterator_t *itr, dict_list_t *list)
{
    itr->list = list;
    itr->cur = NULL;
    itr->next = list->itr;
    itr->advanced = 0;
    list->itr = itr;	
}

static dict_iterator_t *
dict_iterator_detach(dict_iterator_t *iter)
{
    dict_iterator_t *cur, *prev;
    
    for (cur = iter->list->itr, prev = NULL;
	 cur;
	 prev = cur, cur = cur->next)
	if (cur == iter)
	    break;
    
    if (cur) {
	if (prev)
	    prev->next = cur->next;
	else
	    cur->list->itr = cur->next;
    }
    return cur;
}

dict_iterator_t *
dict_iterator_create(dict_list_t *list)
{
    dict_iterator_t *itr;
    
    if (!list)
	return NULL;
    itr = xmalloc(sizeof(*itr));
    dict_iterator_attach(itr, list);
    return itr;
}

void
dict_iterator_destroy(dict_iterator_t **ip)
{
    dict_iterator_t *itr;
    
    if (!ip || !*ip)
	return;
    itr = dict_iterator_detach(*ip);
    if (itr)
	free(itr);
    *ip = NULL;
}
		
void *
dict_iterator_first(dict_iterator_t *ip)
{
    if (!ip)
	return NULL;
    ip->cur = ip->list->head;
    ip->advanced = 0;
    return dict_iterator_current(ip);
}

void *
dict_iterator_next(dict_iterator_t *ip)
{
    if (!ip || !ip->cur)
	return NULL;
    if (!ip->advanced)
	ip->cur = ip->cur->next;
    ip->advanced = 0;
    return dict_iterator_current(ip);
}	

void
dict_iterator_remove_current(dict_iterator_t *ip)
{
    dict_list_remove(ip->list, ip->cur->data, NULL);
}

void
dict_iterator_set_data(dict_iterator_t *ip, void *data)
{
    ip->cur->data = data;
}

static void
_iterator_advance(dict_iterator_t *ip, struct list_entry *e)
{
    for (; ip; ip = ip->next) {
	if (ip->cur == e) {
	    ip->cur = e->next;
	    ip->advanced++;
	}
    }
}

void *
dict_list_item(struct list *list, size_t n)
{
    struct list_entry *p;
    if (!list || n >= list->count)
	return NULL;
    for (p = list->head; n > 0 && p; p = p->next, n--)
	;
    return p->data;
}

size_t
dict_list_count(struct list *list)
{
    if (!list)
	return 0;
    return list->count;
}

void
dict_list_append(struct list *list, void *data)
{
    struct list_entry *ep;
    
    if (!list)
	return;
    ep = xmalloc(sizeof(*ep));
    ep->next = NULL;
    ep->data = data;
    if (list->tail)
	list->tail->next = ep;
    else
	list->head = ep;
    list->tail = ep;
    list->count++;
}

void
dict_list_prepend(struct list *list, void *data)
{
    struct list_entry *ep;
    
    if (!list)
	return;
    ep = xmalloc(sizeof(*ep));
    ep->data = data;
    ep->next = list->head;
    list->head = ep;
    if (!list->tail)
	list->tail = list->head;
    list->count++;
}

static int
cmp_ptr(const void *a, const void *b)
{
    return a != b;
}

void *
dict_list_remove(struct list *list, void *data, dict_list_comp_t cmp)
{
    struct list_entry *p, *prev;

    if (!list)
	return NULL;
    if (!list->head)
	return NULL;
    if (!cmp)
	cmp = cmp_ptr;
    for (p = list->head, prev = NULL; p; prev = p, p = p->next)
	if (cmp(p->data, data) == 0)
	    break;
    
    if (!p)
	return 0;
    _iterator_advance(list->itr, p);
    if (p == list->head) {
	list->head = list->head->next;
	if (!list->head)
	    list->tail = NULL;
    } else 
	prev->next = p->next;
    
    if (p == list->tail)
	list->tail = prev;
    
    free(p);
    list->count--;
    
    return data;
}

void *
dict_list_pop(struct list *list)
{
    return dict_list_remove(list, list->head->data, NULL);
}

/* Note: if modifying this function, make sure it does not allocate any
   memory! */
void
dict_list_iterate(struct list *list, dict_list_iterator_t func, void *data)
{
    dict_iterator_t itr;
    void *p;
	
    if (!list)
	return;
    dict_iterator_attach(&itr, list);
    for (p = dict_iterator_first(&itr); p; p = dict_iterator_next(&itr)) {
	if (func(p, data))
	    break;
    }
    dict_iterator_detach(&itr);
}

void *
dict_list_locate(struct list *list, void *data, dict_list_comp_t cmp)
{
    struct list_entry *cur;
    if (!list)
	return NULL;
    if (!cmp)
	cmp = cmp_ptr;
    for (cur = list->head; cur; cur = cur->next)
	if (cmp(cur->data, data) == 0)
	    break;
    return cur ? cur->data : NULL;
}
	
int
dict_list_insert_sorted(struct list *list, void *data, dict_list_comp_t cmp)
{
    struct list_entry *cur, *prev;
    
    if (!list)
	return -1;
    if (!cmp)
	return -1;
    
    for (cur = list->head, prev = NULL; cur; prev = cur, cur = cur->next)
	if (cmp(cur->data, data) > 0)
	    break;
    
    if (!prev) {
	dict_list_prepend(list, data);
    } else if (!cur) {
	dict_list_append(list, data);
    } else {
	struct list_entry *ep = xmalloc(sizeof(*ep));
	ep->data = data;
	ep->next = cur;
	prev->next = ep;
    }
    return 0;
}

