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

char *
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

static void
argv_expand(int *pargc, char ***pargv, int xargc, char **xargv)
{
    size_t nargc = *pargc + xargc + 1;
    char **nargv = xrealloc(*pargv, (nargc + 1) * sizeof nargv[0]);
    nargv[*pargc] = xstrdup("\n");
    memcpy(nargv + 1 + *pargc, xargv, (xargc + 1) * sizeof nargv[0]);
    *pargc = nargc;
    *pargv = nargv;
}

enum kw_tok {
    kw_login,
    kw_password,
    kw_noauth,
    kw_nosasl,
    kw_sasl,
    kw_mechanism,
    kw_realm,
    kw_service,
    kw_host
};
    
struct keyword {
    char *name;
    int arg;
    enum kw_tok tok;
};

static struct keyword kwtab[] = {
    { "login", 1, kw_login },
    { "password", 1, kw_password },
    { "noauth", 0, kw_noauth },
    { "nosasl", 0, kw_nosasl },
    { "sasl", 0, kw_sasl },
    { "mechanism", 1, kw_mechanism },
    { "realm", 1, kw_realm },
    { "service", 1, kw_service },
    { "host", 1, kw_host },
    { NULL }
};

static struct keyword *
findkw(const char *name)
{
    struct keyword *p;
    for (p = kwtab; p->name; p++)
	if (strcmp(p->name, name) == 0)
	    return p;
    return NULL;
}

/* Parse netrc-like autologin file and set up user and key accordingly. */
int
parse_autologin(const char *filename, char *host, struct auth_cred *pcred,
		int *pflags)
{
    FILE *fp;
    char *buf = NULL;
    size_t n = 0;
    int def_argc = 0;
    char **def_argv = NULL;
    int def_line = 0;
    char **host_argv = NULL;
    int host_argc = 0;
    char ***pp_argv;
    int *pp_argc;
    char **p_argv = NULL;
    int line = 0;
    int flags = 0;
    int stop = 0;
    
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

	if (pp_argv) {
	    if (strcmp(argv[0], "machine") == 0
		|| strcmp(argv[0], "default") == 0) {
		if (stop) {
		    dico_argcv_free(argc, argv);
		    break;
		}
		pp_argv = NULL;
		pp_argc = 0;
	    } else {
		argv_expand(pp_argc, pp_argv, argc, argv);
		free(argv);
		continue;
	    }
	}
	if (strcmp(argv[0], "machine") == 0) {
	    if (hostcmp(argv[1], host) == 0) {
		VDETAIL(1, (_("Found matching line %d\n"), line));
		stop = 1;
		host_argc = argc;
		host_argv = argv;
		pp_argc = &host_argc;
		pp_argv = &host_argv;
		def_line = line;
		continue;
	    }
	} else if (strcmp(argv[0], "default") == 0) {
		VDETAIL(1, (_("Found default line %d\n"), line));
		def_argc = argc;
		def_argv = argv;
		pp_argc = &def_argc;
		pp_argv = &def_argv;
		def_line = line;
		continue;
	} 
	dico_argcv_free(argc, argv);
    }
    fclose(fp);
    free(buf);

    if (host_argv) 
	p_argv = host_argv + 2;
    else if (def_argv) 
	p_argv = def_argv + 1;
    else {
	VDETAIL(1, (_("No matching line found\n")));
	p_argv = NULL;
    }

    if (p_argv) {
	line = def_line;

	pcred->sasl = sasl_enabled_p();
	while (*p_argv)	{
	    if (strcmp(*p_argv, "\n") == 0) {
		line++;
		p_argv++;
	    } else {
		struct keyword *kw = findkw(*p_argv);
		char *arg;
		
		if (!kw) {
		    dico_log(L_ERR, 0,
			     _("%s:%d: unknown keyword"), filename, line);
		    p_argv++;
		    continue;
		}

		if (kw->arg) {
		    if (!p_argv[1]) {
			dico_log(L_ERR, 0,
				 _("%s:%d: %s without argument"),
				 filename, line, p_argv[0]);
			break;
		    }
		    arg = p_argv[1];
		    p_argv += 2;
		} else
		    p_argv++;
		
		switch (kw->tok) {
		case kw_login:
		    pcred->user = xstrdup(arg);
		    flags |= AUTOLOGIN_USERNAME;
		    break;

		case kw_password:
		    pcred->pass = xstrdup(arg);
		    flags |= AUTOLOGIN_PASSWORD;
		    break;

		case kw_service:
		    pcred->service = xstrdup(arg);
		    break;

		case kw_realm:
		    pcred->realm = xstrdup(arg);
		    break;

		case kw_host:
		    pcred->hostname = xstrdup(arg);
		    break;
		    
		case kw_noauth:
		    flags |= AUTOLOGIN_NOAUTH;
		    break;
		    
		case kw_nosasl:
		    pcred->sasl = 0;
		    break;

		case kw_sasl:
		    pcred->sasl = 1;
		    break;
		    
		case kw_mechanism: {
		    int i, c;
		    char **v;
		
		    if (!(flags & AUTOLOGIN_MECH)) {
			pcred->mech = xdico_list_create();
			flags |= AUTOLOGIN_MECH;
		    }
		    if (dico_argcv_get(arg, ",", NULL, &c, &v)) {
			dico_log(L_ERR, 0,
				 _("%s:%d: not enough memory"),
				 filename, line);
			exit(1);
		    }
		    for (i = 0; i < c; i++)
			xdico_list_append(pcred->mech, v[i]);

		    free(v);
		    break;
		  }
		}
	    }
	}
    }
    dico_argcv_free(def_argc, def_argv);
    dico_argcv_free(host_argc, host_argv);

    if (pflags)
	*pflags = flags;
    return 0;
}
