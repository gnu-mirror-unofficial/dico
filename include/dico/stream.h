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


/* Streams */

enum dico_buffer_type {
    dico_buffer_none,
    dico_buffer_line,
    dico_buffer_full
};

#define DICO_STREAM_READ   0x01
#define DICO_STREAM_WRITE  0x02
#define DICO_STREAM_SEEK   0x04

#define DICO_SEEK_SET      0
#define DICO_SEEK_CUR      1
#define DICO_SEEK_END      2

int dico_stream_create(dico_stream_t *pstream, int flags, void *data);
int dico_stream_open(dico_stream_t stream);
void dico_stream_set_open(dico_stream_t stream, int (*openfn) (void *, int));
void dico_stream_set_seek(dico_stream_t stream,
			  int (*fun_seek) (void *, off_t, int, off_t *));
void dico_stream_set_read(dico_stream_t stream,
			  int (*readfn) (void *, char *, size_t, size_t *));
void dico_stream_set_write(dico_stream_t stream,    
			   int (*writefn) (void *, char *, size_t, size_t *));
void dico_stream_set_flush(dico_stream_t stream, int (*flushfn) (void *));
void dico_stream_set_close(dico_stream_t stream, int (*closefn) (void *));
void dico_stream_set_destroy(dico_stream_t stream, int (*destroyfn) (void *));

void dico_stream_set_error_string(dico_stream_t stream,
				  const char *(*error_string) (void *, int));

int dico_stream_set_buffer(dico_stream_t stream,
			   enum dico_buffer_type type,
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


/* FD streams */
dico_stream_t dico_fd_stream_create(int fd, int flags);

dico_stream_t dico_io_stream(dico_stream_t in, dico_stream_t out);

#endif
