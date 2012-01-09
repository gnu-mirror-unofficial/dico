/* This file is part of GNU Dico.
   Copyright (C) 2008, 2010, 2012 Sergey Poznyakoff

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <unistd.h>
#include <getopt.h>
#include <dico.h>

int
main(int argc, char **argv)
{
    int c;
    int flags = 0;
    
    while ((c = getopt(argc, argv, "dhn")) != EOF) {
	switch (c) {
	case 'd':
	    flags |= DICO_LEV_DAMERAU;
	    break;

	case 'n':
	    flags |= DICO_LEV_NORM;
	    break;

	case 'h':
	    printf("Usage: %s [-dn] word word\n", argv[0]);
	    return 0;

	default:
	    return 1;
	}
    }
    argc -= optind;
    argv += optind;
    if (argc != 2) {
	fprintf(stderr, "Usage: %s [-dn] word word\n", argv[0]);
	return 1;
    }
    printf("%d\n", dico_levenshtein_distance(argv[0], argv[1], flags));
    return 0;
}
