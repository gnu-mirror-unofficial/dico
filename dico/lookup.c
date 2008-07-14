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

static void
print_definitions(struct dict_connection *conn)
{
    unsigned long i, count;
    char *p, *base;
    size_t nlines = 0;
    dico_stream_t str;
    
    count = strtoul (conn->buf + 3, &p, 10);
    for (i = 0; i < count; i++) {
	size_t n;
	dict_read_reply(conn);
	if (!dict_status_p(conn, "151")) {
	    dico_log(L_WARN, 0,
		     _("Unexpected reply in place of definition %lu"), i);
	    break;
	}
	dict_multiline_reply(conn, &n);
	nlines += n;
    }

    str = create_pager_stream(nlines);
    base = p = obstack_finish(&conn->stk);
    for (i = 0; i < count; i++) {
	size_t len = strlen(p);
	dico_stream_write(str, p, len);
	p += len + 1;
    }
    obstack_free(&conn->stk, base);
    dico_stream_close(str);
    dico_stream_destroy(&str);
}

static void
print_multiline(struct dict_connection *conn)
{
    size_t nlines;
    dico_stream_t str;
    char *p;
    
    dict_multiline_reply(conn, &nlines);
    str = create_pager_stream(nlines);
    p = obstack_finish(&conn->stk);

    dico_stream_write(str, p, strlen(p));

    obstack_free(&conn->stk, p);
    dico_stream_close(str);
    dico_stream_destroy(&str);
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

    switch (url->req.type) {
    case DICO_REQUEST_DEFINE:
	stream_printf(conn->str, "DEFINE \"%s\" \"%s\"\r\n",
		      quotearg_n (0, url->req.database),
		      quotearg_n (1, url->req.word));
	dict_read_reply(conn);
	if (dict_status_p(conn, "150")) 
	    print_definitions(conn);
	else
	    print_reply(conn);
	break;
	
    case DICO_REQUEST_MATCH:
	if (levenshtein_threshold && dict_capa(conn, "xlev")) {
	    stream_printf(conn->str, "XLEV %u\n", levenshtein_threshold);
	    dict_read_reply(conn);
	    if (!dict_status_p(conn, "250")) {
		dico_log(L_WARN, 0, _("Server rejected XLEV command"));
		print_reply(conn);
	    }
	}
	stream_printf(conn->str, "MATCH \"%s\" \"%s\" \"%s\"\r\n",
		      quotearg_n (0, url->req.database),
		      quotearg_n (1, url->req.strategy),
		      quotearg_n (2, url->req.word));
	dict_read_reply(conn);
	if (dict_status_p(conn, "152")) 
	    print_multiline(conn);
	else
	    print_reply(conn);
	break;
	
    default:
	dico_log(L_CRIT, 0,
		 _("%s:%d: INTERNAL ERROR: unexpected request type"),
		 __FILE__, __LINE__);
    }
    
    stream_printf(conn->str, "QUIT\r\n");
    dict_read_reply(conn);
    /* FIXME */
    return 0;
}

int
dict_lookup(char *word)
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
dict_single_command(char *cmd, char *arg, char *code)
{
    struct dict_connection *conn;
    
    if (!dico_url.host) {
	dico_log(L_ERR, 0, _("Server name or IP not specified"));
	return 1;
    }

    if (dict_connect(&conn, &dico_url))
	return 1;

    if (arg)
	stream_printf(conn->str, "%s \"%s\"\r\n", cmd, arg);
    else
	stream_printf(conn->str, "%s\r\n", cmd);
    
    dict_read_reply(conn);
    if (!dict_status_p(conn, code))
	print_reply(conn);
    else
	print_multiline(conn);
    return 0;
}
