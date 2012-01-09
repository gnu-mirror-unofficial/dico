/* This file is part of GNU Dico.
   Copyright (C) 2010, 2012 Sergey Poznyakoff

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <dico.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <appi18n.h>

static int
word_sel(int cmd, dico_key_t key, const char *dict_word)
{
    int rc = 0;
    char const *key_word = key->word;
    struct dico_tokbuf tb;
    int i;
    
    switch (cmd) {
    case DICO_SELECT_BEGIN:
	break;

    case DICO_SELECT_RUN:
	dico_tokenize_begin(&tb);
	if (dico_tokenize_string(&tb, (char*) dict_word)) {
	    dico_tokenize_end(&tb);
	    return 0;
	}
	for (i = 0; i < tb.tb_tokc; i++)
	    if (utf8_strcasecmp(tb.tb_tokv[i], (char*) key_word) == 0) {
		rc = 1;
		break;
	    }
	dico_tokenize_end(&tb);
	break;

    case DICO_SELECT_END:
	break;
    }
    return rc;
}

static struct dico_strategy word_strat = {
    "word",
    "Match a word anywhere in the headword",
    word_sel
};

static int
word_init(int argc, char **argv)
{
    dico_strategy_add(&word_strat);
    return 0;
}

struct dico_database_module DICO_EXPORT(word, module) = {
    DICO_MODULE_VERSION,
    DICO_CAPA_NODB,
    word_init,
};

	
