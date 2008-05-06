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

#include <dictd.h>
#include <unistd.h>

struct fds {
    int in;
    int out;
};
    

static int
fd_read(void *data, char *buf, size_t size, size_t *pret)
{
    struct fds *p = data;
    int n = read(p->in, buf, size);
    if (n == -1)
	return errno;
    *pret = n;
    return 0;
}

static int
fd_write(void *data, char *buf, size_t size, size_t *pret)
{
    struct fds *p = data;
    int n = write(p->out, buf, size);
    if (n == -1)
	return errno;
    *pret = n;
    return 0;
}

static int
fd_close(void *data)
{
    struct fds *p = data;
    int rc1 = close(p->in), rc2 = close(p->out);

    if (rc1 || rc2)
	return errno;
    return 0;
}

dico_stream_t
fd_stream_create(int ifd, int ofd)
{
    struct fds *p = xmalloc(sizeof(*p));
    dico_stream_t stream;
    int rc = dico_stream_create(&stream, p, 
			        fd_read, fd_write, fd_close);
    if (rc)
	xalloc_die();
    p->in = ifd;
    p->out = ofd;
    return stream;
}
