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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <dico.h>
#include <string.h>

int
outline_init(int argc, char **argv)
{
    return 0;
}

dico_handle_t
outline_open(const char *db, int argc, char **argv)
{
    /* FIXME: */
    return db;
}

int
outline_close(dico_handle_t hp)
{
    /* FIXME */
    return 0;
}

static dico_strategy_t defstrat[] = {
    { "exact", "Match words exactly" },
    { "prefix", "Match word prefixes" }
};

int
outline_strats(dico_handle_t hp, dico_strategy_t **pstrat)
{
    *pstrat = defstrat;
    return ARRAY_SIZE(defstrat);
}

int
outline_match(dico_handle_t hp, dico_stream_t stream,
	      const char *strat, const char *word)
{
    /* FIXME */
    static char nomatch[] = "552 No match";
    dico_stream_writeln(stream, nomatch, strlen(nomatch));
    return 0;
}

int
outline_define(dico_handle_t hp, dico_stream_t stream, const char *word)
{
    /* FIXME */
    static char nomatch[] = "552 No match";
    dico_stream_writeln(stream, nomatch, strlen(nomatch));
    return 0;
}

struct dico_handler_module DICO_EXPORT(outline, module) = {
    outline_init,
    outline_open,
    outline_close,
    NULL,
    NULL,
    outline_strats,
    outline_match,
    outline_define
};
    
