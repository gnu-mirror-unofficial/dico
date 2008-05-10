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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <dico.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <size_max.h>

struct dico_stream {
    dico_linebuf_t inbuf, outbuf;
    int eof;
    int last_err;
    int (*read) (void *, char *, size_t, size_t *);
    int (*write) (void *, char *, size_t, size_t *);
    int (*close) (void *);
    const char *(*error_string) (void *, int);
    void *data;
};    

int
dico_stream_create(dico_stream_t *pstream,
		   void *data, 
		   int (*readfn) (void *, char *, size_t, size_t *),
		   int (*writefn) (void *, char *, size_t, size_t *),
		   int (*closefn) (void *))
{
    dico_stream_t stream = malloc(sizeof(*stream));
    if (stream == NULL)
	return ENOMEM;
    memset(stream, 0, sizeof(*stream));
    stream->read = readfn;
    stream->write = writefn;
    stream->close = closefn;
    stream->data = data;
    *pstream = stream;
    return 0;
}

void
dico_stream_set_error_string(dico_stream_t stream,
			     const char *(*error_string) (void *, int))
{
    stream->error_string = error_string;
}

const char *
dico_stream_strerror(dico_stream_t stream, int rc)
{
    if (stream->error_string)
	stream->error_string(stream->data, rc);
    else
	return strerror(rc);
}

int
dico_stream_last_error(dico_stream_t stream)
{
    return stream->last_err;
}

void
dico_stream_clearerr(dico_stream_t stream)
{
    stream->last_err = 0;
}

int
dico_stream_eof(dico_stream_t stream)
{
    return stream->eof;
}

int
dico_stream_set_buffer(dico_stream_t stream, enum dico_line_buffer_type type,
		       size_t size)
{
    dico_linebuf_t *pbuf;

    switch (type) {
    case lb_in:
	pbuf = &stream->inbuf;
	break;
    case lb_out:
	pbuf = &stream->outbuf;
	break;
    default:
	abort();
    }
    return dico_linebuf_create(pbuf, stream, type, size);
}

int
dico_stream_read_unbuffered(dico_stream_t stream, char *buf, size_t size,
			    size_t *pread)
{
    int rc;

    if (stream->eof) {
	if (pread)
	    *pread = 0;
	return 0;
    }
    if (stream->last_err)
	return stream->last_err;
    
    if (pread == NULL) {
	size_t rdbytes;

	while (size > 0
	       && (rc = stream->read(stream->data, buf, size, &rdbytes)) == 0) {
	    if (rdbytes == 0) {
		stream->eof = 1;
		break;
	    }
	    buf += rdbytes;
	    size -= rdbytes;
	}
    } else {
	rc = stream->read(stream->data, buf, size, pread);
	if (*pread == 0)
	    stream->eof = 1;
    }
    stream->last_err = rc;
    return rc;
}

int
dico_stream_write_unbuffered(dico_stream_t stream, char *buf, size_t size,
			     size_t *pwrite)
{
    int rc;
    
    if (pwrite == NULL) {
	size_t wrbytes;

	while (size > 0
	       && (rc = stream->write(stream->data, buf, size, &wrbytes))
	             == 0) {
	    if (wrbytes == 0) {
		rc = EIO;
		break;
	    }
	    buf += wrbytes;
	    size -= wrbytes;
	}
    } else
	rc = stream->write(stream->data, buf, size, pwrite);
    return rc;
}


