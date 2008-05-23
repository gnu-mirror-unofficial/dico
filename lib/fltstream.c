/* This file is part of Dico
   Copyright (C) 2003,2004,2007,2008 Sergey Poznyakoff
  
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <dico.h>
#include <string.h>

#define FILTER_BSIZE 2048

struct filter_stream {
    dico_stream_t transport;
    char buf[FILTER_BSIZE];
    size_t level;
    size_t min_level;
    size_t max_line_length;
    size_t line_length;
    filter_xcode_t xcode;
};

static int
filter_read(void *data, char *buf, size_t size, size_t *pret)
{
    struct filter_stream *fs = data;
    size_t rdsize;
    int rc;

    if (fs->level < fs->min_level) {
	rc = dico_stream_read(fs->transport, fs->buf + fs->level,
			      sizeof(fs->buf) - fs->level,
			      &rdsize);
	if (rc)
	    return rc;
	fs->level = rdsize;
    }
    
    if (fs->level) {
	rc = fs->xcode(fs->buf, fs->level, buf, size, &rdsize,
		       fs->max_line_length, &fs->line_length);
	if (rc)
	    return rc;
	memmove(fs->buf, fs->buf + rc, fs->level - rc);
	fs->level = rc;
	*pret = rdsize;
	rc = 0;
    } else {
	*pret = 0;
	rc = 0;
    }
    return rc;
}

static int
filter_write(void *data, char *buf, size_t size, size_t *pret)
{
    struct filter_stream *fs = data;
    size_t wrsize;
    int rc;
    
    if (fs->level == sizeof(fs->buf)) {
	rc = dico_stream_write(fs->transport, fs->buf, fs->level);
	if (rc)
	    return rc;
	fs->level = 0;
    }

    rc = fs->xcode(buf, size, fs->buf + fs->level,
		   sizeof(fs->buf) - fs->level, &wrsize,
		   fs->max_line_length, &fs->line_length);
    fs->level += wrsize;
    if (rc > size) 
	rc = size;
    *pret = rc;
    return 0;
}

static int
filter_wr_flush(void *data)
{
    struct filter_stream *fs = data;
    int rc = 0;
    
    if (fs->level)
	rc = dico_stream_write(fs->transport, fs->buf, fs->level);
    return rc;
}

dico_stream_t
filter_stream_create(dico_stream_t str,
		     size_t min_level,
		     size_t max_line_length,
		     filter_xcode_t xcode,
		     int mode)
{
    struct filter_stream *fs = malloc(sizeof(*fs));
    dico_stream_t stream;
    int rc;
    
    if (!fs)
	return NULL;

    rc = dico_stream_create(&stream,
			    mode == FILTER_ENCODE ?
			        DICO_STREAM_WRITE : DICO_STREAM_READ,
			    fs);
    if (rc) {
	free(fs);
	return NULL;
    }
    
    if (mode == FILTER_ENCODE) {
	dico_stream_set_write(stream, filter_write);
	dico_stream_set_flush(stream, filter_wr_flush);
    } else {
	dico_stream_set_read(stream, filter_read);
    }
    
    fs->transport = str;
    fs->level = 0;
    fs->min_level = min_level;
    fs->line_length = 0;
    fs->max_line_length = max_line_length;
    fs->xcode = xcode;

    return stream;
}

dico_stream_t
dico_codec_stream_create(const char *encoding, int mode,
			 dico_stream_t transport)
{
    dico_stream_t str = NULL;
    if (strcmp(encoding, "base64") == 0) 
	str = dico_base64_stream_create(transport, mode);
    else if (strcmp(encoding, "quoted-printable") == 0) 
	str = dico_qp_stream_create(transport, mode);
    return str;
}
