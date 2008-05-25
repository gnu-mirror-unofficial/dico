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

#include "dictorg.h"

#define DE_UNKNOWN_FORMAT -1
#define DE_UNSUPPORTED_FORMAT -2

struct _dict_stream {
    int type;
    dico_stream_t transport;
    int transport_error;
    /* FIXME */
    size_t header_length;
};

static int
_dict_close(void *data)
{
    struct _dict_stream *str = data;
    return dico_stream_close(str->transport);
}

static int
_dict_destroy(void *data)
{
    struct _dict_stream *str = data;
    dico_stream_destroy(&str->transport);
    free(str);
    return 0;
}
    
static int
_dict_open(void *data, int flags)
{
    struct _dict_stream *str = data;
    unsigned char id[2];
    int rc;
    
    if (dico_stream_open(str->transport)) 
	return str->transport_error = dico_stream_last_error(str->transport);

    str->header_length = GZ_XLEN - 1;
    str->type = DICTORG_UNKNOWN;

    rc = dico_stream_read(str->transport, id, 2, NULL);
    if (rc) {
	dico_stream_close(str->transport);
	return str->transport_error = dico_stream_last_error(str->transport);
    }

    if (id[0] != GZ_MAGIC1 || id[1] != GZ_MAGIC2) {
	str->type = DICTORG_TEXT;
	return 0;
    }
    /* FIXME */
    return DE_UNKNOWN_FORMAT;
}

static const char *
_dict_strerror(void *data, int rc)
{
    struct _dict_stream *str = data;

    if (str->transport_error) {
	str->transport_error = 0;
	return dico_stream_strerror(str->transport, rc);
    }

    switch (rc) {
    case DE_UNKNOWN_FORMAT:
	return _("unknown dictionary format");

    case DE_UNSUPPORTED_FORMAT:
	return _("unsupported dictionary format");
	    
    default:
	return strerror(rc);
    }
}

static int
_dict_read_text(struct _dict_stream *str, char *buf, size_t size, size_t *pret)
{
    if (dico_stream_read(str->transport, buf, size, pret))
	return str->transport_error = dico_stream_last_error(str->transport);
    return 0;
}

static int
_dict_read(void *data, char *buf, size_t size, size_t *pret)
{
    struct _dict_stream *str = data;

    switch (str->type) {
    case DICTORG_UNKNOWN:
	return DE_UNKNOWN_FORMAT;
	
    case DICTORG_TEXT:
	return _dict_read_text(str, buf, size, pret);
	
    case DICTORG_GZIP:
    case DICTORG_DZIP:
	return DE_UNSUPPORTED_FORMAT;
    }
    return DE_UNSUPPORTED_FORMAT;
}

static int
_dict_seek_text(struct _dict_stream *str, off_t needle, int whence,
		off_t *presult)
{
    off_t off = dico_stream_seek(str->transport, needle, whence);
    if (off < 0) 
	return str->transport_error = dico_stream_last_error(str->transport);
    *presult = off;
    return 0;
}

static int
_dict_seek(void *data, off_t needle, int whence, off_t *presult)
{
    struct _dict_stream *str = data;
    switch (str->type) {
    case DICTORG_UNKNOWN:
	return DE_UNKNOWN_FORMAT;
	
    case DICTORG_TEXT:
	return _dict_seek_text(str, needle, whence, presult);
	
    case DICTORG_GZIP:
    case DICTORG_DZIP:
	return DE_UNSUPPORTED_FORMAT;
    }
    return DE_UNSUPPORTED_FORMAT;
}

dico_stream_t
dict_stream_create(const char *filename)
{
    int rc;
    dico_stream_t str;
    struct _dict_stream *s = malloc(sizeof(*s));

    if (!s)
	return NULL;
    rc = dico_stream_create(&str, DICO_STREAM_READ|DICO_STREAM_SEEK, s);
    if (rc) {
	free(s);
	return NULL;
    }

    memset(s, 0, sizeof(*s));
    s->type = DICTORG_UNKNOWN;
    s->transport = dico_mapfile_stream_create(filename,
					     DICO_STREAM_READ|DICO_STREAM_SEEK);
    dico_stream_set_open(str, _dict_open);
    dico_stream_set_read(str, _dict_read);
    dico_stream_set_seek(str, _dict_seek);
    dico_stream_set_close(str, _dict_close);
    dico_stream_set_destroy(str, _dict_destroy);
    dico_stream_set_error_string(str, _dict_strerror);
    
    return str;
}

