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

off_t total_bytes_out;

#define OSTREAM_INITIALIZED       0x01
#define OSTREAM_DESTROY_TRANSPORT 0x02

struct ostream {
    dico_stream_t transport;
    int flags;
    const char *type;
    const char *encoding;
};

#define CONTENT_TYPE_HEADER "Content-type: "
#define CONTENT_TRANSFER_ENCODING_HEADER "Content-transfer-encoding: "

static int
print_headers(struct ostream *ostr)
{
    int rc;

    if (ostr->type) {
	dico_stream_write(ostr->transport, CONTENT_TYPE_HEADER,
			  sizeof(CONTENT_TYPE_HEADER) - 1);
	dico_stream_write(ostr->transport, (char *) ostr->type,
			  strlen(ostr->type));
	dico_stream_write(ostr->transport, "\r\n", 2);
    }

    if (ostr->encoding) {
	dico_stream_write(ostr->transport, CONTENT_TRANSFER_ENCODING_HEADER,
			  sizeof(CONTENT_TRANSFER_ENCODING_HEADER) - 1);
	dico_stream_write(ostr->transport, (char*) ostr->encoding,
			  strlen(ostr->encoding));
	dico_stream_write(ostr->transport, "\r\n", 2);
    }

    rc = dico_stream_write(ostr->transport, "\r\n", 2);
    if (rc == 0 && ostr->encoding) {
	dico_stream_t str = dico_codec_stream_create(ostr->encoding,
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
dictd_ostream_create(dico_stream_t str, const char *type, const char *enc)
{
    struct ostream *ostr = xmalloc(sizeof(*ostr));
    dico_stream_t stream;

    int rc = dico_stream_create(&stream, DICO_STREAM_WRITE, ostr);
    if (rc)
	xalloc_die();
    ostr->transport = str;
    ostr->flags = 0;
    ostr->type = type;
    ostr->encoding = enc;
    dico_stream_set_write(stream, ostream_write);
    dico_stream_set_flush(stream, ostream_flush);
    dico_stream_set_destroy(stream, ostream_destroy);
    dico_stream_set_buffer(stream, dico_buffer_line, 1024);
    return stream;
}

    
