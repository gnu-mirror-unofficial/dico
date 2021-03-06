/* This file is part of GNU Dico.
   Copyright (C) 1998-2021 Sergey Poznyakoff

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
#include <xdico.h>
#include <sysexits.h>

void
xalloc_die(void)
{
    dico_die(EX_OSERR, L_CRIT, 0, "Not enough memory");
    /* dico_die never returns.  The call below prevents the spurions 'noreturn'
       warning from gcc and serves as an extra safety measure. */
    abort();
}
