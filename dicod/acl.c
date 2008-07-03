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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <hash.h>

struct dicod_sockaddr {
    unsigned netmask;
    int salen;
    struct sockaddr sa;
};    

struct acl_entry {
    dicod_locus_t locus;
    int allow;
    int authenticated;
    dicod_acl_t acl;
    dico_list_t groups;
    dico_list_t sockaddrs;
};

struct dicod_acl {
    char *name;
    dicod_locus_t locus;
    dico_list_t list;
};


/* ACL creation */

dicod_acl_t
dicod_acl_create(const char *name, dicod_locus_t *locus)
{
    dicod_acl_t acl = xmalloc(sizeof(acl[0]));
    acl->name = xstrdup(name);
    acl->locus = *locus;
    acl->list = dico_list_create();
    return acl;
}

static struct dicod_sockaddr *
create_acl_sockaddr(int family, int len)
{
    struct dicod_sockaddr *p = xzalloc(sizeof(*p));
    p->salen = len;
    p->sa.sa_family = family;
    return p;
}

/* allow|deny [all|authenticated|group <grp: list>]
              [acl <name: string>] [from <addr: list>] */

static int
_parse_token (struct acl_entry *entry, config_value_t *value)
{
    if (strcmp(value->v.string, "all") == 0
	|| strcmp(value->v.string, "any") == 0)
	/* FIXME: Nothing? */;
    else if (strcmp(value->v.string, "auth") == 0
	     || strcmp(value->v.string, "authenticated") == 0)
	entry->authenticated = 1;
    else 
	return 1;
    return 0;
}

static int
_parse_sockaddr(struct acl_entry *entry, config_value_t *value)
{
    struct dicod_sockaddr *sptr;
    const char *string;

    if (value->type != TYPE_STRING) {
	config_error(&entry->locus, 0, _("expected string but found list"));
	return 1;
    }

    string = value->v.string;
    
    if (string[0] == '/') {
	size_t len;
	struct sockaddr_un *s_un;
	
	len = strlen (string);
	if (len >= sizeof(s_un->sun_path)) {
	    config_error(&entry->locus, 0,
			 _("socket name too long: `%s'"),
			 string);
	    return 1;
	}
	sptr = create_acl_sockaddr(AF_UNIX, sizeof(s_un));
	s_un = (struct sockaddr_un *) &sptr->sa;
	memcpy(s_un->sun_path, string, len);
	s_un->sun_path[len] = 0;
    } else {
	struct in_addr addr;
	struct sockaddr_in *s_in;
	char *p = strchr(string, '/');

	if (p)
	    *p = 0;

	if (inet_aton(string, &addr) == 0) {
	    struct hostent *hp = gethostbyname(string);
	    if (!hp) {
		config_error(&entry->locus, 0,
			     _("cannot resolve host name: `%s'"),
			     string);
		if (p)
		    *p = '/';
		return 1;
	    }
	    memcpy(&addr.s_addr, hp->h_addr, sizeof(addr.s_addr));
	}
	addr.s_addr = ntohl(addr.s_addr);

	sptr = create_acl_sockaddr(AF_INET, sizeof(s_in));
	s_in = (struct sockaddr_in *) &sptr->sa;
	s_in->sin_addr = addr;

	if (p) {
	    *p++ = '/';
	    char *q;
	    unsigned netlen;
	  
	    netlen = strtoul(p, &q, 10);
	    if (*q == 0) {
		if (netlen == 0)
		    sptr->netmask = 0;
		else {
		    sptr->netmask = 0xfffffffful >> (32 - netlen);
		    sptr->netmask <<= (32 - netlen);
		    sptr->netmask = htonl(sptr->netmask);
		}
	    } else if (*q == '.') {
		struct in_addr addr;
	      
		if (inet_aton(p, &addr) == 0) {
		    config_error(&entry->locus, 0,
				 _("invalid netmask: `%s'"),
				 p);
		    return 1;
		}
		sptr->netmask = addr.s_addr;
	    } else {
		config_error(&entry->locus, 0,
			     _("invalid netmask: `%s'"),
			     p);
		return 1;
	    }
	} else
	    sptr->netmask = 0xfffffffful;
    }
    xdico_list_append(entry->sockaddrs, sptr);
    return 0;
}

