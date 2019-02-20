/* This file is part of GNU Dico
   Copyright (C) 2007-2019 Sergey Poznyakoff
 
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

#include <config.h>
#include <dico.h>    
#include <ctype.h>

static char soundextbl[] = 
"A0"
"B1"
"C2"
"D3"
"E0"
"F1"
"G2"
"H-"
"I0"
"J2"
"K2"
"L4"
"M5"
"N5"
"O0"
"P1"
"Q2"
"R6"
"S2"
"T3"
"U0"
"V1"
"W-"
"X2"
"Y0"
"Z2"
;

static int
soundex_code(int c)
{
    char *p;
    
    c = toupper(c);
    for (p = soundextbl; *p; p += 2)
	if (*p == c)
	    return p[1];
    return 0;
}

int
dico_soundex_ascii(const char *s, char codestr[DICO_SOUNDEX_SIZE])
{
    int i, prev;
    
    codestr[0] = toupper(*s);
    prev = soundex_code(codestr[0]);
    for (i = 1, s++; i < DICO_SOUNDEX_SIZE-1 && *s; s++) {
	int n =  soundex_code(*s);
	if (n) {
	    if (n == prev)
		continue;
	    switch (n) {
	    case '0':
		break;
	    case '-':
		continue;
	    default:
		codestr[i++] = n;
	    }
	    prev = n;
	}
    }
    for (; i < DICO_SOUNDEX_SIZE-1; i++)
	codestr[i] = '0';
    codestr[i] = 0;
    return 0;
}

/* Compute the soundex code for TEXT (a UTF8 string).  Ignore
   non-ASCII characters. */
int
dico_soundex(const char *text, char codestr[DICO_SOUNDEX_SIZE])
{
    unsigned *input, *s;
    int i, prev;

    if (utf8_mbstr_to_wc(text, &input, NULL))
	return -1;
    s = input;
    do {
	codestr[0] = utf8_wc_toupper(*s++);
    } while (codestr[0] > 127 || (prev = soundex_code(codestr[0])) == 0);
    for (i = 1; i < DICO_SOUNDEX_SIZE-1 && *s; s++) {
	int n =  *s < 128 ? soundex_code(*s) : 0;
	if (n) {
	    if (n == prev)
		continue;
	    switch (n) {
	    case '0':
		break;
	    case '-':
		continue;
	    default:
		codestr[i++] = n;
	    }
	    prev = n;
	}
    }
    for (; i < DICO_SOUNDEX_SIZE-1; i++)
	codestr[i] = '0';
    codestr[i] = 0;
    free(input);
    return 0;
}
