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

struct dico_url dico_url;
struct auth_cred default_cred;
char *client = DICO_CLIENT_ID;
enum dico_client_mode mode = mode_define;
int transcript;
IPADDR source_addr = INADDR_ANY;
int noauth_option;
unsigned levenshtein_threshold;
char *autologin_file;

void
fixup_url()
{
    dico_url.proto = "dict";
    if (!dico_url.host)
	dico_url.host = DEFAULT_DICT_SERVER;
    if (!dico_url.req.database)
	dico_url.req.database = "!";
    if (!dico_url.req.strategy)
	dico_url.req.strategy = ".";
    if (mode == mode_match)
	dico_url.req.type = DICO_REQUEST_MATCH;
}

int
main(int argc, char **argv)
{
    int index, rc = 0;

    dico_set_program_name(argv[0]);
    get_options(argc, argv, &index);
    fixup_url();
    set_quoting_style(NULL, escape_quoting_style);
    signal(SIGPIPE, SIG_IGN);

    argc -= index;
    argv += index;

    switch (mode) {
    case mode_define:
    case mode_match:
	if (!argc) 
	    dico_die(1, L_ERR, 0,
		     _("you should give a word to look for or an URL"));
	break;

    default:
	if (argc)
	    dico_log(L_WARN, 0,
		     _("extra command line arguments ignored"));
    }
    
    switch (mode) {
    case mode_define:
    case mode_match:
	if (!argc) 
	    dico_die(1, L_ERR, 0,
		     _("you should give a word to look for or an URL"));
	while (argc--) 
	    rc |= dict_lookup((*argv)++) != 0;
	break;
	
    case mode_dbs:
	rc = dict_single_command("SHOW DATABASES", NULL, "110");
	break;
	
    case mode_strats:
	rc = dict_single_command("SHOW STRATEGIES", NULL, "111");
	break;
	
    case mode_help:
	rc = dict_single_command("HELP", NULL, "113");
	break;
	
    case mode_info:
	if (!dico_url.req.database)
	    dico_die(1, L_ERR, 0, _("Database name not specified"));
	rc = dict_single_command("SHOW INFO", dico_url.req.database, "112");
	break;
	
    case mode_server:
	rc = dict_single_command("SHOW SERVER", NULL, "114");
	break;
    }	
    
    return rc;
}
