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

#ifndef __dico_utf8_h
#define __dico_utf8_h

size_t utf8_char_width(const unsigned char *p);
size_t utf8_strlen (const char *s);
size_t utf8_strbytelen (const char *s);

struct utf8_iterator {
    char *string;
    char *curptr;
    unsigned curwidth;
};

#define utf8_iter_isascii(itr) \
 ((itr).curwidth == 1 && isascii((itr).curptr[0]))

int utf8_iter_end_p(struct utf8_iterator *itr);
int utf8_iter_first(struct utf8_iterator *itr, unsigned char *ptr);
int utf8_iter_next(struct utf8_iterator *itr);

int utf8_mbtowc_internal (void *data, int (*read) (void*), unsigned int *pwc);
int utf8_wctomb (unsigned char *r, unsigned int wc);

int utf8_symcmp(unsigned char *a, unsigned char *b);
int utf8_symcasecmp(unsigned char *a, unsigned char *b);
int utf8_strcasecmp(unsigned char *a, unsigned char *b);
int utf8_strncasecmp(unsigned char *a, unsigned char *b, size_t maxlen);

unsigned utf8_wc_toupper (unsigned wc);
int utf8_toupper (char *s, size_t len);
unsigned utf8_wc_tolower (unsigned wc);
int utf8_tolower (char *s, size_t len);
size_t utf8_wc_strlen (const unsigned *s);
unsigned *utf8_wc_strdup (const unsigned *s);
size_t utf8_wc_hash_string (const unsigned *ws, size_t n_buckets);
int utf8_wc_strcmp (const unsigned *a, const unsigned *b);
int utf8_wc_to_mbstr(const unsigned *wordbuf, size_t wordlen, char *s,
		     size_t size);
int utf8_mbstr_to_wc(const char *str, unsigned **wptr);

int dico_levenshtein_distance(const char *a, const char *b, int damerau);

#endif

