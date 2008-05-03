/* This file is part of Gjdcit
   Copyright (C) 2003,2004,2007,2008 Sergey Poznyakoff
  
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

/* proto://[user[:password]@][host/]path[;arg=str[;arg=str...] */

static void
alloc_string(char **sptr, const char *start, const char *end)
{
    size_t len = end - start;
    *sptr = xmalloc(len + 1);
    memcpy(*sptr, start, len);
    (*sptr)[len] = 0;
}

static void
url_parse_arg(dict_url_t url, char *p, char *q)
{
    char *s;
    char *key, *value = NULL;
    
    for (s = p; s < q && *s != '='; s++)
	;

    alloc_string(&key, p, s);
    if (s != q)
	alloc_string(&value, s + 1, q);
    dict_assoc_add(url->args, key, value);
    free(key);
    free(value);
    
}

static int
url_get_args(dict_url_t url, char **str)
{
    char *p;

    if (!**str)
	return 0;

    url->args = dict_assoc_create();
    for (p = *str;;) {
	char *q = strchr (p, ';');
	if (q) {
	    url_parse_arg(url, p, q);
	    p = q + 1;
	} else {
	    url_parse_arg(url, p, p + strlen(p));
	    break;
	}
    }
    return 0;
}

static int
url_get_path(dict_url_t url, char **str)
{
    char *p;
    
    p = strchr(*str, ';');
    if (!p)
	p = *str + strlen(*str);
    alloc_string(&url->path, *str, p);
    *str = p;
    if (*p)
	++ * str;
    return url_get_args(url, str);

}

/* On input str points at the beginning of host part */
static int
url_get_host(dict_url_t url, char **str)
{
    char *p;

    p = strchr(*str, '/');

    if (p) {
	alloc_string(&url->host, *str, p);
	*str = p + 1;
    }
    return url_get_path(url, str);
}

/* On input str points past the ':' */
static int
url_get_passwd(dict_url_t url, char **str)
{
    char *p;

    p = strchr(*str, '@');

    if (p) {
	alloc_string(&url->passwd, *str, p);
	*str = p + 1;
    }
    return url_get_host(url, str);
}

/* On input str points past the mech:// part */
static int
url_get_user (dict_url_t url, char **str)
{
    char *p;

    for (p = *str; *p && !strchr (":@", *p); p++)
	;

    switch (*p)
	{
	case ':':
	    alloc_string (&url->user, *str, p);
	    *str = p + 1;
	    return url_get_passwd(url, str);
	case '@':
	    alloc_string(&url->user, *str, p);
	    url->passwd = NULL;
	    *str = p + 1;
	}
    return url_get_host(url, str);
}

static int
url_get_proto(dict_url_t url, const char *str)
{
    char *p;
    
    if (!str)
	return 1;

    p = strchr (str, ':');
    if (!p)
	return 1;
    alloc_string(&url->proto, str, p);

    /* Skip slashes */
    for (p++; *p == '/'; p++)
	;
    return url_get_user(url, &p);
}

void
dict_url_destroy(dict_url_t *purl)
{
    dict_url_t url = *purl;

    free(url->string);
    free(url->proto);
    free(url->host);
    free(url->path);
    free(url->user);
    free(url->passwd);
    dict_assoc_destroy(&url->args);
    free(url);
    *purl = NULL;
}

int
dict_url_parse(dict_url_t *purl, const char *str)
{
    int rc;
    dict_url_t url;
    
    url = xzalloc(sizeof (*url));
    rc = url_get_proto(url, str);
    if (rc)
	dict_url_destroy(&url);
    else {
	url->string = strdup(str);
	*purl = url;
    }
    return rc;
}

char *
dict_url_full_path(dict_url_t url)
{
    char *path;
    size_t size = 1;

    if (url->host)
	size += strlen(url->host);
    if (url->path)
	size += strlen(url->path) + 1;
    path = xmalloc(size + 1);
    if (url->host) {
	strcpy(path, "/");
	strcat(path, url->host);
    }
    if (url->path) {
	if (path[0])
	    strcat(path, "/");
	strcat(path, url->path);
    }
    return path;
}

const char *
dict_url_get_arg(dict_url_t url, const char *argname)
{
    return dict_assoc_find(url->args, argname);
}