static int
_parse_from(struct acl_entry *entry, size_t argc, config_value_t *argv)
{
    if (argc == 0)
	return 0;
    else if (argv->type == TYPE_LIST) {
	config_error(&entry->locus, 0, _("expected `from', but found list"));
	return 1;
    } else if (strcmp (argv->v.string, "from")) {
	config_error(&entry->locus, 0, _("expected `from', but found `%s'"),
		     argv->v.string);
	return 1;
    }
    argc--;
    argv++;

    if (argc == 0) {
	config_error(&entry->locus, 0,
		     _("unexpected end of statement after `from'"));
	return 1;
    }

    entry->sockaddrs = xdico_list_create();
    if (argv->type == TYPE_STRING) {
	if (_parse_sockaddr(entry, argv))
	    return 1;
    } else {
	dico_iterator_t itr = xdico_iterator_create(argv->v.list);
	config_value_t *p;
	int rc = 0;
	for (p = dico_iterator_first(itr); p; p = dico_iterator_next(itr)) 
	    rc += _parse_sockaddr(entry, p);
	if (rc)
	    return rc;
    }

    if (argc - 1) {
	config_warning(&entry->locus, 0,
		       _("junk after `from' list"));
	return 1;
    }
    return 0;
}

static int
_parse_sub_acl(struct acl_entry *entry, size_t argc, config_value_t *argv)
{
    if (argc == 0)
	return 0;
    if (strcmp (argv->v.string, "acl") == 0) {
	argc--;
	argv++;
	if (argc == 0) {
	    config_error(&entry->locus, 0,
			 _("expected ACL name, but found end of statement"));
	    return 1;
	}

	if (argv->type != TYPE_STRING) {
	    config_error(&entry->locus, 0,
			 _("expected string, but found list"));
	    return 1;
	}

	entry->acl = dicod_acl_lookup(argv->v.string);

	if (!entry->acl) {
	    config_error(&entry->locus, 0, _("ACL not defined: `%s'"),
			 argv->v.string);
	    return 1;
	}
	argc--;
	argv++;
    }
    return _parse_from(entry, argc, argv);
}
    
static int
_parse_group(struct acl_entry *entry, size_t argc, config_value_t *argv)
{
    if (strcmp (argv->v.string, "group") == 0) {
	argc--;
	argv++;
	if (argc == 0) {
	    config_error(&entry->locus, 0,
			 _("expected group list, but found end of statement"));
	    return 1;
	}
	if (argv->type == TYPE_STRING) {
	    entry->groups = xdico_list_create();
	    xdico_list_append(entry->groups, (void*)argv->v.string);
	} else
	    entry->groups = argv->v.list;
	argc--;
	argv++;
    }  
    return _parse_sub_acl(entry, argc, argv);
}
	
static int
_parse_acl(struct acl_entry *entry, size_t argc, config_value_t *argv)
{
    if (argv[0].type != TYPE_STRING) {
	config_error(&entry->locus, 0, _("expected string but found list"));
	return 1;
    } else if (_parse_token(entry, argv) == 0)
	return _parse_sub_acl(entry, argc - 1, argv + 1);
    else
	return _parse_group (entry, argc, argv);
}

int
parse_acl_line(dicod_locus_t *locus, int allow, dicod_acl_t acl,
	       config_value_t *value)
{
    struct acl_entry *entry = xzalloc(sizeof(*entry));

    entry->locus = *locus;
    entry->allow = allow;

    switch (value->type) {
    case TYPE_STRING:
	if (_parse_token (entry, value)) {
	    config_error(&entry->locus, 0, _("unknown word `%s'"),
			 value->v.string);
	    return 1;
	}
	break;
	
    case TYPE_ARRAY:
	if (_parse_acl(entry, value->v.arg.c, value->v.arg.v))
	    return 1;
	break;
	
    case TYPE_LIST:
	config_error(locus, 0, _("unexpected list"));
	return 1;
    }
    xdico_list_append(acl->list, entry);
    return 0;
}


