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

const char *xscript_prefix[] = { "S:", "C:" };

static int
parse_initial_reply(struct dict_connection *conn)
{
    char *p;
    size_t n = 0;
    size_t len;
    
    if (!dict_status_p(conn, "220"))
	return 1;
    p = strchr(conn->buf, '<');
    if (!p)
	return 1;
    p++;
    
    while ((len = strcspn(p, ".>"))) {
	char *s;
	if (conn->capac == n) {
	    if (n == 0)
		n = 2;
	    conn->capav = x2nrealloc(conn->capav, &n, sizeof(conn->capav[0]));
	}

	s = xmalloc(len+1);
	memcpy(s, p, len);
	s[len] = 0;
	conn->capav[conn->capac++] = s;
	p += len + 1;
	if (p[-1] == '>')
	    break;
    }

    p = strchr(p, '<');
    if (!p)
	return 1;
    len = strcspn(p, ">");
    if (p[len] != '>')
	return 1;
    conn->msgid = xmalloc(len + 1);
    memcpy(conn->msgid, p, len);
    conn->msgid[len] = 0;
    
    return 0;
}

int
dict_connect(struct dict_connection **pconn, dico_url_t url)
{
    struct sockaddr_in s;
    int fd;
    IPADDR ip;
    dico_stream_t str;
    struct dict_connection *conn;
    
    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
	dico_log(L_ERR, errno,
		 _("cannot create dict socket"));
	return 1;
    }

    s.sin_family = AF_INET;
    s.sin_addr.s_addr = htonl(source_addr);
    s.sin_port = 0;
    if (bind(fd, (struct sockaddr*) &s, sizeof(s)) < 0) {
	dico_log(L_ERR, errno,
		 _("cannot bind AUTH socket"));
    }

    ip = get_ipaddr(url->host);
    if (ip == 0) {
	dico_log(L_ERR, 0, _("%s: Invalid IP or unknown host name"),
		 url->host);
	return 1;
    }
    s.sin_addr.s_addr = htonl(ip);
    s.sin_port = htons(url->port ? url->port : DICO_DICT_PORT);
    if (connect(fd, (struct sockaddr*) &s, sizeof(s)) == -1) {
	dico_log(L_ERR, errno,
		 _("cannot connect to DICT server %s:%d"),
		 url->host, url->port ? url->port : DICO_DICT_PORT);
	close(fd);
	return 1;
    }

    if ((str = dico_fd_io_stream_create(fd, fd)) == NULL) {
	dico_log(L_ERR, errno,
		 _("cannot create dict stream: %s"),
		 strerror(errno));
	return 1;
    }

    if (transcript) {
	dico_stream_t logstr = dico_log_stream_create(L_DEBUG);
	if (!logstr)
	    xalloc_die();
	str = xdico_transcript_stream_create(str, logstr, xscript_prefix);
    }

    conn = xzalloc(sizeof(*conn));
    conn->str = str;
    
    if (dict_read_reply(conn)) {
	dico_log(L_ERR, 0, _("No reply from server"));
	return 1;
    }
    if (parse_initial_reply(conn)) {
	dico_log(L_ERR, 0, _("Invalid reply from server"));
	dict_conn_close(conn);
	return 1;
    }
    
    stream_printf(conn->str, "CLIENT \"%s\"\r\n", client);
    dict_read_reply(conn);
    if (!dict_status_p(conn, "250")) 
	dico_log(L_WARN, 0,
		 _("Unexpected reply to CLIENT command: `%s'"),
		 conn->buf);
	
    *pconn = conn;
    
    return 0;
}

int
dict_read_reply(struct dict_connection *conn)
{
    int rc;
    if (conn->buf)
	conn->buf[0] = 0;
    rc = dico_stream_getline(conn->str, &conn->buf, &conn->size,
			     &conn->level);
    if (rc == 0)
	conn->level = dico_trim_nl(conn->buf);
    return rc;
}

int
dict_status_p(struct dict_connection *conn, char *status)
{
    return conn->level > 3
	&& memcmp(conn->buf, status, 3) == 0
	&& (isspace(conn->buf[3])
	    || (conn->level == 5 && memcmp(conn->buf+3,"\r\n",2) == 0));
}

int
dict_capa(struct dict_connection *conn, char *capa)
{
    int i;
    
    for (i = 0; i < conn->capac; i++)
	if (strcmp(conn->capav[i], capa) == 0)
	    return 1;
    return 0;
}

int
dict_multiline_reply(struct dict_connection *conn, size_t *pnlines)
{
    int rc;
    size_t nlines = 0;
    
    if (!conn->stk_init) {
	obstack_init(&conn->stk);
	conn->stk_init = 1;
    }

    obstack_grow(&conn->stk, conn->buf, conn->level);
    obstack_1grow(&conn->stk, '\n');
    nlines++;
    
    while ((rc = dict_read_reply(conn)) == 0) {
	char *ptr = conn->buf;
	size_t len = conn->level;
	if (*ptr == '.') {
	    if (ptr[1] == 0)
		break;
	    else if (ptr[1] == '.') {
		ptr++;
		len--;
	    }
	}
	obstack_grow(&conn->stk, ptr, len);
	obstack_1grow(&conn->stk, '\n');
	nlines++;
    }
    obstack_1grow(&conn->stk, 0);
    if (pnlines)
	*pnlines = nlines;
    return rc;
}

void
dict_conn_close(struct dict_connection *conn)
{
    dico_stream_close(conn->str);
    dico_stream_destroy(&conn->str);
    free(conn->msgid);
    free(conn->buf);
    dico_argcv_free(conn->capac, conn->capav);
    if (conn->stk_init)
	obstack_free(&conn->stk, NULL);
    free(conn);
}

