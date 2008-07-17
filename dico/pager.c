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
#include <sys/ioctl.h>

dico_stream_t
create_output_stream()
{
    return dico_fd_stream_create(fileno(stdout), DICO_STREAM_WRITE, 1);
}

static int
get_screen_lines(void)
{
    struct winsize ws;

    ws.ws_col = ws.ws_row = 0;
    if ((ioctl(1, TIOCGWINSZ, (char *) &ws) < 0) || ws.ws_row == 0) {
	const char *lines = getenv("LINES");
	if (lines)
	    ws.ws_row = strtol(lines, NULL, 10);
    }
    return ws.ws_row;
}

struct pfile_stream {
    FILE *fp;
};

static int
fp_write(void *data, const char *buf, size_t size, size_t *pret)
{
    struct pfile_stream *p = data;
    *pret = fwrite(buf, 1, size, p->fp);
    return ferror(p->fp);
}

static int
fp_close(void *data)
{
    struct pfile_stream *p = data;
    pclose(p->fp);
    return 0;
}

static dico_stream_t
create_pfile_stream(FILE *fp)
{
    dico_stream_t str;
    struct pfile_stream *s;

    s = xmalloc(sizeof(*s));
    s->fp = fp;
    if (dico_stream_create(&str,  DICO_STREAM_WRITE, s))
	xalloc_die();
    dico_stream_set_write(str, fp_write);
    dico_stream_set_close(str, fp_close);
    return str;
}
    
dico_stream_t
create_pager_stream(size_t nlines)
{
    char *pager = getenv("PAGER");
    FILE *fp;

    if (!pager || !pager[0]
	|| !isatty(fileno(stdout)) || nlines < get_screen_lines())
	return create_output_stream();
    fp = popen(pager, "w");
    if (!fp)
	return create_output_stream();
    return create_pfile_stream(fp);
}