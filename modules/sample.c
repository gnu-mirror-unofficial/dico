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


int
mod_init(int argc, char **argv)
{
    /* FIXME */
    return 1;
}
    
int
mod_close(dico_handle_t hp)
{
    /* FIXME */
    return 1;
}

dico_handle_t
mod_open(const char *dbname, int argc, char **argv)
{
    /* FIXME */
    return NULL;
}

char *
mod_info(dico_handle_t hp)
{
    /* FIXME */
    return NULL;
}

char *
mod_descr(dico_handle_t hp)
{
    /* FIXME */
    return NULL;
}

dico_result_t
mod_match(dico_handle_t hp, const dico_strategy_t strat, const char *word)
{
    /* FIXME */
    return NULL;
}

dico_result_t
mod_define(dico_handle_t hp, const char *word)
{
    /* FIXME */
    return NULL;
}

int
mod_output_result (dico_result_t rp, size_t n, dico_stream_t str)
{
    /* FIXME */
    return 1;
}

size_t
mod_result_count (dico_result_t rp)
{
    /* FIXME */
    return 0;
}

size_t
mod_compare_count (dico_result_t rp)
{
    /* FIXME */
    return 0;
}

void
mod_free_result(dico_result_t rp)
{
    /* FIXME */
}

struct dico_handler_module DICO_EXPORT(sample, module) = {
    DICO_MODULE_VERSION,
    mod_init,
    mod_open,
    mod_close,
    mod_info,
    mod_descr,
    mod_match,
    mod_define,
    mod_output_result,
    mod_result_count,
    mod_compare_count,
    mod_free_result
};
