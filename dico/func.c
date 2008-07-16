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

static struct dict_connection *conn;

void
ds_silent_close()
{
    if (conn) {
	stream_printf(conn->str, "QUIT\r\n");
	dict_read_reply(conn);
	dict_conn_close(conn);
	conn = NULL;
    }
}    

void
ds_open(int argc, char **argv)
{
    if (argc > 1) {
	if (argc == 3) {
	    int n = str2port(argv[2]);
	    if (n == -1) {
		script_error(0, _("Invalid port number or service name"));
		return;
	    }
	    dico_url.port = n;
	}
	xdico_assign_string(&dico_url.host, argv[1]);
    }

    if (!dico_url.host) {
	script_error(0, _("Please specify server name or IP address"));
	return;
    }
    
    ds_silent_close();

    if (dict_connect(&conn, &dico_url)) 
	script_error(0, _("Cannot connect to the server"));
}

void
ds_close(int argc, char **argv)
{
    if (!conn) 
	script_error(0, _("Nothing to close"));
    else 
	ds_silent_close();
}
	
void
ds_autologin(int argc, char **argv)
{
    if (argc == 1) {
	if (!autologin_file)
	    printf("%s\n", _("No autologin file."));
	else
	    printf("%s\n", autologin_file);
    } else 
	xdico_assign_string(&autologin_file, argv[1]);
}

void
ds_database(int argc, char **argv)
{
    if (argc == 1) {
	printf("%s\n", dico_url.req.database ? dico_url.req.database : "!");
    } else
	/* FIXME: Check if such database is available?*/
	xdico_assign_string(&dico_url.req.database, argv[1]);
}

void
ds_strategy(int argc, char **argv)
{
    static char *str;
    if (argc == 1) {
	printf("%s\n", dico_url.req.strategy);
    } else
	/* FIXME: Check if such strategy is available?*/
	xdico_assign_string(&dico_url.req.strategy, argv[1]);
}

void
set_bool(int *pval, char *str)
{
    if (strcmp(str, "yes") == 0
	|| strcmp(str, "on") == 0
	|| strcmp(str, "true") == 0)
	*pval = 1;
    else if (strcmp(str, "no") == 0
	     || strcmp(str, "off") == 0
	     || strcmp(str, "false") == 0)
	*pval = 0;
    else
	script_error(0, _("Expected boolean value"));
}

void
ds_transcript(int argc, char **argv)
{
    if (argc == 1) 
	printf(_("transcript is %s\n"), transcript ? _("on") : _("off"));
    else
	set_bool(&transcript, argv[1]);
}
