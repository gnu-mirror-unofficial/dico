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

#ifndef __dico_lbuf_h
#define __dico_lbuf_h

/* Line buffer */
enum dico_line_buffer_type { lb_in, lb_out };

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

#endif
