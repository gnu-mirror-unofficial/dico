/* This file is part of GNU Dico
   Copyright (C) 2008 Sergey Poznyakoff
  
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
#include <xdico.h>
#include <xalloc.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include <obstack.h>

struct xdico_input {
    struct obstack stk;
    char *rootptr;
    int argc;
    char **argv;
};

#define ISWS(c) ((c) == ' ' || (c) == '\t')
#define ISQUOTE(c) ((c) == '"' || (c) == '\'')

static char quote_transtab[] = "\\\\\"\"a\ab\bf\fn\nr\rt\t";

int
xdico_unquote_char(int c)
{
    char *p;

    for (p = quote_transtab; *p; p += 2) {
	if (*p == c)
	    return p[1];
    }
    return 0;
}

int
xdico_quote_char(int c)
{
    char *p;

    for (p = quote_transtab; *p; p += 2) {
	if (p[1] == c)
	    return p[0];
    }
    return 0;
}

xdico_input_t
xdico_tokenize_begin()
{
    return xzalloc(sizeof(struct xdico_input));
}

void
xdico_tokenize_end(xdico_input_t *pin)
{
    xdico_input_t in = *pin;
    obstack_free(&in->stk, NULL);
    free(in);
    *pin = NULL;
}

int
xdico_tokenize_input(xdico_input_t in, char *str, int *pargc, char ***pargv)
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
		 && !(utf8_iter_isascii(itr) && (quote ? 0 : ISWS(*itr.curptr)));
	     utf8_iter_next(&itr)) {
	    if (utf8_iter_isascii(itr)) {
		if (*itr.curptr == quote) {
		    utf8_iter_next(&itr);
		    break;
		} else if (*itr.curptr == '\\') {
		    utf8_iter_next(&itr);
		    if (utf8_iter_isascii(itr)) {
			obstack_1grow(&in->stk, xdico_quote_char(*itr.curptr));
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
	in->rootptr += strlen(in->rootptr) + 1;
    }
    in->argv[i] = NULL;
    *pargc = in->argc;
    *pargv = in->argv;
    return argc;
}
