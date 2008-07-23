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

#include <dicod.h>

static int levenshtein_distance = 1;

static int
lev_sel(int cmd, const char *word, const char *dict_word, void *closure)
{
    if (cmd == DICO_SELECT_RUN) {
	int dist = dico_levenshtein_distance(word, dict_word, closure != NULL);
	if (dist < 0)
	    return 0;
	return dist <= levenshtein_distance;
    }
    return 0;
}

static struct dico_strategy levstrat = {
    "lev",
    "Match headwords within given Levenshtein distance",
    lev_sel,
    NULL
};

static struct dico_strategy dlevstrat = {
    "dlev",
    "Match headwords within given Damerau-Levenshtein distance",
    lev_sel,
    (void*)1
};

static void
dicod_xlevdist(dico_stream_t str, int argc, char **argv)
{
    if (c_strcasecmp(argv[1], "tell") == 0) 
	stream_printf(str, "280 %d\r\n", levenshtein_distance);
    else if (isdigit(argv[1][0]) && argv[1][0] != '0' && argv[1][1] == 0) {
	levenshtein_distance = atoi(argv[1]);
	stream_printf(str, "250 ok - Levenshtein threshold set to %d\r\n",
		      levenshtein_distance);
    } else
	stream_writez(str, "500 invalid argument\r\n");
}
	
void
register_lev()
{
    static struct dicod_command cmd =
	{ "XLEV", 2, 2, "distance", "Set Levenshtein distance",
	  dicod_xlevdist };
    dico_strategy_add(&levstrat);
    dico_strategy_add(&dlevstrat);
    dico_set_default_strategy("lev");
    dicod_capa_register("xlev", &cmd, NULL, NULL);
}

	  
