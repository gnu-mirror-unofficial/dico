/* This file is part of GNU Dico.
   Copyright (C) 2008-2019 Sergey Poznyakoff

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

union acl_value {
    int family;
    struct {
	struct sockaddr_un s_un;
	int s_len;
    } s;
    struct grecs_cidr cidr;
};

struct acl_entry {
    grecs_locus_t locus;
    int allow;
    int authenticated;
    int dflt;
    dicod_acl_t acl;
    dico_list_t groups;
    dico_list_t values;
};

struct dicod_acl {
    char *name;
    grecs_locus_t locus;
    dico_list_t list;
};

dico_list_t
dicod_string_list_from_grecs(struct grecs_list *inlist)
{
    struct grecs_list_entry *ep;

    dico_list_t list = xdico_list_create();
    dico_list_set_free_item(list, dicod_free_item, NULL);

    for (ep = inlist->head; ep; ep = ep->next) {
	grecs_value_t *vp = ep->data;
	if (vp->type != GRECS_TYPE_STRING) {
	    grecs_error(&vp->locus, 0, _("expected string"));
	    break;
	}
	xdico_list_append(list, xstrdup(vp->v.string));
    }
    return list;
}


/* ACL creation */

dicod_acl_t
dicod_acl_create(const char *name, grecs_locus_t *locus)
{
    dicod_acl_t acl = xmalloc(sizeof(acl[0]));
    acl->name = xstrdup(name);
    acl->locus = *locus;
    acl->locus.beg.file = strdup(acl->locus.beg.file);
    acl->locus.end.file = strdup(acl->locus.end.file);
    acl->list = dico_list_create();
    return acl;
}

/* allow|deny [all|authenticated|group <grp: list>]
              [acl <name: string>] [from <addr: list>] */

static int
_parse_token (struct acl_entry *entry, grecs_value_t *value)
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
_parse_acl_value(struct acl_entry *entry, grecs_value_t *value)
{
    union acl_value *val;
    const char *string;
    
    if (value->type != GRECS_TYPE_STRING) {
	grecs_error(&entry->locus, 0, _("expected string but found list"));
	return 1;
    }

    string = value->v.string;

    if (string[0] == '/') {
	size_t len;
	struct sockaddr_un *s_un;
	
	len = strlen (string);
	if (len >= sizeof(s_un->sun_path)) {
	    grecs_error(&entry->locus, 0,
			 _("socket name too long: `%s'"),
			 string);
	    return 1;
	}
	val = grecs_malloc(sizeof(*val));
	s_un = (struct sockaddr_un *) &val->s.s_un;
	s_un->sun_family = AF_UNIX;
	memcpy(s_un->sun_path, string, len);
	s_un->sun_path[len] = 0;
	val->s.s_len = offsetof(struct sockaddr_un,sun_path) + len;
    } else {
	struct grecs_cidr cidr;
	
	if (grecs_str_to_cidr(&cidr, string, &value->locus))
	    return -1;
	val = grecs_malloc(sizeof(*val));
	val->cidr = cidr;
    }
    xdico_list_append(entry->values, val);
    return 0;
}

static int
_parse_from(struct acl_entry *entry, size_t argc, grecs_value_t **argv)
{
    if (argc == 0)
	return 0;
    else if (argv[0]->type == GRECS_TYPE_LIST) {
	grecs_error(&argv[0]->locus, 0, _("expected `from', but found list"));
	return 1;
    } else if (strcmp (argv[0]->v.string, "from")) {
	grecs_error(&argv[0]->locus, 0, _("expected `from', but found `%s'"),
		     argv[0]->v.string);
	return 1;
    }
    argc--;
    argv++;

    if (argc == 0) {
	grecs_error(&entry->locus, 0,
		     _("unexpected end of statement after `from'"));
	return 1;
    }

    if (argv[0]->type == GRECS_TYPE_STRING &&
	strcmp(argv[0]->v.string, "any") == 0) {
	entry->dflt = 1;
    } else {
	entry->values = xdico_list_create();
	if (argv[0]->type == GRECS_TYPE_STRING) {
	    if (_parse_acl_value(entry, argv[0]))
		return 1;
	} else {
	    int rc = 0;
	    struct grecs_list_entry *ep;
	    
	    for (ep = argv[0]->v.list->head; ep; ep = ep->next) {
		grecs_value_t *p = ep->data;
		rc += _parse_acl_value(entry, p);
	    }
	    if (rc)
		return rc;
	}
    }
    
    if (argc - 1) {
	grecs_warning(&entry->locus, 0, _("junk after `from' list"));
	return 1;
    }
    return 0;
}

