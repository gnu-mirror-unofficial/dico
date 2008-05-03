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

#include <dictd.h>

struct dictd_user_db {
    void *handle;
    dict_url_t url;
    const char *qpw;
    const char *qgrp;
    int (*_db_open) (void **, dict_url_t url);
    int (*_db_close) (void *);
    int (*_db_get_password) (void *, const char *, const char *, char **);
    int (*_db_get_groups) (void *, const char *, const char *, char ***);
};

int
udb_open(dictd_user_db_t db)
{
    if (!db->_db_open)
	return 0;
    return db->_db_open(&db->handle, db->url);
}

int
udb_close(dictd_user_db_t db)
{
    int rc;

    if (!db->_db_close)
	rc = 0;
    else 
	rc = db->_db_close(db->handle);
    db->handle = NULL;
    return rc;
}

int
udb_get_password(dictd_user_db_t db, const char *key, char **pass)
{
    return db->_db_get_password(db->handle, db->qpw, key, pass);
}

int
udb_get_groups(dictd_user_db_t db, const char *key, char ***groups)
{
    return db->_db_get_groups(db->handle, db->qgrp, key, groups);
}

dict_list_t /* of struct udb_def */ udb_def_list;

void
udp_define(struct udb_def *dptr)
{
    if (!udb_def_list)
	udb_def_list = dict_list_create();
    dict_list_append(udb_def_list, dptr);
}

static int
udb_def_cmp(const void *item, const void *data)
{
    const struct udb_def *def = item;
    const char *proto = data;
    return strcmp(def->proto, proto);
}

int
udb_create(dictd_user_db_t *pdb,
	   const char *urlstr, const char *qpw, const char *qgrp,
	   gd_locus_t *locus)
{
    dict_url_t url;
    int rc;
    struct udb_def *def;
    struct dictd_user_db *uptr;
    
    rc = dict_url_parse(&url, urlstr);
    if (rc) {
	config_error(locus, 0, _("invalid URL"));
	return 1;
    }

    def = dict_list_locate(udb_def_list, url->proto, udb_def_cmp);
    if (!def) {
	config_error(locus, 0, _("invalid URL: unknown protocol"));
	dict_url_destroy(&url);
	return 1;
    }

    uptr = xzalloc(sizeof(*uptr));
    
    uptr->url = url;
    uptr->qpw = qpw;
    uptr->qgrp = qgrp;
    uptr->_db_open = def->_db_open;
    uptr->_db_close = def->_db_close;
    uptr->_db_get_password = def->_db_get_password;
    uptr->_db_get_groups = def->_db_get_groups;

    *pdb = uptr;
    return 0;
}

void
udb_init()
{
    udp_define(&text_udb_def);
}
