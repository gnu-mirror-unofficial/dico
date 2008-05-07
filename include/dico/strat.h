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

#ifndef __dico_strat_h
#define __dico_strat_h

#include <sys/types.h>
#include <dico/list.h>

typedef struct dico_strategy {
    char *name;
    char *descr;
} dico_strategy_t;

dico_strategy_t *dico_strategy_dup(const dico_strategy_t *strat);
const dico_strategy_t *dico_strategy_find(const char *name);
int dico_strategy_add(const dico_strategy_t *strat);
dico_iterator_t  dico_strategy_iterator(void);
void dico_strategy_iterate(dico_list_iterator_t itr, void *data);
size_t dico_strategy_count(void);

#endif
