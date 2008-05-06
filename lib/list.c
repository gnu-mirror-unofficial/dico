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
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>

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
    dico_list_t list;
    struct list_entry *cur;
    int advanced;
};

struct list *
dico_list_create()
{
    struct list *p = malloc(sizeof(*p));
    if (p) {
	    p->head = p->tail = NULL;
	    p->itr = NULL;
    }
    return p;
}

void
dico_list_destroy(struct list **plist, dico_list_iterator_t user_free,
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
dico_iterator_current(dico_iterator_t ip)
{
    if (!ip)
	return NULL;
    return ip->cur ? ip->cur->data : NULL;
}

static void
dico_iterator_attach(dico_iterator_t itr, dico_list_t list)
{
    itr->list = list;
    itr->cur = NULL;
    itr->next = list->itr;
    itr->advanced = 0;
    list->itr = itr;	
}

static dico_iterator_t 
dico_iterator_detach(dico_iterator_t iter)
{
    dico_iterator_t cur, prev;
    
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

dico_iterator_t
dico_iterator_create(dico_list_t list)
{
    dico_iterator_t itr;
    
    if (!list) {
        errno = EINVAL;    
	return NULL;
    }
    itr = malloc(sizeof(*itr));
    if (itr)
        dico_iterator_attach(itr, list);
    return itr;
}

void
dico_iterator_destroy(dico_iterator_t *ip)
{
    dico_iterator_t itr;
    
    if (!ip || !*ip)
	return;
    itr = dico_iterator_detach(*ip);
    if (itr)
	free(itr);
    *ip = NULL;
}
		
void *
dico_iterator_first(dico_iterator_t ip)
{
    if (!ip)
	return NULL;
    ip->cur = ip->list->head;
    ip->advanced = 0;
    return dico_iterator_current(ip);
}

void *
dico_iterator_next(dico_iterator_t ip)
{
    if (!ip || !ip->cur)
	return NULL;
    if (!ip->advanced)
	ip->cur = ip->cur->next;
    ip->advanced = 0;
    return dico_iterator_current(ip);
}	

void *
dico_iterator_remove_current(dico_iterator_t ip)
{
    return dico_list_remove(ip->list, ip->cur->data, NULL);
}

void
dico_iterator_set_data(dico_iterator_t ip, void *data)
{
    ip->cur->data = data;
}

static void
_iterator_advance(dico_iterator_t ip, struct list_entry *e)
{
    for (; ip; ip = ip->next) {
	if (ip->cur == e) {
	    ip->cur = e->next;
	    ip->advanced++;
	}
    }
}

void *
dico_list_item(struct list *list, size_t n)
{
    struct list_entry *p;
    if (!list || n >= list->count)
	return NULL;
    for (p = list->head; n > 0 && p; p = p->next, n--)
	;
    return p->data;
}

size_t
dico_list_count(struct list *list)
{
    if (!list)
	return 0;
    return list->count;
}

int
dico_list_append(struct list *list, void *data)
{
    struct list_entry *ep;
    
    if (!list) {
	errno = EINVAL;    
	return 1;
    }
    ep = malloc(sizeof(*ep));
    if (!ep)
	return 1;
    ep->next = NULL;
    ep->data = data;
    if (list->tail)
	list->tail->next = ep;
    else
	list->head = ep;
    list->tail = ep;
    list->count++;
    return 0;
}

int
dico_list_prepend(struct list *list, void *data)
{
    struct list_entry *ep;
    
    if (!list) {
	errno = EINVAL;
	return 1;
    }
    ep = malloc(sizeof(*ep));
    if (!ep)
	return 1;
    ep->data = data;
    ep->next = list->head;
    list->head = ep;
    if (!list->tail)
	list->tail = list->head;
    list->count++;
    return 0;
}

static int
cmp_ptr(const void *a, const void *b)
{
    return a != b;
}

void *
dico_list_remove(struct list *list, void *data, dico_list_comp_t cmp)
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
dico_list_pop(struct list *list)
{
    return dico_list_remove(list, list->head->data, NULL);
}

/* Note: if modifying this function, make sure it does not allocate any
   memory! */
void
dico_list_iterate(struct list *list, dico_list_iterator_t func, void *data)
{
    struct iterator itr;
    void *p;
	
    if (!list)
	return;
    dico_iterator_attach(&itr, list);
    for (p = dico_iterator_first(&itr); p; p = dico_iterator_next(&itr)) {
	if (func(p, data))
	    break;
    }
    dico_iterator_detach(&itr);
}

void *
dico_list_locate(struct list *list, void *data, dico_list_comp_t cmp)
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
dico_list_insert_sorted(struct list *list, void *data, dico_list_comp_t cmp)
{
    int rc;
    struct list_entry *cur, *prev;
    
    if (!list || !cmp) {
	errno = EINVAL;
	return 1;
    }
    
    for (cur = list->head, prev = NULL; cur; prev = cur, cur = cur->next)
	if (cmp(cur->data, data) > 0)
	    break;
    
    if (!prev) {
	rc = dico_list_prepend(list, data);
    } else if (!cur) {
	rc = dico_list_append(list, data);
    } else {
	struct list_entry *ep = malloc(sizeof(*ep));
	if (ep) {
	    rc = 0;
	    ep->data = data;
	    ep->next = cur;
	    prev->next = ep;
	} else
	    rc = 1;
    }
    return rc;
}

/* Computes an intersection of the two lists. The resulting list
   contains elements from the list A that are also encountered
   in the list B. Elements are compared using function CMP.
   The resulting list preserves the ordering of A. */
dico_list_t 
dico_list_intersect (dico_list_t a, dico_list_t b, dico_list_comp_t cmp)
{
    dico_list_t res;
    dico_iterator_t itr = dico_iterator_create(a);
    void *p;
    
    if (!itr)
	return NULL;
    res = dico_list_create();
    if (!res)
	return NULL;
    for (p = dico_iterator_first(itr); p; p = dico_iterator_next(itr)) {
	if (dico_list_locate(b, p, cmp))
	    dico_list_append(res, p); /* FIXME: check return, and? */
    }
    dico_iterator_destroy (&itr);
    return res;
}

