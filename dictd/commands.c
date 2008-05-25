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

#include <dictd.h>

int got_quit;

void
dictd_quit(dico_stream_t str, int argc, char **argv)
{
    got_quit = 1;
    stream_writez(str, "221 bye");
    report_timing(str, "dictd");
    stream_writez(str, "\r\n");
}

void dictd_show_std_help(dico_stream_t str);

void
dictd_help(dico_stream_t str, int argc, char **argv)
{
    const char *text = help_text;
    dico_stream_t ostr;
	
    stream_writez(str, "113 help text follows\r\n");
    ostr = dictd_ostream_create(str, NULL, NULL);
    
    if (text) {
	if (text[0] == '+') {
	    dictd_show_std_help(ostr);
	    text++;
	}
	stream_write_multiline(ostr, text);
    } else
	dictd_show_std_help(ostr);

    stream_writez(ostr, "\r\n");
    dico_stream_close(ostr);
    dico_stream_destroy(&ostr);

    stream_writez(str, ".\r\n");
    stream_writez(str, "250 ok\r\n");    
}

void
dictd_show_info(dico_stream_t str, int argc, char **argv)
{
    char *dbname = argv[2];
    dictd_database_t *dict = find_database(dbname);
    if (!dict) 
	stream_writez(str, "550 invalid database, use SHOW DB for list\r\n");
    else {
	dico_stream_t ostr;
	char *info = dictd_get_database_info(dict);
	stream_printf(str, "112 information for %s\r\n", dbname);
	ostr = dictd_ostream_create(str, NULL, NULL);
	if (info) {
	    stream_write_multiline(ostr, info);
	    dictd_free_database_info(dict, info);
	} else
	    stream_writez(ostr, "No information available.\r\n");
	stream_writez(ostr, "\r\n");
	dico_stream_close(ostr);
	dico_stream_destroy(&ostr);
	
	stream_writez(str, ".\r\n");
	stream_writez(str, "250 ok\r\n");    
    }
}

static int
_show_database(void *item, void *data)
{
    dictd_database_t *dict = item;
    dico_stream_t str = data;
    char *descr = dictd_get_database_descr(dict);
    stream_printf(str, "%s \"%s\"\r\n",
		  dict->name, descr ? descr : ""); /* FIXME: Quote descr. */
    dictd_free_database_descr(dict, descr);
    return 0;
}

void
dictd_show_databases(dico_stream_t str, int argc, char **argv)
{
    size_t count = database_count();
    if (count == 0) 
	stream_printf(str, "554 No databases present\r\n");
    else {
	dico_stream_t ostr;
	
	stream_printf(str, "110 %lu databases present\r\n",
		      (unsigned long) count);
	ostr = dictd_ostream_create(str, NULL, NULL);
	database_iterate(_show_database, ostr);
	dico_stream_close(ostr);
	dico_stream_destroy(&ostr);
	stream_writez(str, ".\r\n");
	stream_writez(str, "250 ok\r\n");
    }
}

static int
_show_strategy(void *item, void *data)
{
    dico_strategy_t sp = item;
    dico_stream_t str = data;

    stream_printf(str, "%s \"%s\"\r\n",
		  sp->name, sp->descr); /* FIXME: Quote descr. */
    return 0;
}

void
dictd_show_strategies(dico_stream_t str, int argc, char **argv)
{
    size_t count = dico_strategy_count();
    if (count == 0)
	stream_printf(str, "555 No strategies available\r\n");
    else {
	dico_stream_t ostr;
	
	stream_printf(str, "111 %lu strategies present: list follows\r\n",
		      (unsigned long) count);
	ostr = dictd_ostream_create(str, NULL, NULL);
	dico_strategy_iterate(_show_strategy, ostr);
	dico_stream_close(ostr);
	dico_stream_destroy(&ostr);
	stream_writez(str, ".\r\n");
	stream_writez(str, "250 ok\r\n");
    }
}
	

void
dictd_show_server(dico_stream_t str, int argc, char **argv)
{
    dico_stream_t ostr;
    
    stream_writez(str, "114 server information\r\n");
    ostr = dictd_ostream_create(str, NULL, NULL);
    /* FIXME: (For logged in users) show:
       dictd (gjdict 1.0.90) on Linux 2.6.18, Trurl.gnu.org.ua up 81+01:33:49, 12752570 forks (6554.7/hour)
    */
    stream_write_multiline(ostr, server_info);
    stream_writez(ostr, "\r\n");
    dico_stream_close(ostr);
    dico_stream_destroy(&ostr);
    stream_writez(str, ".\r\n");
    stream_writez(str, "250 ok\r\n");    
}

void
dictd_client(dico_stream_t str, int argc, char **argv)
{
    dico_log(L_INFO, 0, "Client info: %s", argv[1]);
    stream_writez(str, "250 ok\r\n");
}

