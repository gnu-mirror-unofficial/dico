/* This file is part of Gjdcit
   Copyright (C) 2003,2004,2007,2008 Sergey Poznyakoff
  
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <dico.h>
#include <string.h>
#include <errno.h>

/* proto://[user[:password]@][host/]path[;arg=str[;arg=str...] */

static int
alloc_string(char **sptr, const char *start, const char *end)
{
    size_t len = end - start;
    *sptr = malloc(len + 1);
    if (!*sptr)
	return 1;
    memcpy(*sptr, start, len);
    (*sptr)[len] = 0;
    return 0;
}

static int
url_parse_arg(dico_url_t url, char *p, char *q)
{
    char *s;
    char *key, *value = NULL;
    
    for (s = p; s < q && *s != '='; s++)
	;

    if (alloc_string(&key, p, s))
	return 1;
    if (s != q) {
	if (alloc_string(&value, s + 1, q))
	    return 1;
    }
    dico_assoc_add(url->args, key, value);
    free(key);
    free(value);
    return 0;
}

static int
url_get_args(dico_url_t url, char **str)
{
    int rc;
    char *p;

    if (!**str)
	return 0;

    url->args = dico_assoc_create();
    if (!url->args)
	return 1;
    for (p = *str, rc = 0; !rc;) {
	char *q = strchr (p, ';');
	if (q) {
	    rc = url_parse_arg(url, p, q);
	    p = q + 1;
	} else {
	    rc = url_parse_arg(url, p, p + strlen(p));
	    break;
	}
    }
    return rc;
}

static int
url_get_path(dico_url_t url, char **str)
{
    char *p;
    
    p = strchr(*str, ';');
    if (!p)
	p = *str + strlen(*str);
    if (alloc_string(&url->path, *str, p))
	return 1;
    *str = p;
    if (*p)
	++ * str;
    return url_get_args(url, str);

}

/* On input str points at the beginning of host part */
static int
url_get_host(dico_url_t url, char **str)
{
    char *p;

    p = strchr(*str, '/');

    if (p) {
	if (alloc_string(&url->host, *str, p))
	    return 1;
	*str = p + 1;
    }
    return url_get_path(url, str);
}

/* On input str points past the ':' */
static int
url_get_passwd(dico_url_t url, char **str)
{
    char *p;

    p = strchr(*str, '@');

    if (p) {
	if (alloc_string(&url->passwd, *str, p))
	    return 1;
	*str = p + 1;
    }
    return url_get_host(url, str);
}

/* On input str points past the mech:// part */
static int
url_get_user (dico_url_t url, char **str)
{
    char *p;

    for (p = *str; *p && !strchr (":@", *p); p++)
	;

    switch (*p)
	{
	case ':':
	    if (alloc_string (&url->user, *str, p))
		return 1;
	    *str = p + 1;
	    return url_get_passwd(url, str);
	case '@':
	    if (alloc_string(&url->user, *str, p))
		return 1;
	    url->passwd = NULL;
	    *str = p + 1;
	}
    return url_get_host(url, str);
}

static int
url_get_proto(dico_url_t url, const char *str)
{
    char *p;
    
    if (!str) {
	errno = EINVAL;
	return 1;
    }
    
    p = strchr (str, ':');
    if (!p) {
	errno = EINVAL;
	return 1;
    }
    
    alloc_string(&url->proto, str, p);

    /* Skip slashes */
    for (p++; *p == '/'; p++)
	;
    return url_get_user(url, &p);
}

void
dico_url_destroy(dico_url_t *purl)
{
    dico_url_t url = *purl;

    free(url->string);
    free(url->proto);
    free(url->host);
    free(url->path);
    free(url->user);
    free(url->passwd);
    dico_assoc_destroy(&url->args);
    free(url);
    *purl = NULL;
}

int
dico_url_parse(dico_url_t *purl, const char *str)
{
    int rc;
    dico_url_t url;
    
    url = malloc(sizeof (*url));
    if (url)
	return 1;
    memset(url, 0, sizeof(*url));
    rc = url_get_proto(url, str);
    if (rc)
	dico_url_destroy(&url);
    else {
	url->string = strdup(str);
	*purl = url;
    }
    return rc;
}

char *
dico_url_full_path(dico_url_t url)
{
    char *path;
    size_t size = 1;

    if (url->host)
	size += strlen(url->host);
    if (url->path)
	size += strlen(url->path) + 1;
    path = malloc(size + 1);
    if (path) {
	if (url->host) {
	    strcpy(path, "/");
	    strcat(path, url->host);
	}
	if (url->path) {
	    if (path[0])
		strcat(path, "/");
	    strcat(path, url->path);
	}
    }
    return path;
}

const char *
dico_url_get_arg(dico_url_t url, const char *argname)
{
    return dico_assoc_find(url->args, argname);
}
