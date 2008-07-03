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

#include <dicod.h>
#include <hash.h>

struct alias {
    char *kw;     /* Keyword */
    int argc;
    char **argv;
    dicod_locus_t locus;
};

static Hash_table *alias_table;

/* Calculate the hash of a struct input_file_ident.  */
static size_t
alias_hasher(void const *data, unsigned n_buckets)
{
    const struct alias *ap = data;
    return hash_string(ap->kw, n_buckets);
}

/* Compare two input_file_idents for equality.  */
static bool
alias_compare(void const *data1, void const *data2)
{
    const struct alias *ap1 = data1;
    const struct alias *ap2 = data2;
    return strcmp(ap1->kw, ap2->kw) == 0;
}

int
alias_install(const char *kw, int argc, char **argv, dicod_locus_t *ploc)
{
    struct alias *sample = xmalloc(sizeof(*sample)),
	         *ap;

    sample->kw = kw;
    sample->argc = argc;
    sample->argv = argv;
    sample->locus = *ploc;
    
    if (! ((alias_table
	    || (alias_table = hash_initialize(0, 0, 
					      alias_hasher,
					      alias_compare,
					      NULL)))
	   && (ap = hash_insert(alias_table, sample))))
	xalloc_die();
    
    if (ap != sample) {
	free(sample);
	config_error(ploc, 0, _("alias `%s' already defined"), sample->kw);
	config_error(&ap->locus, 0,
		     _("this is the location of the previous definition"));
	return 1; /* Found */
    }
    return 0;
}

static int
list_alias_cmp(const void *a, const void *b)
{
    const struct alias *ap1 = a;
    const struct alias *ap2 = b;
    return strcmp(ap1->kw, ap2->kw);
}

int
alias_expand(int argc, char **argv, int *pargc, char ***pargv)
{
    struct alias sample, *ap;
    dico_list_t alist = NULL;
    
    if (!alias_table)
	return 1;
    
    for (sample.kw = argv[0]; (ap = hash_lookup(alias_table, &sample));) {
	if (dico_list_locate(alist, ap, list_alias_cmp))
	    break;
	sample.kw = (char*) ap->argv[0];
	if (!alist)
	    alist = xdico_list_create();
	xdico_list_append(alist, ap);
    }

    if (alist) {
	int pos;
	int nargc;
	char **nargv;
	dico_iterator_t itr = xdico_iterator_create(alist);
	char *kw;
	
	nargc = 1;
	for (ap = dico_iterator_first(itr); ap; ap = dico_iterator_next(itr)) 
	    nargc += ap->argc - 1;

	pos = nargc;
	nargc += argc - 1;
	nargv = xcalloc(nargc + 1, sizeof(nargv[0]));

	memcpy(nargv + pos, argv + 1, argc * sizeof(argv[0]));
	pos--;
	for (ap = dico_iterator_first(itr); ap; ap = dico_iterator_next(itr)) {
	    int i;
	    for (i = ap->argc-1; i > 0; i--)
		nargv[pos--] = ap->argv[i];
	    kw = ap->argv[0];
	}
	nargv[pos] = kw;
	*pargc = nargc;
	*pargv = nargv;
	dico_list_destroy(&alist, NULL, NULL);
	return 0;
    }
    return 1;
}
