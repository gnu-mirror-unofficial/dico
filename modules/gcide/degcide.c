/* This file is part of GNU Dico.
   Copyright (C) 2012 Sergey Poznyakoff

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
#include <unistd.h>
#include <getopt.h>    
#include <stdio.h>
#include <stdlib.h>    
#include <errno.h>
#include <sysexits.h>
#include "gcide.h"

static void
usage(FILE *fp)
{
    fprintf(fp, "usage: %s [-dhs] FILE OFF SIZE\n", dico_program_name);
}    

struct print_closure {
    int level;
};

static int
print_tag(int end, struct gcide_tag *tag, void *data)
{
    size_t i;
    struct print_closure *clos = data;

    if (end) {
	clos->level--;
	printf("%*.*s", 2*clos->level,2*clos->level, "");
	printf("END");
	if (tag->tag_parmc)
	    printf(" %s", tag->tag_name);
	putchar('\n');
	return 0;
    }
    printf("%*.*s", 2*clos->level,2*clos->level, "");
    switch (tag->tag_type) {
    case gcide_content_unspecified:
	break;
    case gcide_content_text:
	printf("TEXT");
	for (i = 0; i < tag->tag_parmc; i++)
	    printf(" %s", tag->tag_parmv[i]);
	printf(":\n%s\n%*.*sENDTEXT", tag->tag_v.text,
	       2*clos->level, 2*clos->level, "");
	if (tag->tag_parmc)
	    printf(" %s", tag->tag_name);
	putchar('\n');
	break;
    case gcide_content_taglist:
	printf("BEGIN");
	for (i = 0; i < tag->tag_parmc; i++)
	    printf(" %s", tag->tag_parmv[i]);
	putchar('\n');
	clos->level++;
	break;
    }
    return 0;
}

struct print_hint {
    int as;
};

static int
print_text(int end, struct gcide_tag *tag, void *data)
{
    struct print_hint *hint = data;
    
    switch (tag->tag_type) {
    case gcide_content_unspecified:
	break;
    case gcide_content_text:
	if (hint->as) {
	    char *s = tag->tag_v.text;

	    if (strncmp(s, "as", 2) == 0 &&
		(isspace(s[3]) || ispunct(s[3]))) {

		fwrite(s, 3, 1, stdout);
		for (s += 3; *s && isspace(*s); s++)
		    putchar(*s);
		printf("``%s", s);
	    } else
		printf("``");
	} else
	    printf("%s", tag->tag_v.text);
	break;
    case gcide_content_taglist:
	if (tag->tag_parmc) {
	    hint->as = 0;
	    if (end) {
		if (strcmp(tag->tag_name, "as") == 0)
		    printf("''");
	    } else {
		if (strcmp(tag->tag_name, "sn") == 0)
		    putchar('\n');
		else if (strcmp(tag->tag_name, "as") == 0)
		    hint->as = 1;
	    }
	}
    }
    return 0;
}

int
main(int argc, char **argv)
{
    int c;
    char *file;
    unsigned long offset;
    unsigned long size;
    FILE *fp;
    char *textbuf;
    struct gcide_parse_tree *tree;
    struct print_closure clos;
    int show_struct = 0;
    
    dico_set_program_name(argv[0]);

    while ((c = getopt(argc, argv, "dhs")) != -1) {
	switch (c) {
	case 'd':
	    gcide_markup_debug = 1;
	    break;

	case 'h':
	    usage(stdout);
	    exit(0);

	case 's':
	    show_struct = 1;
	    break;
	    
	default:
	    exit(EX_USAGE);
	}
    }

    argc -= optind;
    argv += optind;
    if (argc != 3) {
	usage(stderr);
	exit(EX_USAGE);
    }

    file = argv[0];
    offset = atoi(argv[1]);
    size = atoi(argv[2]);

    textbuf = malloc(size);
    if (!textbuf) {
	dico_log(L_ERR, 0, "not enough memory");
	exit(EX_UNAVAILABLE);
    }
    
    fp = fopen(file, "r");
    if (!fp) {
	dico_log(L_ERR, errno, "cannot open file %s", file);
	exit(EX_UNAVAILABLE);
    }

    if (fseek(fp, offset, SEEK_SET)) {
	dico_log(L_ERR, errno, "%s", file);
	exit(EX_UNAVAILABLE);
    }

    if (fread(textbuf, size, 1, fp) != 1) {
	dico_log(L_ERR, errno, "%s: read error", file);
	exit(EX_UNAVAILABLE);
    }
	
    tree = gcide_markup_parse(textbuf, size);
    if (!tree)
	exit(EX_UNAVAILABLE);

    clos.level = 0;
    gcide_parse_tree_inorder(tree, show_struct ? print_tag : print_text, &clos);
    
    gcide_parse_tree_free(tree);
    exit(0);
}
