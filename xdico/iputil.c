/* This file is part of GNU Dico.
   Copyright (C) 1998-2019 Sergey Poznyakoff

   GNU Dico is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Dico is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>. */

#include <config.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <intprops.h>
#include <inttostr.h>

#include <xdico.h>
#include <xalloc.h>

static size_t
mu_stpcpy (char **pbuf, size_t *psize, const char *src)
{
    size_t slen = strlen (src);
    if (pbuf == NULL || *pbuf == NULL)
	return slen;
    else {
	char *buf = *pbuf;
	size_t size = *psize;
	if (size > slen)
	    size = slen;
	memcpy (buf, src, size);
	*psize -= size;
	*pbuf += size;
	if (*psize)
	    **pbuf = 0;
	else
	    (*pbuf)[-1] = 0;
	return size;
    }
}

#define S_UN_NAME(sa, salen) \
  ((salen < offsetof (struct sockaddr_un,sun_path)) ? "" : (sa)->sun_path)

void
sockaddr_to_str (const struct sockaddr *sa, int salen,
		 char *bufptr, size_t buflen,
		 size_t *plen)
{
    char buf[INT_BUFSIZE_BOUND (uintmax_t)]; /* FIXME: too much */
    size_t len = 0;
    switch (sa->sa_family) {
    case AF_INET:
    case AF_INET6:
    {
	char host[NI_MAXHOST];
	char srv[NI_MAXSERV];
	if (getnameinfo(sa, salen,
			host, sizeof(host), srv, sizeof(srv),
			NI_NUMERICHOST|NI_NUMERICSERV) == 0) {
	    if (sa->sa_family == AF_INET6) {
		len += mu_stpcpy(&bufptr, &buflen, "inet6://[");
		len += mu_stpcpy(&bufptr, &buflen, host);
		len += mu_stpcpy(&bufptr, &buflen, "]:");
		len += mu_stpcpy(&bufptr, &buflen, srv);
	    } else {
		len += mu_stpcpy(&bufptr, &buflen, "inet://");
		len += mu_stpcpy(&bufptr, &buflen, host);
		len += mu_stpcpy(&bufptr, &buflen, ":");
		len += mu_stpcpy(&bufptr, &buflen, srv);
	    }
	} else {
	    if (sa->sa_family == AF_INET6)
		len += mu_stpcpy(&bufptr, &buflen, "inet6://[getnameinfo failed]");
	    else
		len += mu_stpcpy(&bufptr, &buflen, "inet://[getnameinfo failed]");
	}
	break;
    }

    case AF_UNIX:
    {
	struct sockaddr_un *s_un = (struct sockaddr_un *)sa;
	if (S_UN_NAME(s_un, salen)[0] == 0)
	    len += mu_stpcpy (&bufptr, &buflen, "anonymous socket");
	else {
	    len += mu_stpcpy (&bufptr, &buflen, "socket ");
	    len += mu_stpcpy (&bufptr, &buflen, s_un->sun_path);
	}
	break;
    }

    default:
	len += mu_stpcpy (&bufptr, &buflen, "{Unsupported family: ");
	len += mu_stpcpy (&bufptr, &buflen, umaxtostr (sa->sa_family, buf));
	len += mu_stpcpy (&bufptr, &buflen, "}");
    }
    if (plen)
	*plen = len + 1;
}

char *
sockaddr_to_astr (const struct sockaddr *sa, int salen)
{
    size_t size;
    char *p;
    
    sockaddr_to_str(sa, salen, NULL, 0, &size);
    p = xmalloc(size);
    sockaddr_to_str(sa, salen, p, size, NULL);
    return p;
}
