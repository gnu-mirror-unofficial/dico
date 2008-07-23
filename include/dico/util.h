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

#ifndef __dico_util_h
#define __dico_util_h

char *dico_full_file_name(const char *dir, const char *file);
size_t dico_trim_nl(char *buf);
size_t dico_trim_ws(char *buf);

char *xdico_sasl_mech_to_capa(char *mech);
int xdico_sasl_capa_match_p(const char *mech, const char *capa);

#endif
