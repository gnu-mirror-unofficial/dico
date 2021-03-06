/* This file is part of GNU Dico.
   Copyright (C) 2008-2021 Sergey Poznyakoff

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

#ifndef __dico_parseopt_h
#define __dico_parseopt_h

#include <dico/types.h>

enum dico_opt_type {
    dico_opt_null,
    dico_opt_bool,
    dico_opt_bitmask,
    dico_opt_bitmask_rev,
    dico_opt_long,
    dico_opt_string,
    dico_opt_enum,
    dico_opt_const,
    dico_opt_const_string
};

struct dico_option {
    const char *name;
    size_t len;
    enum dico_opt_type type;
    void *data;
    union {
	long value;
	const char **enumstr;
    } v;
    int (*func) (struct dico_option *, const char *);
};

#define DICO_OPTSTR(s) #s, (sizeof(#s) - 1)

#define DICO_PARSEOPT_PARSE_ARGV0 0x01
#define DICO_PARSEOPT_PERMUTE     0x02

int dico_parseopt(struct dico_option *opt, int argc, char **argv,
		  int flags, int *index);

#endif
