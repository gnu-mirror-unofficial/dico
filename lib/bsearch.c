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

/* Searches a sorted array of NELEM objects, the initial element being
   pointed to by BASE, for a member that matches the object pointed to
   by KEY. The size of each array member is given by ELSIZE. The array
   must be sorted in ascending order according to the comparison function
   COMP.

   The COMP function is called with three arguments: pointer to the
   key object, pointer to an array element, and CLOSURE. It should return
   negative value, 0, or positive value, depending on whether the key is
   found, respectively, to be less than, to match, or to be greater than
   the array member. The CLOSURE argument can be used to supply additional
   data to the COMP function. Its use is left to the discretion of the
   programmer.

   If the array contains one or more matching entries, the function
   is guaranteed to return the address of the first one.
 */
void *
dico_bsearch(void *key, const void *base, size_t nelem, size_t elsize,
	     int (*comp) (const void *, const void *, void *),
	     void *closure)
{
    char const *l = (char const*)base;
    char const *r = l + nelem * elsize;
    char const *s;
    void *found = NULL;
    
    while (l < r) {
	s = l + (((r - l) / elsize) >> 1) * elsize;
	int rc = comp(key, s, closure);
	if (rc > 0)
	    l = s + elsize;
	else {
	    if (rc == 0)
		found = (void*) s;
	    r = s;
	}
    }
    return found;
}
