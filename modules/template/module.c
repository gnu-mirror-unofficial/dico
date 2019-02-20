/* This file is part of GNU Dico.
   Copyright (C) 2008-2019 Sergey Poznyakoff

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
#include <dico.h>

static int
<MODNAME>_init(int argc, char **argv)
{
    /* FIXME */
    return 1;
}
    
static dico_handle_t
<MODNAME>_init_db(const char *dbname, int argc, char **argv)
{
    /* FIXME */
    return NULL;
}

static int
<MODNAME>_free_db(dico_handle_t hp)
{
    /* FIXME */
    return 1;
}

static int
<MODNAME>_open(dico_handle_t dp)
{
    /* FIXME */
    return 1;
}

static int
<MODNAME>_close(dico_handle_t hp)
{
    /* FIXME */
    return 1;
}

static char *
<MODNAME>_info(dico_handle_t hp)
{
    /* FIXME */
    return NULL;
}

static char *
<MODNAME>_descr(dico_handle_t hp)
{
    /* FIXME */
    return NULL;
}

int
<MODNAME>_lang(dico_handle_t hp, dico_list_t list[2])
{
    /* FIXME */
    return 1;
}

static dico_result_t
<MODNAME>_match(dico_handle_t hp, const dico_strategy_t strat, const char *word)
{
    /* FIXME */
    return NULL;
}

static dico_result_t
<MODNAME>_define(dico_handle_t hp, const char *word)
{
    /* FIXME */
    return NULL;
}

static int
<MODNAME>_output_result (dico_result_t rp, size_t n, dico_stream_t str)
{
    /* FIXME */
    return 1;
}

static size_t
<MODNAME>_result_count (dico_result_t rp)
{
    /* FIXME */
    return 0;
}

static size_t
<MODNAME>_compare_count (dico_result_t rp)
{
    /* FIXME */
    return 0;
}

static void
<MODNAME>_free_result(dico_result_t rp)
{
    /* FIXME */
}

static int
<MODNAME>_result_headers(dico_result_t rp, dico_assoc_list_t hdr)
{  
    /* FIXME */
    return 0;
}

static int
<MODNAME>_run_test(int argc, char **argv)
{
    /* FIXME */
    return 1;
}

char *
<MODNAME>_db_mime_header(dico_handle_t hp)
{
    /* FIXME */
    return NULL;
}

struct dico_database_module DICO_EXPORT(<MODNAME>, module) = {
    .dico_version        =  DICO_MODULE_VERSION,
    .dico_capabilities   =  DICO_CAPA_NONE,
    .dico_init           =  <MODNAME>_init,
    .dico_init_db        =  <MODNAME>_init_db,
    .dico_free_db        =  <MODNAME>_free_db,
    .dico_open           =  <MODNAME>_open,
    .dico_close          =  <MODNAME>_close,
    .dico_db_info        =  <MODNAME>_info,
    .dico_db_descr       =  <MODNAME>_descr,
    .dico_db_lang        =  <MODNAME>_lang,
    .dico_match          =  <MODNAME>_match,
    .dico_define         =  <MODNAME>_define,
    .dico_output_result  =  <MODNAME>_output_result,
    .dico_result_count   =  <MODNAME>_result_count,
    .dico_compare_count  =  <MODNAME>_compare_count,
    .dico_free_result    =  <MODNAME>_free_result,
    .dico_result_headers =  <MODNAME>_result_headers,
    .dico_run_test       =  <MODNAME>_run_test,
    .dico_db_mime_header =  <MODNAME>_db_mime_header
};