void
dictd_match(dico_stream_t str, int argc, char **argv)
{
    const char *dbname = argv[1];
    const char *word = argv[3];
    const dico_strategy_t strat = dico_strategy_find(argv[2]);
    
    if (!strat) 
	stream_writez(str,
		      "551 Invalid strategy, use \"SHOW STRAT\" "
		      "for a list of strategies\r\n");
    else if (strcmp(dbname, "!") == 0) 
	dictd_match_word_first(str, strat, word);
    else if (strcmp(dbname, "*") == 0) 
	dictd_match_word_all(str, strat, word);
    else {
	dictd_database_t *db = find_database(dbname);
    
	if (!db) 
	    stream_writez(str,
			  "550 invalid database, use SHOW DB for list\r\n");
	else
	    dictd_match_word_db(db, str, strat, word);
    }
}

void
dictd_define(dico_stream_t str, int argc, char **argv)
{
    char *dbname = argv[1];
    char *word = argv[2];
    
    if (strcmp(dbname, "!") == 0) {
	dictd_define_word_first(str, word);
    } else if (strcmp(dbname, "*") == 0) {
	dictd_define_word_all(str, word);
    } else {   
	dictd_database_t *db = find_database(dbname);
    
	if (!db) 
	    stream_writez(str,
			  "550 invalid database, use SHOW DB for list\r\n");
	else
	    dictd_define_word_db(db, str, word);
    }
}


struct dictd_command command_tab[] = {
    { "DEFINE", 3, "database word", "look up word in database",
      dictd_define },
    { "MATCH", 4, "database strategy word",
      "match word in database using strategy",
      dictd_match },
    { "SHOW DB", 2, NULL, "list all accessible databases",
      dictd_show_databases, },
    { "SHOW DATABASES", 2, NULL, "list all accessible databases",
      dictd_show_databases, },
    { "SHOW STRAT", 2, NULL, "list available matching strategies",
      dictd_show_strategies },
    { "SHOW STRATEGIES", 2, NULL, "list available matching strategies",
      dictd_show_strategies  },
    { "SHOW INFO", 3, "database", "provide information about the database",
      dictd_show_info },
    { "SHOW SERVER", 2, NULL, "provide site-specific information",
      dictd_show_server },
    { "CLIENT", 2, "info", "identify client to server",
      dictd_client },
    { "STATUS", 1, NULL, "display timing information" },
    { "HELP", 1, NULL, "display this help information",
      dictd_help },
    { "QUIT", 1, NULL, "terminate connection", dictd_quit },
    { NULL }
};

dico_list_t /* of struct dictd_command */ command_list;

void
dictd_add_command(struct dictd_command *cmd)
{
    if (!command_list)
	command_list = xdico_list_create();
    xdico_list_append(command_list, cmd);
}

void
dictd_init_command_tab()
{
    struct dictd_command *p;
    
    for (p = command_tab; p->keyword; p++) 
	dictd_add_command(p);
}

static int
_print_help(void *item, void *data)
{
    struct dictd_command *p = item;
    dico_stream_t str = data;
    int len = strlen(p->keyword);

    stream_writez(str, p->keyword);
    if (p->param) {
	stream_printf(str, " %s", p->param);
	len += strlen(p->param) + 1;
    }
    
    if (len < 31)
	len = 31 - len;
    else
	len = 0;
    stream_printf(str, "%*.*s -- %s\r\n", len, len, "", p->help);
    return 0;
}

void
dictd_show_std_help(dico_stream_t str)
{
    dico_list_iterate(command_list, _print_help, str);
}


struct locate_data {
    int argc;
    char **argv;
};

static int
_cmd_arg_cmp(const void *item, const void *data)
{
    const struct dictd_command *p = item;
    const struct locate_data *datptr = data;
    int i, off = 0;

    for (i = 0; i < datptr->argc; i++) {
	int len = strlen(datptr->argv[i]);
	if (c_strncasecmp(p->keyword + off, datptr->argv[i], len) == 0) {
	    off += len;
	    if (p->keyword[off] == 0) 
		return 0;
	    if (p->keyword[off] == ' ') {
		off++;
		continue;
	    } else
		break;
	}
    }
    return 1;
}
    

struct dictd_command *
locate_command(int argc, char **argv)
{
    struct locate_data ld;
    ld.argc = argc;
    ld.argv = argv;
    return dico_list_locate(command_list, &ld, _cmd_arg_cmp);
}

void
dictd_handle_command(dico_stream_t str, int argc, char **argv)
{
    struct dictd_command *cmd = locate_command(argc, argv);
    if (!cmd) 
	stream_writez(str, "500 unknown command\r\n");
    else if (argc != cmd->nparam) 
	stream_writez(str, "501 wrong number of arguments\r\n");
    else if (!cmd->handler)
	stream_writez(str, "502 command is not yet implemented, sorry\r\n");
    else
	cmd->handler(str, argc, argv);
}

