/* This file is part of Dico.
   Copyright (C) 2003, 2007, 2008 Sergey Poznyakoff

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

struct dico_line_buffer {
    enum dico_line_buffer_type type;
    dico_stream_t stream;
    char *buffer;        /* Line buffer */
    size_t size;         /* Allocated size */
    size_t level;        /* Current filling level */
};


int
dico_linebuf_create(struct dico_line_buffer **pbuf,
		    dico_stream_t stream, enum dico_line_buffer_type type,
		    size_t size)
{
    struct dico_line_buffer *buf;

    buf = malloc(sizeof(*buf));
    if (!buf)
	return ENOMEM;
    buf->buffer = malloc(size);
    if (!buf->buffer) {
	free(buf);
	return ENOMEM;
    }
    buf->stream = stream;
    buf->type = type;
    buf->size = size;
    buf->level = 0;
    *pbuf = buf;
    return 0;
}

void
dico_linebuf_destroy(struct dico_line_buffer **s)
{
    if (s && *s) {
	free((*s)->buffer);
	free(*s);
	*s = NULL;
    }
}

void
dico_linebuf_drop(struct dico_line_buffer *s)
{
    s->level = 0;
}

int
dico_linebuf_grow(struct dico_line_buffer *s, const char *ptr, size_t size)
{
    if (!s->buffer) {
	s->buffer = malloc(size);
	s->size = size;
	s->level = 0;
    } else if (s->size - s->level < size) {
	size_t newsize = s->size + size;
	s->buffer = realloc(s->buffer, newsize);
	if (s->buffer)
	    s->size = newsize;
    }
    
    if (!s->buffer)
	return ENOMEM;
  
    memcpy(s->buffer + s->level, ptr, size);
    s->level += size;
    return 0;
}

size_t
dico_linebuf_read(struct dico_line_buffer *s, char *optr, size_t osize)
{
    int len;
    
    len = s->level > osize ? osize : s->level;
    memcpy(optr, s->buffer, len);
    if (s->level > len) {
	memmove(s->buffer, s->buffer + len, s->level - len);
	s->level -= len;
    } else if (s->level == len)
	s->level = 0;
    
    return len;
}

int
dico_linebuf_readline(struct dico_line_buffer *s, char *ptr, size_t size)
{
    char *p = memchr(s->buffer, '\n', s->level);

    if (p)
	size = p - s->buffer + 1;
    return dico_linebuf_read(s, ptr, size);
}

int
dico_linebuf_writelines(struct dico_line_buffer *s)
{
    if (s->level >= 2) {
	char *start, *end;
      
	for (start = s->buffer,
		 end = memchr(start, '\n', s->buffer + s->level - start);
	     end && end < s->buffer + s->level;
	     start = end + 1,
		 end = memchr(start, '\n', s->buffer + s->level - start)) {
		int rc = dico_stream_write_unbuffered(s->stream, start,
						      end - start + 1, NULL);
		if (rc)
		    return rc;
	    }

	if (start > s->buffer) {
	    if (start < s->buffer + s->level) {
		int rest = s->buffer + s->level - start;
		memmove(s->buffer, start, rest);
		s->level = rest;
	    } else 
		s->level = 0;
	}
    }
    return 0;
}

size_t
dico_linebuf_level(struct dico_line_buffer *s)
{
    return s->level;
}

char *
dico_linebuf_data(struct dico_line_buffer *s)
{
    return s->buffer;
}

int
dico_linebuf_write(struct dico_line_buffer *s, char *ptr, size_t size)
{
    int rc = dico_linebuf_grow(s, ptr, size);
    if (rc == 0)
	rc = dico_linebuf_writelines(s);
    return rc;
}

int
dico_linebuf_flush(struct dico_line_buffer *s)
{
    int rc = 0;
    
    switch (s->type) {
    case lb_in:
	rc = dico_stream_read_unbuffered(s->stream, s->buffer, s->size,
					 &s->level);
	break;

    case lb_out:
	if (s->level) {
	    rc = dico_stream_write_unbuffered(s->stream, s->buffer, s->level,
					      NULL);
	    s->level = 0;
	}
	break;
    }

    return rc;
}
