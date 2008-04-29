/* This file is part of Gjdict.
   Copyright (C) 2008 Sergey Poznyakoff

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <dictd.h>


int
dictd_loop(stream_t str)
{
    char *buf = NULL;
    size_t size = 0;
    size_t rdbytes;
    
    while (stream_getline(str, &buf, &size, &rdbytes) == 0) {
	char nbuf[128];
	if (strncmp(buf, "quit", 4) == 0)
	    break;
	sprintf(nbuf, "Read %lu bytes: ", rdbytes);
	stream_write(str, nbuf, strlen(nbuf));
	stream_writeln(str, buf, rdbytes);
    }
    
    stream_close(str);
    stream_destroy(&str);
    return 0;
}

int
dictd_inetd()
{
    stream_t str = fd_stream_create(0, 1);
    stream_set_buffer(str, lb_in, 512);
    stream_set_buffer(str, lb_out, 512);
    return dictd_loop(str);
}
