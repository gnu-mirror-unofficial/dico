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
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include <obstack.h>

struct input {
    struct obstack stk;
    char *rootptr;
    int argc;
    char **argv;
};

#define ISWS(c) ((c) == ' ' || (c) == '\t')
#define ISQUOTE(c) ((c) == '"' || (c) == '\'')

int
tokenize_input(struct input *in, char *str)
{
    struct utf8_iterator itr;
    int i, argc = 0;
    
    if (!in->rootptr) 
	obstack_init(&in->stk);
    else
	obstack_free(&in->stk, in->rootptr);

    utf8_iter_first(&itr, (unsigned char *)str);

    while (1) {
	int quote;
	
	for (; !utf8_iter_end_p(&itr)
		 && utf8_iter_isascii(itr) && ISWS(*itr.curptr);
	     utf8_iter_next(&itr))
	    ;

	if (utf8_iter_end_p(&itr))
	    break;

	if (utf8_iter_isascii(itr) && ISQUOTE(*itr.curptr)) {
	    quote = *itr.curptr;
	    utf8_iter_next(&itr);
	} else
	    quote = 0;

	for (; !utf8_iter_end_p(&itr)
		 && !(utf8_iter_isascii(itr) && ISWS(*itr.curptr));
	     utf8_iter_next(&itr)) {
	    if (utf8_iter_isascii(itr)) {
		if (*itr.curptr == quote) {
		    utf8_iter_next(&itr);
		    break;
		} else if (*itr.curptr == '\\') {
		    utf8_iter_next(&itr);
		    if (utf8_iter_isascii(itr)) {
			obstack_1grow(&in->stk, quote_char(*itr.curptr));
		    } else {
			obstack_1grow(&in->stk, '\\');
			obstack_grow(&in->stk, itr.curptr, itr.curwidth);
		    }
		    continue;
		}
	    }
	    obstack_grow(&in->stk, itr.curptr, itr.curwidth);
	}
	obstack_1grow(&in->stk, 0);
	argc++;
    }

    in->rootptr = obstack_finish(&in->stk);

    in->argc = argc;
    in->argv = obstack_alloc(&in->stk, (argc + 1) * sizeof(in->argv[0]));

    for (i = 0; i < argc; i++) {
	in->argv[i] = in->rootptr;
	in->rootptr += utf8_strbytelen(in->rootptr) + 1;
    }
    in->argv[i] = NULL;
    return argc;
}

int
stream_writez(stream_t str, char *buf)
{
    return stream_write(str, buf, utf8_strbytelen(buf));
}

int
stream_printf(stream_t str, const char *fmt, ...)
{
    int len;
    char *buf;
    
    va_list ap;
    va_start(ap, fmt);
    len = vasprintf(&buf, fmt, ap);
    va_end(ap);
    if (len < 0) {
	logmsg(L_CRIT, 0,
	       _("not enough memory while formatting reply message"));
	exit(1);
    }
    len = stream_write(str, buf, len);
    free(buf);
    return len;
}

void
stream_write_multiline(stream_t str, const char *text)
{
    struct utf8_iterator itr;
    size_t len = 0;
    
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
}


static void
output_capabilities(stream_t str)
{
    stream_write(str, "<", 1);
    /* FIXME */
    stream_write(str, ">", 1);
}

/*
  3.1.  Initial Connection

   When a client initially connects to a DICT server, a code 220 is sent
   if the client's IP is allowed to connect:

             220 text capabilities msg-id
*/
static void
initial_banner(stream_t str)
{
    stream_write(str, "220 ", 4);
    stream_writez(str, hostname);
    stream_write(str, " ", 1);
    if (initial_banner_text)
	stream_writez(str, initial_banner_text);
    else
	stream_writez(str, (char*) program_version);
    stream_write(str, " ", 1);
    output_capabilities(str);
    stream_write(str, " ", 1);
    stream_printf(str, "<%lu.%lu@%s>",
		  (unsigned long) getpid(),
		  (unsigned long) time(NULL),
		  hostname);
    stream_write(str, "\r\n", 2);
}

static void
trim(char *buf, size_t len)
{
    if (len > 1 && buf[--len] == '\n') {
	buf[len] = 0;
	if (len > 1 && buf[--len] == '\r')
	    buf[len] = 0;
    }
}

int
dictd_loop(stream_t str)
{
    char *buf = NULL;
    size_t size = 0;
    size_t rdbytes;
    struct input input;
    
    memset(&input, 0, sizeof input);
    initial_banner(str);
    got_quit = 0;
    while (!got_quit && stream_getline(str, &buf, &size, &rdbytes) == 0) {
	trim(buf, rdbytes);
	tokenize_input(&input, buf);
	if (input.argc == 0)
	    continue;
	dictd_handle_command(str, input.argc, input.argv);
    }
    
    stream_close(str);
    stream_destroy(&str);
    return 0;
}

int
dictd_inetd()
{
    stream_t str = fd_stream_create(0, 1);
    stream_set_buffer(str, lb_in, 512);
    stream_set_buffer(str, lb_out, 512);
    return dictd_loop(str);
}
