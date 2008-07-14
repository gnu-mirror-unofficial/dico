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

char *host;
int port;
char *database = "!";
char *strategy = ".";
char *user;
char *key;
char *client = DICO_CLIENT_ID;
struct dico_request req;
enum dico_client_mode mode = mode_define;

int
dico_lookup(const char *word)
{
    dico_url_t url;
    if (dico_url_parse(&url, word))
	return 1;
    printf("URL: %s\n", url->string);
    printf("PROTO: %s\n", url->proto);
    printf("HOST: %s\n", url->host);
    printf("PORT: %d\n", url->port);
    printf("PATH: %s\n", url->path);
    printf("USER: %s\n", url->user);
    printf("PASS: %s\n", url->passwd);
    
    if (memcmp(url->proto, "dict", 4) == 0) {
	printf("Request:\n");
	printf("TYPE: %d\n", url->req.type);
	printf("WORD: %s\n", url->req.word);
	printf("DB: %s\n", url->req.database);
	printf("STRAT: %s\n", url->req.strategy);
	printf("N: %d\n", url->req.n);
    }
    
    if (url->args) {
	dico_iterator_t itr = xdico_iterator_create(url->args);
	struct dico_assoc *pa;
	for (pa = dico_iterator_first(itr); pa;
	     pa = dico_iterator_next(itr))
	    printf(" %s=%s\n", pa->key, pa->value);
	dico_iterator_destroy(&itr);
    }
    
    return 0;
}

int
main(int argc, char **argv)
{
    int index, rc;

    dico_set_program_name(argv[0]);
    get_options(argc, argv, &index);
    argc -= index;
    argv += index;
    if (!argc) 
	dico_die(1, L_ERR, 0,
		 _("you should give a word to look for or an URL"));
    while (argc--) 
	rc |= dico_lookup((*argv)++) != 0;
    return rc;
}
