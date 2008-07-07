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

#include <dicod.h>

static void
dicod_xversion(dico_stream_t str, int argc, char **argv)
{
    stream_writez(str, "110 ");
    stream_writez(str, (char*)program_version);
    dico_stream_write(str, "\r\n", 2);
}
    
void
register_xversion()
{
    static struct dicod_command cmd = 
	{ "XVERSION", 1, NULL, "show implementation and version info",
	  dicod_xversion };
    dicod_capa_register("xversion", &cmd, NULL, NULL);
}