/* ACL verification */

static int
cmp_group_name(const void *item, const void *data)
{
    return strcmp((char*)item, (char*)data);
}

#define S_UN_NAME(sa, salen) \
  ((salen < offsetof (struct sockaddr_un,sun_path)) ? "" : (sa)->sun_path)

static int
_check_sockaddr(void *item, void *data)
{
    struct dicod_sockaddr *sptr = item;
    int *pres = data;
    
    if (sptr->sa.sa_family != client_addr.sa_family)
	return 0;
    
    switch (sptr->sa.sa_family) {
    case AF_INET:
        {
	    struct sockaddr_in *sin_clt = (struct sockaddr_in *)&client_addr;
	    struct sockaddr_in *sin_item = (struct sockaddr_in *)&sptr->sa;
	    
	    if (ntohl (sin_clt->sin_addr.s_addr) ==
		(sin_item->sin_addr.s_addr & sptr->netmask)) {
		*pres = 1;
		return 1;
	    }
	    break;
	}
	
    case AF_UNIX:
        {
	    struct sockaddr_un *sun_clt = (struct sockaddr_un *)&client_addr;
	    struct sockaddr_un *sun_item = (struct sockaddr_un *)&sptr->sa;
	    
	    if (S_UN_NAME (sun_clt, client_addrlen)[0]
		&& S_UN_NAME (sun_item, sptr->salen)[0]
		&& strcmp (sun_clt->sun_path, sun_item->sun_path) == 0) {
		*pres = 1;
		return 1;
	    }
	}
    }
    return 0;
}

static int
_acl_check(struct acl_entry *ent)
{
    int result = 1;
    
    if (ent->authenticated) {
	result = user_name != NULL;
	if (!result)
	    return result;
    }

    if (ent->groups) {
	result = dico_list_intersect_p(ent->groups, user_groups,
				       cmp_group_name);
	if (!result)
	    return result;
    }	

    result = dicod_acl_check(ent->acl, 1);
    if (!result)
	return result;
    
    if (ent->sockaddrs) {
	result = 0;
	dico_list_iterate(ent->sockaddrs, _check_sockaddr, &result);
    }
    
    return result;
}

static int
_acl_check_cb(void *item, void *data)
{
    struct acl_entry *ent = item;
    int *pres = data;
    int result = _acl_check(ent);
    if (debug_level > 10) {
	dico_log(L_DEBUG, 0, "%s:%d: %s",
		 ent->locus.file, ent->locus.line,
		 result ? _("MATCHES") : _("does not match"));
    }
    if (result) {
	*pres = ent->allow;
	return 1;
    }
    return 0;
}
    
int
dicod_acl_check(dicod_acl_t acl, int result)
{
    if (acl) 
	dico_list_iterate(acl->list, _acl_check_cb, &result);
    return result;
}


/* Hash table */

static Hash_table *acl_table;

/* Calculate the hash of a string.  */
static size_t
acl_hasher(void const *data, unsigned n_buckets)
{
    const struct dicod_acl *p = data;
    return hash_string(p->name, n_buckets);
}

/* Compare two strings for equality.  */
static bool
acl_compare(void const *data1, void const *data2)
{
    const struct dicod_acl *p1 = data1;
    const struct dicod_acl *p2 = data2;
    return strcasecmp(p1->name, p2->name) == 0;
}

int
dicod_acl_install(dicod_acl_t acl, dicod_locus_t *locus)
{
    dicod_acl_t ret;
    if (! ((acl_table
	    || (acl_table = hash_initialize(0, 0, 
					    acl_hasher,
					    acl_compare, 0)))
	   && (ret = hash_insert(acl_table, acl))))
	xalloc_die();

    if (ret != acl) {
	if (locus)
	    *locus = ret->locus;
	return 1;
    }
    return 0;
}

dicod_acl_t
dicod_acl_lookup(const char *name)
{
    struct dicod_acl samp;
    if (!acl_table)
	return NULL;
    samp.name = (char*) name;
    return hash_lookup(acl_table, &samp);
}