int
dico_stream_read(dico_stream_t stream, char *buf, size_t size, size_t *pread)
{
    if (stream->inbuf) {
	int rc;
	size_t rdbytes;
	
	if (pread == NULL) {
	    size_t n = 0;
	    while (n < size) {
		rc = dico_stream_read_unbuffered(stream, buf, size, &rdbytes);
		if (rc)
		    break;
		if (rdbytes == 0)
		    break;
		dico_linebuf_grow(stream->inbuf, buf, rdbytes);
		n += rdbytes;
	    }
	    if (rc && n == 0)
		return rc;
	    
	    if (dico_linebuf_read(stream->inbuf, buf, size) != size)
		rc = EIO;
	    else
		rc = 0;
	} else {
	    if (dico_linebuf_level(stream->inbuf) == 0) {
		rc = dico_stream_read_unbuffered(stream, buf, size, &rdbytes);
		if (rc)
		    return rc;
		dico_linebuf_grow(stream->inbuf, buf, rdbytes);
	    }
	
	    *pread = dico_linebuf_read(stream->inbuf, buf, size);
	    rc = 0;
	}
	return rc;
    } else
	return dico_stream_read_unbuffered(stream, buf, size, pread);
}

int
dico_stream_readln(dico_stream_t stream, char *buf, size_t size, size_t *pread)
{
    int rc;
    char c;
    size_t n = 0;

    if (size == 0)
	return EIO;
    
    size--;
    for (n = 0; n < size && (rc = dico_stream_read(stream, &c, 1, NULL)) == 0;
	 n++) {
	*buf++ = c;
	if (c == '\n')
	    break;
    }
    *buf = 0;
    if (pread)
	*pread = n;
    return rc;
}

int
dico_stream_getline(dico_stream_t stream, char **pbuf, size_t *psize,
		    size_t *pread)
{
    int rc;
    char *lineptr = *pbuf;
    size_t n = *psize;
    size_t cur_len = 0;
    
    if (lineptr == NULL || n == 0) {
	char *new_lineptr;
	n = 120;
	new_lineptr = realloc(lineptr, n);
	if (new_lineptr == NULL) 
	    return ENOMEM;
	lineptr = new_lineptr;
    }
    
    for (;;) {
	char c;

	rc = dico_stream_read(stream, &c, 1, NULL);
	if (rc)
	    break;
	
	/* Make enough space for len+1 (for final NUL) bytes.  */
	if (cur_len + 1 >= n) {
	    size_t needed_max =
		SSIZE_MAX < SIZE_MAX ? (size_t) SSIZE_MAX + 1 : SIZE_MAX;
	    size_t needed = 2 * n + 1;   /* Be generous. */
	    char *new_lineptr;

	    if (needed_max < needed)
		needed = needed_max;
	    if (cur_len + 1 >= needed) {
		rc = EOVERFLOW;
		break;
	    }
	    
	    new_lineptr = realloc(lineptr, needed);
	    if (new_lineptr == NULL) {
		rc = ENOMEM;
		break;
	    }
	    
	    lineptr = new_lineptr;
	    n = needed;
	}

	lineptr[cur_len] = c;
	cur_len++;

	if (c == '\n')
	    break;
    }
    lineptr[cur_len] = '\0';
    
    *pbuf = lineptr;
    *psize = n;

    if (pread)
	*pread = cur_len;
    return rc;
}

int
dico_stream_write(dico_stream_t stream, char *buf, size_t size)
{
    if (stream->outbuf)
	return dico_linebuf_write(stream->outbuf, buf, size);
    return dico_stream_write_unbuffered(stream, buf, size, NULL);
}

int
dico_stream_writeln(dico_stream_t stream, char *buf, size_t size)
{
    int rc;
    if ((rc = dico_stream_write(stream, buf, size)) == 0)
	rc = dico_stream_write(stream, "\r\n", 2);
    return rc;
}

int
dico_stream_flush(dico_stream_t stream)
{
    int rc = 0;
    if (stream->outbuf)
	rc = dico_linebuf_flush(stream->outbuf);
    return rc;
}

int
dico_stream_close(dico_stream_t stream)
{
    int rc = 0;
    dico_stream_flush(stream);
    if (stream->close)
	rc = stream->close(stream->data);
    return rc;
}

void
dico_stream_destroy(dico_stream_t *stream)
{
    free(*stream);
    *stream = NULL;
}

