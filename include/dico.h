/* This file is part of Dico.
   Copyright (C) 1998-2000, 2008 Sergey Poznyakoff

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

#ifndef __dico_h
#define __dico_h

#include <sys/types.h>
#include <stdlib.h>
#include <stdarg.h>

#include <dico/list.h>
#include <dico/assoc.h>
#include <dico/stream.h>
#include <dico/url.h>
#include <dico/xlat.h>

#ifndef offsetof
# define offsetof(s,f) ((size_t)&((s*)0)->f)
#endif
#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

#ifndef DICO_ARG_UNUSED
# define DICO_ARG_UNUSED __attribute__ ((__unused__))
#endif

#ifndef DICO_PRINTFLIKE
# define DICO_PRINTFLIKE(fmt,narg) __attribute__ ((__format__ (__printf__, fmt, narg)))
#endif

#endif
    
