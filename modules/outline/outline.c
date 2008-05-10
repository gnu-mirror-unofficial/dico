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

static dico_strategy_t defstrat[] = {
    { "exact", "Match words exactly" },
    { "prefix", "Match word prefixes" }
};

int
outline_init(int argc, char **argv)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(defstrat); i++)
	dico_strategy_add(defstrat + i);
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

dico_result_t
outline_match(dico_handle_t hp, const char *strat, const char *word)
{
    /* FIXME */
    return NULL;
}

dico_result_t
outline_define(dico_handle_t hp, dico_stream_t stream, const char *word)
{
    /* FIXME */
    return 0;
}

int
outline_output_result (dico_result_t rp, size_t n, dico_stream_t str)
{
    /* FIXME */
    return 1;
}

size_t
outline_result_count (dico_result_t rp)
{
    return 0;
}

void
outline_free_result (dico_result_t rp)
{
    /* FIXME */
}

struct dico_handler_module DICO_EXPORT(outline, module) = {
    outline_init,
    outline_open,
    outline_close,
    NULL,
    NULL,
    outline_match,
    outline_define,
    outline_output_result,
    outline_result_count,
    outline_free_result
};
    
