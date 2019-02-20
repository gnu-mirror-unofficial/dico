/* This file is part of GNU Dico.
   Copyright (C) 2016-2019 Sergey Poznyakoff

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
#include <stdio.h>
#include <dico.h>

int
main(int argc, char **argv)
{
    dico_set_program_name(argv[0]);

    if (argc == 1) {
	fprintf(stderr, "Usage: %s word [word...]\n", dico_program_name);
	return 2;
    }
    
    while (--argc) {
	char code[DICO_SOUNDEX_SIZE];
	char *arg = *++argv;
	    
	dico_soundex(arg, code);
	printf("%s: %s\n", arg, code);
    }

    return 0;
}
