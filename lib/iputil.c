/* This file is part of Gjdict.
   Copyright (C) 1998-2000, 2008 Sergey Poznyakoff

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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <gjdict.h>

/*
 *	Return an IP address in host long notation from a host
 *	name or address in dot notation.
 */
IPADDR
get_ipaddr(char *host)
{
    struct hostent *hp;
    struct in_addr addr;

    if (inet_aton(host, &addr))
	return ntohl(addr.s_addr);
    else if ((hp = gethostbyname(host)) == NULL)
	return 0;
    else
	return ntohl(*(UINT4 *) hp->h_addr);
}

/*
 *	Return a printable host name (or IP address in dot notation)
 *	for the supplied IP address.
 */
char *
ip_hostname(IPADDR ipaddr)
{
    struct hostent *hp;
    UINT4 n_ipaddr;

    n_ipaddr = htonl(ipaddr);
    hp = gethostbyaddr((char *) &n_ipaddr, sizeof(struct in_addr),
		       AF_INET);
    if (!hp) {
	struct in_addr in;
	in.s_addr = htonl(ipaddr);
	return inet_ntoa(in);
    }
    return (char *) hp->h_name;
}

IPADDR
getmyip()
{
    char myname[256];

    gethostname(myname, sizeof(myname));
    return get_ipaddr(myname);
}

int
str2port(char *str)
{
    struct servent *serv;
    char *p;
    int port;

    /* First try to read it from /etc/services */
    serv = getservbyname(str, "tcp");

    if (serv != NULL)
	port = ntohs(serv->s_port);
    else {
	long l;
	/* Not in services, maybe a number? */
	l = strtol(str, &p, 0);

	if (*p || l < 0 || l > USHRT_MAX)
	    return -1;

	port = l;
    }

    return port;
}