static int
_parse_sub_acl(struct acl_entry *entry, size_t argc, grecs_value_t **argv)
{
    if (argc == 0)
	return 0;
    if (strcmp (argv[0]->v.string, "acl") == 0) {
	argc--;
	argv++;
	if (argc == 0) {
	    grecs_error(&entry->locus, 0,
			 _("expected ACL name, but found end of statement"));
	    return 1;
	}

	if (argv[0]->type != GRECS_TYPE_STRING) {
	    grecs_error(&argv[0]->locus, 0,
			 _("expected string, but found list"));
	    return 1;
	}

	entry->acl = dicod_acl_lookup(argv[0]->v.string);

	if (!entry->acl) {
	    grecs_error(&entry->locus, 0, _("ACL not defined: `%s'"),
			 argv[0]->v.string);
	    return 1;
	}
	argc--;
	argv++;
    }
    return _parse_from(entry, argc, argv);
}
    
static int
_parse_group(struct acl_entry *entry, size_t argc, grecs_value_t **argv)
{
    if (strcmp (argv[0]->v.string, "group") == 0) {
	argc--;
	argv++;
	if (argc == 0) {
	    grecs_error(&entry->locus, 0,
			 _("expected group list, but found end of statement"));
	    return 1;
	}
	switch (argv[0]->type) {
	case GRECS_TYPE_STRING:
	    entry->groups = xdico_list_create();
	    xdico_list_append(entry->groups, xstrdup(argv[0]->v.string));
	    break;
	case GRECS_TYPE_LIST:
	    entry->groups = dicod_string_list_from_grecs(argv[0]->v.list);
	    break;
	default:
	    grecs_error(&argv[0]->locus, 0,
			_("expected group list, but found array"));
	    return 1;
	}	    
	argc--;
	argv++;
    }  
    return _parse_sub_acl(entry, argc, argv);
}
	
static int
_parse_acl(struct acl_entry *entry, size_t argc, grecs_value_t **argv)
{
    if (argv[0]->type != GRECS_TYPE_STRING) {
	grecs_error(&argv[0]->locus, 0, _("expected string value"));
	return 1;
    } else if (_parse_token(entry, argv[0]) == 0)
	return _parse_sub_acl(entry, argc - 1, argv + 1);
    else
	return _parse_group (entry, argc, argv);
}

int
parse_acl_line(grecs_locus_t *locus, int allow, dicod_acl_t acl,
	       grecs_value_t *value)
{
    struct acl_entry *entry = xzalloc(sizeof(*entry));

    entry->locus = *locus;
    entry->locus.beg.file = strdup(entry->locus.beg.file);
    entry->locus.end.file = strdup(entry->locus.end.file);
    entry->allow = allow;

    switch (value->type) {
    case GRECS_TYPE_STRING:
	if (_parse_token (entry, value)) {
	    grecs_error(&entry->locus, 0, _("unknown word `%s'"),
			value->v.string);
	    return 1;
	}
	break;
	
    case GRECS_TYPE_ARRAY:
	if (_parse_acl(entry, value->v.arg.c, value->v.arg.v))
	    return 1;
	break;
	
    case GRECS_TYPE_LIST:
	grecs_error(locus, 0, _("unexpected list"));
	return 1;
    }
    xdico_list_append(acl->list, entry);
    return 0;
}


/* ACL verification */

static int
cmp_group_name(const void *item, const void *data, void *ignore)
{
    return strcmp((char*)item, (char*)data);
}

#define S_UN_NAME(sa, salen) \
  ((salen < offsetof(struct sockaddr_un,sun_path)) ? "" : (sa)->sun_path)

static int
_check_value(void *item, void *data)
{
    union acl_value *vptr = item;
    int *pres = data;
    
    if (vptr->family != client_addr.ss_family)
	return 0;
    
    switch (vptr->family) {
    case AF_INET:
    case AF_INET6:
	if (grecs_sockadd_cidr_match((struct sockaddr*)&client_addr,
				     &vptr->cidr) == 0) {
	    *pres = 1;
	    return 1;
	}
	break;
	
    case AF_UNIX:
        {
	    struct sockaddr_un *sun_clt = (struct sockaddr_un *)&client_addr;
	    struct sockaddr_un *sun_item = &vptr->s.s_un;
	    
	    if (S_UN_NAME(sun_clt, client_addrlen)[0]
		&& S_UN_NAME(sun_item, vptr->s.s_len)[0]
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
	    return 0;
    }

    if (ent->groups) {
	result = dico_list_intersect_p(ent->groups, user_groups,
				       cmp_group_name, NULL);
	if (!result)
	    return 0;
    }	

    result = dicod_acl_check(ent->acl, 1);
    if (!result)
	return 0;

    if (ent->dflt)
	result = 1;
    else if (ent->values) {
	result = 0;
	dico_list_iterate(ent->values, _check_value, &result);
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
		 /* FIXME: beg:end */
		 ent->locus.beg.file, ent->locus.beg.line,
		 /* TRANSLATORS: `MATCHES' is the verb `match' in 2nd person.
		    E.g., in French: CONCORD AVEC */
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
acl_hasher(void const *data, size_t n_buckets)
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
dicod_acl_install(dicod_acl_t acl, grecs_locus_t *locus)
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

