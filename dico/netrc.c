/* This file is part of Dico. 
   Copyright (C) 1998-2000, 2008 Sergey Poznyakoff

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

#include "dico-priv.h"

#define VDETAIL(n,s) 

static char *
skipws(char *buf)
{
    while (*buf && isascii(*buf) && isspace(*buf))
	buf++;
    return buf;
}

/* Compare two hostnames. Return 0 if they have the same address type,
   address length *and* at least one of the addresses of A matches
   B */
int
hostcmp(const char *a, const char *b)
{
    struct hostent *hp = gethostbyname(a);
    char **addrlist;
    char *dptr;
    char **addr;
    size_t i, count;
    size_t entry_length;
    int entry_type;

    if (!hp)
	return 1;

    for (count = 1, addr = hp->h_addr_list; *addr; addr++)
	count++;
    addrlist = xmalloc (count * (sizeof *addrlist + hp->h_length)
			- hp->h_length);
    dptr = (char *) (addrlist + count);
    for (i = 0; i < count - 1; i++) {
	memcpy(dptr, hp->h_addr_list[i], hp->h_length);
	addrlist[i] = dptr;
	dptr += hp->h_length;
    }
    addrlist[i] = NULL;
    entry_length = hp->h_length;
    entry_type = hp->h_addrtype;

    hp = gethostbyname(b);
    if (!hp || entry_length != hp->h_length || entry_type != hp->h_addrtype) {
	free(addrlist);
	return 1;
    }

    for (addr = addrlist; *addr; addr++) {
	char **p;

	for (p = hp->h_addr_list; *p; p++) {
	    if (memcmp(*addr, *p, entry_length) == 0) {
		free(addrlist);
		return 0;
	    }
	}
    }
    free(addrlist);
    return 1;
}

/* Parse traditional .netrc file and set up user and key accordingly. */
int
parse_netrc(const char *filename, char *host, struct auth_cred *pcred)
{
    FILE *fp;
    char *buf = NULL;
    size_t n = 0;
    int def_argc = 0;
    char **def_argv;
    char **p_argv = NULL;
    int line = 0;

    fp = fopen (filename, "r");
    if (!fp) {
	if (errno != ENOENT) {
	    dico_log(L_ERR, errno, _("Cannot open netrc file %s"),
		     filename);
	}
	return 1;
    } else
	VDETAIL(1, (_("Opening netrc file %s...\n"), filename));

    while (getline (&buf, &n, fp) > 0 && n > 0) {
	int rc;
	char *p;
	size_t len;
	int argc;
	char **argv;

	line++;
	len = strlen(buf);
	if (len > 1 && buf[len - 1] == '\n')
	    buf[len - 1] = 0;
	p = skipws(buf);
	if (*p == 0 || *p == '#')
	    continue;

	if ((rc = dico_argcv_get(buf, "", "#", &argc, &argv))) {
	    dico_log(L_ERR, rc, _("dico_argcv_get failed"));
	    fclose(fp);
	    free(buf);
	    return 1;
	}
      
	if (strcmp(argv[0], "machine") == 0) {
	    if (hostcmp(argv[1], host) == 0) {
		VDETAIL(1, (_("Found matching line %d\n"), line));

		if (def_argc)
		    dico_argcv_free(def_argc, def_argv);
		def_argc = argc;
		def_argv = argv;
		p_argv = argv + 2;
		break;
	    }
	} else if (strcmp(argv[0], "default") == 0) {
	    VDETAIL(1, (_("Found default line %d\n"), line));

	    if (def_argc)
		dico_argcv_free(def_argc, def_argv);
	    def_argc = argc;
	    def_argv = argv;
	    p_argv = argv + 1;
	} else {
	    VDETAIL(1, (_("Ignoring unrecognized line %d\n"), line));
	    dico_argcv_free(argc, argv);
	}
    }
    fclose(fp);
    free(buf);

    if (!p_argv)
	VDETAIL(1, (_("No matching line found\n")));
    else {
	while (*p_argv)	{
	    if (!p_argv[1]) {
		dico_log(L_ERR, 0,
			 _("%s:%d: incomplete sentence"), filename, line);
		break;
	    }
	    if (strcmp(*p_argv, "login") == 0) 
		pcred->user = xstrdup(p_argv[1]);
	    else if (strcmp(*p_argv, "password") == 0)
		pcred->pass = xstrdup(p_argv[1]);
	    p_argv += 2;
	}
	dico_argcv_free(def_argc, def_argv);
    }
    return 0;
}
