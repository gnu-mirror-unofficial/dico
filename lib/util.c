/* This file is part of Gjdcit
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <gjdict.h>
#include <string.h>

void
trimnl(char *buf, size_t len)
{
    if (len > 1 && buf[--len] == '\n') {
	buf[len] = 0;
	if (len > 1 && buf[--len] == '\r')
	    buf[len] = 0;
    }
}

char *
make_full_file_name(const char *dir, const char *file)
{
    size_t dirlen = strlen(dir);
    int need_slash = !(dirlen && dir[dirlen - 1] == '/');
    size_t size = dirlen + need_slash + 1 + strlen(file) + 1;
    char *buf = xmalloc(size);

    strcpy(buf, dir);
    if (need_slash)
	strcpy(buf + dirlen++, "/");
    else {
	while (dirlen > 0 && buf[dirlen-1] == '/')
	    dirlen--;
	dirlen++;
    }
    while (*file == '/')
	file++;
    strcpy(buf + dirlen, file);
    return buf;
}
