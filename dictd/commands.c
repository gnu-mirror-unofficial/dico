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
    struct utf8_iterator itr;
    size_t len = 0;
    
    stream_writez(str, "113 help text follows\r\n");
    for (utf8_iter_first(&itr, (unsigned char *)text);
	 !utf8_iter_end_p(&itr);
	 utf8_iter_next(&itr)) {
	if (utf8_iter_isascii(itr) && *itr.curptr == '\n') {
	    stream_writeln(str, itr.curptr - len, len);
	    len = 0;
	} else
	    len += itr.curwidth;
    }
    if (len)
	stream_writeln(str, itr.curptr - len, len);
    stream_writez(str, "\r\n.\r\n");
    stream_writez(str, "250 ok\r\n");    
}



struct dictd_command command_tab[] = {
    { "DEFINE", 3, 3, },
    { "MATCH", 4, 4, },
    { "SHOW", 2, 3, },
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

