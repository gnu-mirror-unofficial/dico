/* This file is part of GNU Dico
   Copyright (C) 2018-2019 Sergey Poznyakoff

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

#include <config.h>
#include "dico.h"
#include <string.h>

static void *dico_mergesort(void *a, void *b, size_t nmemb, size_t size,
		       int (*comp)(const void *, const void *, void *),
		       void *closure);
static void merge(void *source, void *work, size_t size,
		  size_t left, size_t right, size_t end,
		  int (*comp)(const void *, const void *, void *),
		  void *closure);

int
dico_sort(void *base, size_t nmemb, size_t size,
	  int (*comp)(const void *, const void *, void *),
	  void *closure)
{
    void *tmp, *res;

    tmp = calloc(nmemb, size);
    if (!tmp)
	return -1;
    res = dico_mergesort(base, tmp, nmemb, size, comp, closure);
    if (res != base)
	memcpy(base, res, nmemb * size);
    free(tmp);
    return 0;
}

static inline size_t
min(size_t a, size_t b)
{
    return a < b ? a : b;
}

static void *
dico_mergesort(void *a, void *b, size_t nmemb, size_t size,
	       int (*comp)(const void *, const void *, void *),
	       void *closure)
{
    size_t width;

    for (width = 1; width < nmemb; width <<= 1) {
	size_t i;
	void *t;
	
	for (i = 0; i < nmemb; i += 2 * width) {
	    merge(a, b, size,
		  i, min(i + width, nmemb), min(i + 2 * width, nmemb),
		  comp, closure);
	}
	t = a;
	a = b;
	b = t;
    }
    return a;
}

static void
merge(void *source, void *work, size_t size,
      size_t left, size_t right, size_t end,
      int (*comp)(const void *, const void *, void *),
      void *closure)
{
    size_t i = left;
    size_t j = right;
    size_t k;
#define MEMB(b,n) ((char*)(b) + (n) * size)
#define COPY(dp,dn,sp,sn) (memcpy(MEMB(dp,dn), MEMB(sp,sn), size))
    
    for (k = left; k < end; k++) {
	if (i < right
	    && (j >= end || comp(MEMB(source, i),
				 MEMB(source, j),
				 closure) <= 0)) {
	    COPY(work, k, source, i);
	    i++;
	} else {
	    COPY(work, k, source, j);
	    j++;
	}
    }
}
