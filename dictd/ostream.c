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

#include <dictd.h>

extern int option_mime;

struct ostream {
    dico_stream_t transport;
    int inited;
};

static int
ostream_write(void *data, char *buf, size_t size, size_t *pret)
{
    struct ostream *ostr = data;
    if (!ostr->inited) {
	if (option_mime && dico_stream_write(ostr->transport, "\r\n", 2))
	    return 1;
	ostr->inited = 1;
    }
    if (buf[0] == '.' && dico_stream_write(ostr->transport, ".", 1))
	return 1;
    *pret = size;
    return dico_stream_write(ostr->transport, buf, size);
}

dico_stream_t
dictd_ostream_create(dico_stream_t str)
{
    struct ostream *ostr = xmalloc(sizeof(*ostr));
    dico_stream_t stream;

    int rc = dico_stream_create(&stream, ostr, NULL, ostream_write, NULL);
    if (rc)
	xalloc_die();
    ostr->transport = str;
    ostr->inited = 0;
    dico_stream_set_buffer(stream, lb_out, 1024);
    return stream;
}
