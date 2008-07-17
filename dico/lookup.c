/* This file is part of Dico. 
   Copyright (C) 1998-2000, 2008 Sergey Poznyakoff

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

#include "dico-priv.h"

static void
print_reply(struct dict_connection *conn)
{
    printf("%s\n", conn->buf);
}

static size_t
utf8_count_newlines(char *str)
{
    size_t count = 0;
    struct utf8_iterator itr;
    for (utf8_iter_first(&itr, (unsigned char *)str);
	 !utf8_iter_end_p(&itr); utf8_iter_next(&itr)) {
	if (utf8_iter_isascii(itr) && *itr.curptr == '\n')
	    count++;
    }
    return count;
}

static size_t
result_count_lines(struct dict_result *res)
{
    size_t i, count = 0;
    
    switch (res->type) {
    case dict_result_define:
	for (i = 0; i < res->count; i++)
	    count += res->set.def[i].nlines + 3;
	break;

    case dict_result_match:
	count = res->count;
	break;

    case dict_result_text:
	count = utf8_count_newlines(res->base);
    }
    return count;
}

static void
format_defn(dico_stream_t str, struct define_result *def)
{
    stream_printf(str, _("From %s, %s:\n"),
		  def->database, def->descr);
    stream_writez(str, def->defn);
    dico_stream_write(str, "\n", 1);
}

static void
format_match(dico_stream_t str, struct match_result *mat)
{
    stream_printf(str, "%s \"%s\"\n", mat->database, mat->word);
}

void
print_result(struct dict_result *res)
{
    unsigned long i;
    dico_stream_t str;
    
    str = create_pager_stream(result_count_lines(res));
    switch (res->type) {
    case dict_result_define:
	for (i = 0; i < res->count; i++)
	    format_defn(str, &res->set.def[i]);
	break;

    case dict_result_match:
	for (i = 0; i < res->count; i++)
	    format_match(str, &res->set.mat[i]);
	break;

    case dict_result_text:
	stream_writez(str, res->base);
	break;
    }
    dico_stream_close(str);
    dico_stream_destroy(&str);
}

int
dict_lookup(struct dict_connection *conn, dico_url_t url)
{
    int rc;
    switch (url->req.type) {
    case DICO_REQUEST_DEFINE:
	rc = dict_define(conn,
			 quotearg_n (0, url->req.database),
			 quotearg_n (1, url->req.word));
	break;
	
    case DICO_REQUEST_MATCH:
	rc = dict_match(conn,
			quotearg_n (0, url->req.database),
			quotearg_n (1, url->req.strategy),
			quotearg_n (2, url->req.word));
	break;
	
    default:
	dico_log(L_CRIT, 0,
		 _("%s:%d: INTERNAL ERROR: unexpected request type"),
		 __FILE__, __LINE__);
    }

    if (rc == 0) {
	struct dict_result *res = dict_conn_last_result(conn);
	print_result(res);
	dict_result_free(res);
    } else
	print_reply(conn);
    
    return 0;
}

int
dict_lookup_url(dico_url_t url)
{
    struct dict_connection *conn;
    
    if (!url->host) {
	dico_log(L_ERR, 0, _("Server name or IP not specified"));
	return 1;
    }

    if (dict_connect(&conn, url))
	return 1;

    dict_lookup(conn, url);
    
    stream_printf(conn->str, "QUIT\r\n");
    dict_read_reply(conn);
    dict_conn_close(conn);
    /* FIXME */
    return 0;
}

int
dict_word(char *word)
{
    int rc;
    dico_url_t url;

    if (memcmp (word, "dict://", 7) == 0) {
	if (dico_url_parse(&url, word))
	    return 1;
	rc = dict_lookup_url(url);
	dico_url_destroy(&url);
    } else {
	dico_url.req.word = word;
	rc = dict_lookup_url(&dico_url);
    }
	
    return rc;
}

int
dict_run_single_command(struct dict_connection *conn,
			char *cmd, char *arg, char *code)
{
    if (arg)
	stream_printf(conn->str, "%s \"%s\"\r\n", cmd, arg);
    else
	stream_printf(conn->str, "%s\r\n", cmd);
    
    dict_read_reply(conn);
    if (!dict_status_p(conn, code))
	print_reply(conn);
    else {
	struct dict_result *res;
	
	dict_multiline_reply(conn);
	dict_read_reply(conn);
	dict_result_create(conn, dict_result_text, 1,
			   obstack_finish(&conn->stk));
	res = dict_conn_last_result(conn);
	print_result(res);
	dict_result_free(res);
    }
}

int
dict_single_command(char *cmd, char *arg, char *code)
{
    struct dict_connection *conn;
    
    if (!dico_url.host) {
	dico_log(L_ERR, 0, _("Server name or IP not specified"));
	return 1;
    }

    if (dict_connect(&conn, &dico_url))
	return 1;

    dict_run_single_command(conn, cmd, arg, code);
    dict_conn_close(conn);    
    return 0;
}
