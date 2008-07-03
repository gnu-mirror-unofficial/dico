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

#include <dicod.h>
#include <unistd.h>

dico_stream_t
fd_stream_create(int ifd, int ofd)
{
    dico_stream_t in, out;
    dico_stream_t str;

    in = dico_fd_stream_create(ifd, DICO_STREAM_READ);
    if (!in)
	return 0;
    out = dico_fd_stream_create(ofd, DICO_STREAM_WRITE);
    if (!out) {
	dico_stream_destroy(&in);
	return NULL;
    }

    str = dico_io_stream(in, out);
    if (!str) {
	dico_stream_destroy(&in);
	dico_stream_destroy(&out);
    }
    return str;
}
