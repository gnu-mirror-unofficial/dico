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
