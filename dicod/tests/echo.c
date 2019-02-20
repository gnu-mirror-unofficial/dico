/* An "echo" database for GNU Dico test suite.
   Copyright (C) 2012-2019 Sergey Poznyakoff

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
#include <dico.h>
#include <string.h>

/* This module either returns query strings without any changes (echo mode),
   or denies any queries (null mode). */

enum echo_mode {
    ECHO_ECHO, /* Operate in echo mode */
    ECHO_NULL  /* Operate in null mode */
};

struct dico_handle_struct {
    enum echo_mode mode;
    char *prefix;
    size_t prefix_len;
};

static int
echo_init(int argc, char **argv)
{
    return 0;
}

static dico_handle_t
echo_init_db(const char *dbname, int argc, char **argv)
{
    int null_mode = 0;
    dico_handle_t hp;
    char *prefix = NULL;
    
    struct dico_option init_db_option[] = {
	{ DICO_OPTSTR(null), dico_opt_bool, &null_mode },
	{ DICO_OPTSTR(prefix), dico_opt_string, &prefix },
	{ NULL }
    };

    if (dico_parseopt(init_db_option, argc, argv, 0, NULL))
	return NULL;

    hp = malloc(sizeof(*hp));
    if (hp) {
	hp->mode = null_mode ? ECHO_NULL : ECHO_ECHO;
	if (prefix) {
	    hp->prefix = strdup(prefix);
	    if (!hp->prefix) {
		dico_log(L_ERR, 0, "not enough memory");
		free(hp);
		return NULL;
	    }
	    hp->prefix_len = strlen(prefix);
	} else {
	    hp->prefix = NULL;
	    hp->prefix_len = 0;
	}
    } else
	dico_log(L_ERR, 0, "not enough memory");
    return hp;
}

static int
echo_free_db(dico_handle_t hp)
{
    free(hp);
    return 0;
}

static int
echo_open(dico_handle_t dp)
{
    return 0;
}

static int
echo_close(dico_handle_t hp)
{
    return 0;
}

static char *
echo_info(dico_handle_t ep)
{
    static char *echo_info_str[2] = {
	"\
ECHO database.\n\n\
This database echoes each query.\n",
	"\
NULL database.\n\n\
This database returns NULL (no result) to any match and define\n\
requests.\n"
    };
    return strdup(echo_info_str[ep->mode]);
}

static char *
echo_descr(dico_handle_t ep)
{
    static char *echo_descr_str[2] = {
	"GNU Dico ECHO database",
	"GNU Dico NULL database"
    };
    char *res;
    if (ep->mode == ECHO_ECHO && ep->prefix) {
	size_t len = strlen(echo_descr_str[ep->mode]) + ep->prefix_len + 11;
	res = malloc(len);
	if (res)
	    snprintf(res, len, "%s (prefix %s)",
		     echo_descr_str[ep->mode], ep->prefix);
    } else
	res = strdup(echo_descr_str[ep->mode]);
    return res;
}

static dico_result_t
new_result(dico_handle_t ep, char const *word)
{
    char *res = malloc(strlen(word) + ep->prefix_len + 1);
    if (!res)
	dico_log(L_ERR, 0, "not enough memory");
    else {
	if (ep->prefix)
	    memcpy(res, ep->prefix, ep->prefix_len);
	strcpy(res + ep->prefix_len, word);
    }
    return (dico_result_t) res;
}

static dico_result_t
echo_match(dico_handle_t ep, const dico_strategy_t strat, const char *word)
{
    if (ep->mode == ECHO_NULL)
	return NULL;
    return new_result(ep, word);
}

static dico_result_t
echo_define(dico_handle_t ep, const char *word)
{
    if (ep->mode == ECHO_NULL)
	return NULL;
    return new_result(ep, word);
}

static int
echo_output_result(dico_result_t rp, size_t n, dico_stream_t str)
{
    char *word = (char*)rp;
    dico_stream_write(str, word, strlen(word));
    return 0;
}

static size_t
echo_result_count(dico_result_t rp)
{
    return 1;
}

static size_t
echo_compare_count(dico_result_t rp)
{
    return 1;
}

static void
echo_free_result(dico_result_t rp)
{
    free(rp);
}

static char *
echo_mime_header(dico_handle_t ep)
{
    return strdup("Content-Type: text/plain; charset=utf-8\n\
Content-Transfer-Encoding: 8bit\n");
}

struct dico_database_module DICO_EXPORT(echo, module) = {
    .dico_version = DICO_MODULE_VERSION,
    .dico_capabilities = DICO_CAPA_NONE,
    .dico_init = echo_init,
    .dico_init_db = echo_init_db,
    .dico_free_db = echo_free_db,
    .dico_open = echo_open,
    .dico_close = echo_close,
    .dico_db_info = echo_info,
    .dico_db_descr = echo_descr,
    .dico_match = echo_match,
    .dico_define = echo_define,
    .dico_output_result = echo_output_result,
    .dico_result_count = echo_result_count,
    .dico_compare_count = echo_compare_count,
    .dico_free_result = echo_free_result,
    .dico_db_mime_header = echo_mime_header
};
