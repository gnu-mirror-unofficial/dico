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

#ifndef __dico_filter_h
#define __dico_filter_h

#include <dico/stream.h>

#define FILTER_ENCODE 0
#define FILTER_DECODE 1

typedef int (*filter_xcode_t) (const char *, size_t,
			       char *, size_t, size_t *, size_t, size_t *);

dico_stream_t filter_stream_create(dico_stream_t str,
				   size_t min_level,
				   size_t max_line_length,
				   filter_xcode_t xcode,
				   int mode);
dico_stream_t dico_base64_stream_create(dico_stream_t str, int mode);

int dico_base64_decode(const char *iptr, size_t isize,
		       char *optr, size_t osize,
		       size_t *pnbytes,
		       size_t line_max, size_t *pline_len);
int dico_base64_encode (const char *iptr, size_t isize,
			char *optr, size_t osize,
			size_t *pnbytes, size_t line_max, size_t *pline_len);


#endif
