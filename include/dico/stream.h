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

#ifndef __dico_stream_h
#define __dico_stream_h

/* Line buffer */
typedef struct dico_line_buffer *dico_linebuf_t;
enum dico_line_buffer_type { lb_in, lb_out };
typedef struct dico_stream *dico_stream_t;

int dico_linebuf_create(dico_linebuf_t *s, dico_stream_t stream,
			enum dico_line_buffer_type type, size_t size);
void dico_linebuf_destroy(dico_linebuf_t *s);
void dico_linebuf_drop(dico_linebuf_t s);

int dico_linebuf_grow(dico_linebuf_t s, const char *ptr, size_t size);
size_t dico_linebuf_read(dico_linebuf_t s, char *ptr, size_t size);
int dico_linebuf_readline(dico_linebuf_t s, char *ptr, size_t size);
int dico_linebuf_write(dico_linebuf_t s, char *ptr, size_t size);
int dico_linebuf_writelines(dico_linebuf_t s);
size_t dico_linebuf_level(dico_linebuf_t s);
char *dico_linebuf_data(dico_linebuf_t s);
int dico_linebuf_flush(dico_linebuf_t s);


/* Streams */

int dico_stream_create(dico_stream_t *pstream,
		       void *data, 
		       int (*readfn) (void *, char *, size_t, size_t *),
		       int (*writefn) (void *, char *, size_t, size_t *),
		       int (*closefn) (void *));

void dico_stream_set_error_string(dico_stream_t stream,
				  const char *(*error_string) (void *, int));

int dico_stream_set_buffer(dico_stream_t stream,
			   enum dico_line_buffer_type type,
			   size_t size);

int dico_stream_read_unbuffered(dico_stream_t stream, char *buf, size_t size,
				size_t *pread);
int dico_stream_write_unbuffered(dico_stream_t stream, char *buf, size_t size,
				 size_t *pwrite);

int dico_stream_read(dico_stream_t stream, char *buf, size_t size,
		     size_t *pread);
int dico_stream_readln(dico_stream_t stream, char *buf, size_t size,
		       size_t *pread);
int dico_stream_getline(dico_stream_t stream, char **pbuf, size_t *psize,
			size_t *pread);
int dico_stream_write(dico_stream_t stream, char *buf, size_t size);
int dico_stream_writeln(dico_stream_t stream, char *buf, size_t size);

const char *dico_stream_strerror(dico_stream_t stream, int rc);
int dico_stream_last_error(dico_stream_t stream);
void dico_stream_clearerr(dico_stream_t stream);
int dico_stream_eof(dico_stream_t stream);

int dico_stream_flush(dico_stream_t stream);
int dico_stream_close(dico_stream_t stream);
void dico_stream_destroy(dico_stream_t *stream);

#endif
