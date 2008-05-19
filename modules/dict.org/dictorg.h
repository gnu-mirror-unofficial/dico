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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <dico.h>
#include <ctype.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#define DICTORG_ENTRY_PREFIX        "00-database"
#define DICTORG_ENTRY_PREFIX_LEN    sizeof(DICTORG_ENTRY_PREFIX)-1
#define DICTORG_SHORT_ENTRY_NAME    DICTORG_ENTRY_PREFIX"-short"
#define DICTORG_LONG_ENTRY_NAME     DICTORG_ENTRY_PREFIX"-long"
#define DICTORG_INFO_ENTRY_NAME     DICTORG_ENTRY_PREFIX"-info"

#define DICTORG_FLAG_UTF8           DICTORG_ENTRY_PREFIX"-utf8"
#define DICTORG_FLAG_8BIT_NEW       DICTORG_ENTRY_PREFIX"-8bit-new"
#define DICTORG_FLAG_8BIT_OLD       DICTORG_ENTRY_PREFIX"-8bit"
#define DICTORG_FLAG_ALLCHARS       DICTORG_ENTRY_PREFIX"-allchars"
#define DICTORG_FLAG_VIRTUAL        DICTORG_ENTRY_PREFIX"-virtual"
#define DICTORG_FLAG_ALPHABET       DICTORG_ENTRY_PREFIX"-alphabet"
#define DICTORG_FLAG_DEFAULT_STRAT  DICTORG_ENTRY_PREFIX"-default-strategy"

#define DICTORG_ENTRY_PLUGIN        DICTORG_ENTRY_PREFIX"-plugin"
#define DICTORG_ENTRY_PLUGIN_DATA   DICTORG_ENTRY_PREFIX"-plugin-data"

#define DICTORG_UNKNOWN    0
#define DICTORG_TEXT       1
#define DICTORG_GZIP       2
#define DICTORG_DZIP       3

struct index_entry {
    char *word;             /* Word */
    size_t length;          /* Its length in bytes */
    size_t wordlen;         /* Its length in characters */
    off_t offset;           /* Offset of the corresponding article in file */
    size_t size;            /* Size of the article */
};

struct dictdb {
    const char *dbname;
    char *basename;
    size_t numwords;
    struct index_entry *index;
};

typedef struct dictorg_data *dictorg_data_t;
