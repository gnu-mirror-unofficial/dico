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

#include <dictd.h>

struct _line_buffer {
    enum line_buffer_type type;
    stream_t stream;
    char *buffer;        /* Line buffer */
    size_t size;         /* Allocated size */
    size_t level;        /* Current filling level */
};


int
linebuf_create(struct _line_buffer **pbuf,
	       stream_t stream, enum line_buffer_type type,
	       size_t size)
{
    struct _line_buffer *buf;

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
linebuf_destroy(struct _line_buffer **s)
{
    if (s && *s) {
	free((*s)->buffer);
	free(*s);
	*s = NULL;
    }
}

void
linebuf_drop(struct _line_buffer *s)
{
    s->level = 0;
}

int
linebuf_grow(struct _line_buffer *s, const char *ptr, size_t size)
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
linebuf_read(struct _line_buffer *s, char *optr, size_t osize)
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
linebuf_readline(struct _line_buffer *s, char *ptr, size_t size)
{
    char *p = memchr(s->buffer, '\n', s->level);

    if (p)
	size = p - s->buffer + 1;
    return linebuf_read(s, ptr, size);
}

int
linebuf_writelines(struct _line_buffer *s)
{
    if (s->level >= 2) {
	char *start, *end;
      
	for (start = s->buffer,
		 end = memchr(start, '\n', s->buffer + s->level - start);
	     end && end < s->buffer + s->level;
	     start = end + 1,
		 end = memchr(start, '\n', s->buffer + s->level - start)) {
		int rc = stream_write_unbuffered(s->stream, start,
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
linebuf_level(struct _line_buffer *s)
{
    return s->level;
}

char *
linebuf_data(struct _line_buffer *s)
{
    return s->buffer;
}

int
linebuf_write(struct _line_buffer *s, char *ptr, size_t size)
{
    int rc = linebuf_grow(s, ptr, size);
    if (rc == 0)
	rc = linebuf_writelines(s);
    return rc;
}

int
linebuf_flush(struct _line_buffer *s)
{
    int rc;
    
    switch (s->type) {
    case lb_in:
	rc = stream_read_unbuffered(s->stream, s->buffer, s->size, &s->level);
	break;

    case lb_out:
	if (s->level) {
	    rc = stream_write_unbuffered(s->stream, s->buffer, s->level, NULL);
	    s->level = 0;
	}
	break;
    }

    return rc;
}
