/* This file is part of GNU Dico.
   Copyright (C) 2008 Sergey Poznyakoff

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

#include <dicod.h>

extern int option_mime;

off_t total_bytes_out;

#define OSTREAM_INITIALIZED       0x01
#define OSTREAM_DESTROY_TRANSPORT 0x02

struct ostream {
    dico_stream_t transport;
    int flags;
    dico_assoc_list_t headers;
};

static int
print_headers(struct ostream *ostr)
{
    int rc = 0;
    char *enc;
    
    if (ostr->headers) {
	dico_iterator_t itr;
	struct dico_assoc *p;
	
	itr = dico_assoc_iterator(ostr->headers);
	for (p = dico_iterator_first(itr); p; p = dico_iterator_next(itr)) {
	    dico_stream_write(ostr->transport, p->key, strlen(p->key));
	    dico_stream_write(ostr->transport, ": ", 2);
	    dico_stream_write(ostr->transport, p->value, strlen(p->value));
	    dico_stream_write(ostr->transport, "\r\n", 2);
	}
	dico_iterator_destroy(&itr);
    }
    
    rc = dico_stream_write(ostr->transport, "\r\n", 2);
    
    if (rc == 0
	&& (enc = dico_assoc_find(ostr->headers,
				  CONTENT_TRANSFER_ENCODING_HEADER))) {
	dico_stream_t str = dico_codec_stream_create(enc,
						     FILTER_ENCODE,
						     ostr->transport);
	if (str) {
	    ostr->transport = str;
	    ostr->flags |= OSTREAM_DESTROY_TRANSPORT;
	}    
    }
    return rc;
}


static int
ostream_write(void *data, const char *buf, size_t size, size_t *pret)
{
    struct ostream *ostr = data;
    off_t nbytes = dico_stream_bytes_out(ostr->transport);
    int rc;
    
    if (!(ostr->flags & OSTREAM_INITIALIZED)) {
	if (option_mime && print_headers(ostr))
	    return dico_stream_last_error(ostr->transport);
	ostr->flags |= OSTREAM_INITIALIZED;
    }
    if (buf[0] == '.' && dico_stream_write(ostr->transport, ".", 1))
	return dico_stream_last_error(ostr->transport);
    *pret = size;
    rc = dico_stream_write(ostr->transport, buf, size);
    if (rc == 0)
	total_bytes_out += dico_stream_bytes_out(ostr->transport) -
	                    nbytes;
    return rc;
}

static int
ostream_flush(void *data)
{
    struct ostream *ostr = data;
    return dico_stream_flush(ostr->transport);
}

static int
ostream_destroy(void *data)
{
    struct ostream *ostr = data;
    if (ostr->flags & OSTREAM_DESTROY_TRANSPORT)
	dico_stream_destroy(&ostr->transport);
    free(data);
    return 0;
}

dico_stream_t
dicod_ostream_create(dico_stream_t str, dico_assoc_list_t headers)
{
    struct ostream *ostr = xmalloc(sizeof(*ostr));
    dico_stream_t stream;

    int rc = dico_stream_create(&stream, DICO_STREAM_WRITE, ostr);
    if (rc)
	xalloc_die();
    ostr->transport = str;
    ostr->flags = 0;
    ostr->headers = headers;
    dico_stream_set_write(stream, ostream_write);
    dico_stream_set_flush(stream, ostream_flush);
    dico_stream_set_destroy(stream, ostream_destroy);
    dico_stream_set_buffer(stream, dico_buffer_line, 1024);
    return stream;
}

    
