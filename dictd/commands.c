/* This file is part of Gjdict.
   Copyright (C) 2008 Sergey Poznyakoff

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

int got_quit;

void
dictd_quit(stream_t str, int argc, char **argv)
{
    got_quit = 1;
    stream_writez(str, "221 bye\r\n");
}

void
dictd_help(stream_t str, int argc, char **argv)
{
    static char default_help_text[] = "\
DEFINE database word         -- look up word in database\n\
MATCH database strategy word -- match word in database using strategy\n\
SHOW DB                      -- list all accessible databases\n\
SHOW DATABASES               -- list all accessible databases\n\
SHOW STRAT                   -- list available matching strategies\n\
SHOW STRATEGIES              -- list available matching strategies\n\
SHOW INFO database           -- provide information about the database\n\
SHOW SERVER                  -- provide site-specific information\n\
OPTION MIME                  -- use MIME headers\n\
CLIENT info                  -- identify client to server\n\
AUTH user string             -- provide authentication information\n\
STATUS                       -- display timing information\n\
HELP                         -- display this help information\n\
QUIT                         -- terminate connection\n";
    char *text = help_text ? help_text : default_help_text;
    
    stream_writez(str, "113 help text follows\r\n");
    stream_write_multiline(str, text);
    stream_writez(str, "\r\n.\r\n");
    stream_writez(str, "250 ok\r\n");    
}

static int
_show_database(void *item, void *data)
{
    dictd_dictionary_t *dict = item;
    stream_t str = data;

    stream_printf(str, "%s \"%s\"\r\n",
		  dict->name, dict->descr); /* FIXME: Quote descr. */
    return 0;
}

void
dictd_show_database_info(stream_t str, const char *dbname)
{
    dictd_dictionary_t *dict = find_dictionary(dbname);
    if (!dict) 
	stream_writez(str, "550 invalid database, use SHOW DB for list\r\n");
    else {
	stream_printf(str, "112 information for %s\r\n", dbname);
	if (dict->info)
	    stream_write_multiline(str, dict->info);
	stream_writez(str, "\r\n.\r\n");
	stream_writez(str, "250 ok\r\n");    
    }
}


void
dictd_show_databases(stream_t str)
{
    size_t count = dict_list_count(dictionary_list);
    stream_printf(str, "110 %lu database(s) configured\r\n",
		  (unsigned long) count);
    dict_list_iterate(dictionary_list, _show_database, str);
    stream_writez(str, ".\r\n");
    stream_writez(str, "250 ok\r\n");    
}

void
dictd_show_server(stream_t str)
{
    stream_writez(str, "114 server information\r\n");
    /* FIXME: (For logged in users) show:
       dictd (gjdict 1.0.90) on Linux 2.6.18, Trurl.gnu.org.ua up 81+01:33:49, 12752570 forks (6554.7/hour)
    */
    stream_write_multiline(str, server_info);
    stream_writez(str, "\r\n.\r\n");
    stream_writez(str, "250 ok\r\n");    
}

void
dictd_show(stream_t str, int argc, char **argv)
{
    if (c_strcasecmp(argv[1], "DB") == 0
	|| c_strcasecmp(argv[1], "DATABASES") == 0) {
	if (argc != 2) {
	    stream_writez(str, "500 wrong number of arguments\r\n");
	    return;
	}
	dictd_show_databases(str);
    } else if (c_strcasecmp(argv[1], "STRAT") == 0
	       || c_strcasecmp(argv[1], "STRATEGIES") == 0) {
	if (argc != 2) {
	    stream_writez(str, "500 wrong number of arguments\r\n");
	    return;
	}
	/* FIXME */
	stream_writez(str, "500 command is not yet implemented, sorry\r\n");
    } else if (c_strcasecmp(argv[1], "INFO") == 0) {
	if (argc != 3) {
	    stream_writez(str, "500 wrong number of arguments\r\n");
	    return;
	}
	dictd_show_database_info(str, argv[2]);
    } else if (c_strcasecmp(argv[1], "SERVER") == 0) {
	if (argc != 2) {
	    stream_writez(str, "500 wrong number of arguments\r\n");
	    return;
	}
	dictd_show_server(str);
    } else
	stream_writez(str, "500 unknown command\r\n");
}


struct dictd_command command_tab[] = {
    { "DEFINE", 3, 3, },
    { "MATCH", 4, 4, },
    { "SHOW", 2, 3, dictd_show },
    { "CLIENT", 2, 2, },
    { "STATUS", 1, 1, },
    { "HELP", 1, 1, dictd_help },
    { "QUIT", 1, 1, dictd_quit },
    { "AUTH", 3, 3, },
    { "SASLAUTH", 3, 3, },
    { NULL }
};

struct dictd_command *
locate_command(const char *kw)
{
    struct dictd_command *p;

    for (p = command_tab; p->keyword; p++)
	if (c_strcasecmp(p->keyword, kw) == 0)
	    return p;
    return NULL;
}

void
dictd_handle_command(stream_t str, int argc, char **argv)
{
    struct dictd_command *cmd = locate_command(argv[0]);
    if (!cmd) 
	stream_writez(str, "500 unknown command\r\n");
    else if (!(cmd->minargs <= argc && argc <= cmd->maxargs)) 
	stream_writez(str, "500 wrong number of arguments\r\n");
    else if (!cmd->handler)
	stream_writez(str, "500 command is not yet implemented, sorry\r\n");
    else
	cmd->handler(str, argc, argv);
}

