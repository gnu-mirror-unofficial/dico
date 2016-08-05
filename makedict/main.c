/* This file is part of GNU Dico.
   Copyright (C) 1998-2016 Sergey Poznyakoff

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

char *dictdir = ".";
char *kanjidict = "kanjidic.gz";
char *edict = "edict.gz";
char *outdir = "jdict";
char *dictname = DICT_DB;
int verbose;


char *
mkfullname(char *dir, char *name)
{
    int dirlen = strlen(dir);
    int namelen = strlen(name);
    char *rp;

    if (name[0] == '/')
	return name;
    rp = xmalloc(dirlen + namelen + (dir[dirlen-1] != '/') + 1);
    strcpy(rp, dir);
    if (dir[dirlen-1] != '/')
	rp[dirlen++] = '/';
    strcpy(rp + dirlen, name);
    return rp;
}

void
make_names()
{
    int i;
    kanjidict = mkfullname(dictdir, kanjidict);
    edict = mkfullname(dictdir, edict);
    dictname = mkfullname(outdir, dictname);
    for (i = 0; dbidx[i].name; i++)
	dbidx[i].name = mkfullname(outdir, dbidx[i].name);
}

int
main(int argc, char **argv)
{
    dico_set_program_name(argv[0]);

    if (argc == 1) {
	print_help();
	exit(0);
    }
    get_options(argc, argv);
    make_names();
    return compile();
}


