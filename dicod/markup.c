/* This file is part of GNU Dico.
   Copyright (C) 2008 Sergey Poznyakoff

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

#include <dicod.h>

static void
dicod_markup(dico_stream_t str, int argc, char **argv)
{
    char *p;
    if (argc == 2) {
	/* Report current markup type */
	stream_printf(str, "280 %s is current markup type\r\n",
		      dico_markup_type);
    } else if ((p = dico_markup_lookup(argv[2]))) {
	dico_markup_type = p;
	stream_printf(str, "250 markup type set to %s\r\n", dico_markup_type);
    } else
	stream_writez(str, "500 invalid argument\r\n");
}

void
register_markup()
{
    static struct dicod_command cmd =
	{ "OPTION MARKUP", 2, 3, "type", "Set output markup type",
	  dicod_markup };
    dicod_capa_register("markup", &cmd, NULL, NULL);
    if (dico_markup_register("none"))
	xalloc_die();
}
