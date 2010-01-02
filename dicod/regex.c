/* This file is part of GNU Dico.
   Copyright (C) 2008, 2010 Sergey Poznyakoff

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
#include <regex.h>

struct regex_closure {
    int flags;
    regex_t reg;
};

static int
regex_sel(int cmd, const char *word, const char *dict_word, void *closure)
{
    struct regex_closure *rp = closure;
    int rc;

    switch (cmd) {
    case DICO_SELECT_BEGIN:
	rc = regcomp(&rp->reg, word, rp->flags);
	if (rc) {
	    char errbuf[512];
	    regerror(rc, &rp->reg, errbuf, sizeof(errbuf));
	    dico_log(L_ERR, 0, _("Regex error: %s"), errbuf);
	}
	break;

    case DICO_SELECT_RUN:
	rc = regexec(&rp->reg, dict_word, 0, NULL, 0) == 0;
	break;

    case DICO_SELECT_END:
	rc = 0;
	regfree(&rp->reg);
	break;
    }
    return rc;
}

static struct regex_closure ext_closure = {
    REG_EXTENDED|REG_ICASE
};

static struct regex_closure basic_closure = {
    REG_EXTENDED|REG_ICASE
};

static struct dico_strategy re_strat = {
    "re",
    "POSIX 1003.2 (modern) regular expressions",
    regex_sel,
    &ext_closure
};

static struct dico_strategy regex_strat = {
    "regexp",
    "Old (basic) regular expressions",
    regex_sel,
    &basic_closure
};

void
register_regex()
{
    dico_strategy_add(&re_strat);
    dico_strategy_add(&regex_strat);
}

    
