/* This file is part of GNU Dico.
   Copyright (C) 1998-2017 Sergey Poznyakoff

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

#include <makedict.h>

FILE *
open_compressed(char *dictname, int *pflag)
{
    FILE *fp;
    char *command_string;
    int namelen;		/* length of filename, and flag */
    int extlen;

    if (access(dictname, R_OK) != 0) {
	return NULL;
    }
#ifdef UNCOMPRESS
    namelen = strlen(dictname);
    extlen = strlen(UNCOMPRESSEXT);
    if (strncmp(&dictname[namelen - extlen], UNCOMPRESSEXT, extlen) != 0) {
	namelen = 0;		/* flag for later on */
	fp = fopen(dictname, "r");
    } else {
	size_t command_len = sizeof(UNCOMPRESS) + 1 + namelen;
	command_string = xmalloc(command_len);
	strcpy(command_string, UNCOMPRESS);
	strcat(command_string, " ");
	strcat(command_string, dictname);
	fp = popen(command_string, "r");
	free(command_string);
    }
    *pflag = namelen;
#else
    *pflag = 0;
    fp = fopen(dictname, "r");
#endif				/* UNCOMPRESS */

    return fp;

}

