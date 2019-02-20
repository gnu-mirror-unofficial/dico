/* This file is part of GNU Dico.
   Copyright (C) 2008-2019 Sergey Poznyakoff

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
#include <appi18n.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#define METAPH_SEGM_SIZE 16

struct metaph_segment {
    char segm[METAPH_SEGM_SIZE];
    struct metaph_segment *next;
};

struct metaph_code {
    size_t length;
    size_t nsegm;
    struct metaph_segment *segm_head, *segm_tail;
};

static size_t
metaph_code_length(struct metaph_code *code)
{
    return code ? code->length : 0;
}

static void
metaph_code_trim(struct metaph_code *code, size_t length)
{
    if (code && code->length > length)
	code->length = length;
}

static struct metaph_code *
metaph_code_create(void)
{
    struct metaph_code *code = malloc(sizeof *code);
    if (code) {
	code->length = 0;
	code->nsegm = 0;
	code->segm_head = code->segm_tail = NULL;
    }
    return code;
}

static struct metaph_segment *
metaph_code_alloc_segment(struct metaph_code *code)
{
    struct metaph_segment *newseg = malloc(sizeof newseg[0]);
    if (!newseg)
	return NULL;
    newseg->next = NULL;
    memset(newseg->segm, 0, METAPH_SEGM_SIZE);
    if (code->segm_tail)
	code->segm_tail->next = newseg;
    else
	code->segm_head = newseg;
    code->segm_tail = newseg;
    code->nsegm++;
    return newseg;
}

static struct metaph_code *
metaph_code_dup(struct metaph_code *src)
{
    struct metaph_code *dst = metaph_code_create();
    struct metaph_segment *src_segm, *dst_segm;
    if (!dst)
	return NULL;
    for (src_segm = src->segm_head; src_segm; src_segm = src_segm->next) {
	dst_segm = metaph_code_alloc_segment(dst);
	memcpy(dst_segm->segm, src_segm->segm, METAPH_SEGM_SIZE);
    }
    dst->length = src->length;
    return dst;
}

static void
metaph_code_free(struct metaph_code *code)
{
    if (code) {
	struct metaph_segment *s = code->segm_head;

	while (s) {
	    struct metaph_segment *next = s->next;
	    free(s);
	    s = next;
	}
	free(code);
    }
}

static int
metaph_code_eq(struct metaph_code *a, struct metaph_code *b)
{
    struct metaph_segment *sa, *sb;
    size_t length;

    if (a == NULL || b == NULL)
	return 0;
    
    if (a->length != b->length)
	return 0;
    length = a->length;
    sa = a->segm_head;
    sb = b->segm_head;
    while (length) {
	size_t segm_length = length < METAPH_SEGM_SIZE
	                        ? length : METAPH_SEGM_SIZE;
	if (memcmp(sa->segm, sb->segm, segm_length))
	    return 0;
	length -= segm_length;
	sa = sa->next;
	sb = sb->next;
    }
    return 1;
}

static int
metaph_code_get_buffer(struct metaph_code *code,
		       char **ret_ptr, size_t *ret_size)
{
    size_t s = code->nsegm * METAPH_SEGM_SIZE - code->length;
    if (s == 0) {
	if (!metaph_code_alloc_segment(code))
	    return -1;
	s = METAPH_SEGM_SIZE;
    }
    *ret_ptr = code->segm_tail->segm + code->length % METAPH_SEGM_SIZE;
    *ret_size = s;
    return 0;
}

static int
metaph_code_add(struct metaph_code *code, char const *str)
{
    if (str) {
	size_t length = strlen(str);

	while (length) {
	    char *p;
	    size_t n;
	    
	    if (metaph_code_get_buffer(code, &p, &n))
		return -1;
	    if (n > length)
		n = length;
	    memcpy(p, str, n);
	    str += n;
	    code->length += n;
	    length -= n;
	}
    }
    return 0;
}

static void
metaph_code_dump(struct metaph_code *code)
{
    if (!code)
	printf("%s\n", "NULL");
    else {
	struct metaph_segment *s;
	size_t length;
	
	printf("length = %zu\n", code->length);
	printf("nsegm = %zu\n", code->nsegm);
	length = code->length;
	for (s = code->segm_head; s; s = s->next) {
	    size_t i;
	    
	    putchar('\'');
	    for (i = 0; length > 0 && i < METAPH_SEGM_SIZE; i++, length--)
		putchar(s->segm[i]);
	    printf("'\n");
	}
    }
}    

static void
metaph_code_print(struct metaph_code *code)
{
    if (!code)
	printf("%s\n", "NULL");
    else {
	struct metaph_segment *s;
	size_t length;
	
	length = code->length;
	for (s = code->segm_head; s; s = s->next) {
	    size_t i;
            for (i = 0; length > 0 && i < METAPH_SEGM_SIZE; i++, length--)
		putchar(s->segm[i]);
	}
    }
}    

typedef struct metaph_code *double_metaphone_code[2];

static int
double_metaphone_add(double_metaphone_code code,
		     char const *primary,
		     char const *secondary)
{
    if (secondary) {
	if (!code[1]) {
	    code[1] = metaph_code_dup(code[0]);
	    if (!code[1]) {
                DICO_LOG_MEMERR();
		return -1;
	    }
	}
        if (metaph_code_add(code[1], secondary))
	    return -1;
    } else if (code[1]) {
        if (metaph_code_add(code[1], primary))
            return -1;
    }
    metaph_code_add(code[0], primary);
    return 0;
}

/* Return true if POS + OFF is within the STR and the substring at
   STR[POS + OFF] matches one of the alternatives in PAT.  Alternatives
   are delimited by | characters.
*/
static int
looking_at(unsigned const *str, size_t pos, int off, char const *pat)
{
    size_t i;

    if (off < 0 && pos < -off)
	return 0;
    pos += off;
    i = pos;
    while (*pat) {
	if (*pat == str[i]) {
	    ++i;
	    ++pat;
	    if (!*pat || *pat == '|')
		return 1;
	} else {
	    /* reset */
	    while (*pat && *pat != '|')
		++pat;
	    if (*pat)
		++pat;
	    i = pos;
	}
    }
    return 0;
}

static void
double_metaphone_free(double_metaphone_code code)
{
    metaph_code_free(code[0]);
    metaph_code_free(code[1]);
}

#define ISVOWEL(s,p,i) looking_at(s, p, i, "A|E|I|O|U|Y")

static int
is_slavo_germanic(unsigned const *str)
{
    static unsigned pat[] = {
	'W', 0,
	'K', 0,
	'C', 'Z', 0,
	'W', 'I', 'T', 'Z', 0,
	0
    };
    int i;
    
    for (i = 0; pat[i]; i++) {
	if (utf8_wc_strstr(str, pat))
	    return 1;
	while (pat[i])
	    ++i;
    }
    return 0;
}

static int
double_metaphone_encode(double_metaphone_code code,
			char const *str, size_t max_length)
{
    unsigned *buf;          /* STR converted to UTF */
    size_t current = 0;     /* Current position */
    size_t length, last;
#define DMETAPH_ADD(c,a,b)			\
    do {					\
	if (double_metaphone_add(c,a,b))	\
	    goto err;				\
    } while (0)
    int slavo_germanic = -1; 
#define IS_SLAVO_GERMANIC()						\
    ((slavo_germanic == -1)						\
     ? (slavo_germanic = is_slavo_germanic(buf)) : slavo_germanic)
    
    if (utf8_mbstr_to_wc(str, &buf, NULL)) {
	dico_log(L_ERR, errno, "%s: cannot convert \"%s\"", __func__, str);
	return -1;
    }
    length = utf8_wc_strlen(buf);
    last = length - 1;
    
    code[0] = metaph_code_create();
    if (!code[0]) {
	dico_log(L_ERR, 0, _("%s: not enough memory"), __func__);
	free(buf);
	return -1;
    }
    code[1] = NULL;

    utf8_wc_strupper(buf);
    
    current = 0;
    if (looking_at(buf, current, 0, "GN|KN|PN|WR|PS"))
	++current;

    while (current < length
	   && (max_length == 0
	       || metaph_code_length(code[0]) < max_length
	       || metaph_code_length(code[1]) < max_length)) {
	switch (buf[current]) {
	case 'A':
	case 'E':
	case 'I':
	case 'O':
	case 'U':
	case 'Y':
	    if (current == 0)
		/* Map initial vowel to A */
		DMETAPH_ADD(code, "A", NULL);
	    current++;
	    break;

	case 'B':
	    DMETAPH_ADD(code, "P", NULL);
	    ++current;
	    if (buf[current] == 'B')
		++current;
	    break;

	case L'Ç':
	    DMETAPH_ADD(code, "S", NULL);
	    ++current;
	    break;
	    
	case 'C':
	    /* Various Germanic */
	    if (current > 1
		&& !ISVOWEL(buf, current, -2) 
		&& looking_at(buf, current, -1, "ACH") 
		&& buf[current + 2] != 'I'
		&& (buf[current + 2] != 'E'
		    || looking_at(buf, current, -2, "BACHER|MACHER"))) {       
		DMETAPH_ADD(code, "K", NULL);
		current += 2;
		break;
	    }

	    /* Special case 'caesar' */
	    if (current == 0 && looking_at(buf, current, 0, "CAESAR")) {
		DMETAPH_ADD(code, "S", NULL);
		current += 2;
		break;
	    }

	    /* Italian 'chianti' */
	    if (looking_at(buf, current, 0, "CHIA")) {
		DMETAPH_ADD(code, "K", NULL);
		current += 2;
		break;
	    }

	    if (looking_at(buf, current, 0, "CH")) {       
		/* 'michael'? */
		if (current > 0 && looking_at(buf, current, 0, "CHAE")) {
		    DMETAPH_ADD(code, "K", "X");
		    current += 2;
		    break;
		}

		/* Greek roots e.g. 'chemistry', 'chorus' */
		if (current == 0
		    && (looking_at(buf, current, 1,
				   "HARAC|HARIS|HOR|HYM|HIA|HEM")) 
		    && !looking_at(buf, 0, 0, "CHORE")) {
		    DMETAPH_ADD(code, "K", NULL);
		    current += 2;
		    break;
		}

		/* Germanic, greek, or otherwise 'ch' for 'kh' sound */
		if ((looking_at(buf, 0, 0, "VAN |VON ")
		     || looking_at(buf, 0, 0, "SCH"))
		    /* 'architect but not 'arch', 'orchestra', 'orchid' */
		    || looking_at(buf, current, -2, "ORCHES|ARCHIT|ORCHID")
		    || looking_at(buf, current, +2, "T|S")
		    || ((looking_at(buf, current, -1, "A|O|U|E") || current == 0)
			/* e.g., 'wachtler', 'wechsler', but not 'tichner' */
			&& looking_at(buf, current, +2,
				      "L|R|N|M|B|H|F|V|W| "))) {
		    DMETAPH_ADD(code, "K", NULL);
		} else {  
		    if (current > 0) {
			if (looking_at(buf, 0, 0, "MC"))
			    /* e.g., "McHugh" */
			    DMETAPH_ADD(code, "K", NULL);
			else
			    DMETAPH_ADD(code, "X", "K");
		    } else
			DMETAPH_ADD(code, "X", NULL);
		}
		current += 2;
		break;
	    }
	    /* E.g, 'czerny' */
	    if (looking_at(buf, current, 0, "CZ")
		&& !looking_at(buf, current, -2, "WICZ")) {
		DMETAPH_ADD(code, "S", "X");
		current += 2;
		break;
	    }

	    /* E.g., 'focaccia' */
	    if (looking_at(buf, current, +1, "CIA")) {
		DMETAPH_ADD(code, "X", NULL);
		current += 3;
		break;
	    }

	    /* Double 'C', but not if e.g. 'McClellan' */
	    if (looking_at(buf, current, 0, "CC") &&
		!(current == 1 && buf[0] == 'M')) {
		/* 'bellocchio' but not 'bacchus' */
		if (looking_at(buf, current, +2, "I|E|H")
		    && !looking_at(buf, current, +2, "HU")) {
		    /* 'accident', 'accede' 'succeed' */
		    if ((current == 1 && buf[current - 1] == 'A') 
			|| looking_at(buf, current, -1, "UCCEE|UCCES"))
			DMETAPH_ADD(code, "KS", NULL);
		    /* 'bacci', 'bertucci', other italian */
		    else
			DMETAPH_ADD(code, "X", NULL);
		    current += 3;
		    break;
		} else {
		    /* Pierce's rule */
		    DMETAPH_ADD(code, "K", NULL);
		    current += 2;
		    break;
		}
	    }
	    
	    if (looking_at(buf, current, 0, "CK|CG|CQ")) {
		DMETAPH_ADD(code, "K", NULL);
		current += 2;
		break;
	    }

	    if (looking_at(buf, current, 0, "CI|CE|CY")) {
		/* Italian vs. english */
		if (looking_at(buf, current, 0, "CIO|CIE|CIA"))
		    DMETAPH_ADD(code, "S", "X");
		else
		    DMETAPH_ADD(code, "S", NULL);
		current += 2;
		break;
	    }

	    DMETAPH_ADD(code, "K", NULL);
                                
	    /* Name sent in 'mac caffrey', 'mac gregor' */
	    if (looking_at(buf, current, +1, " C| Q| G"))
		current += 3;
	    else
		if (looking_at(buf, current, +1, "C|K|Q") 
		    && !looking_at(buf, current, +1, "CE|CI"))
		    current += 2;
		else
		    current++;
	    break;

	case 'D':
	    if (looking_at(buf, current, 0, "DG")) {
		if (looking_at(buf, current, +2, "I|E|Y")) {
		    /* E.g. 'edge' */
		    DMETAPH_ADD(code, "J", NULL);
		    current += 3;
		    break;
		} else {
		    /* E.g. 'edgar' */
		    DMETAPH_ADD(code, "TK", NULL);
		    current += 2;
		    break;
		}
	    }
	    
	    if (looking_at(buf, current, 0, "DT|DD")) {
		DMETAPH_ADD(code, "T", NULL);
		current += 2;
		break;
	    }
	    DMETAPH_ADD(code, "T", NULL);
	    current++;
	    break;

	case 'F':
	    ++current;
	    if (buf[current] == 'F')
		++current;
	    DMETAPH_ADD(code, "F", NULL);
	    break;

	case 'G':
	    if (buf[current + 1] == 'H') {
		if ((current > 0) && !ISVOWEL(buf, current, -1)) {
		    DMETAPH_ADD(code, "K", NULL);
		    current += 2;
		    break;
		}

		if (current < 3) {
		    /* 'ghislane', 'ghiradelli' */
		    if (current == 0) {
			if (buf[current + 2] == 'I')
			    DMETAPH_ADD(code, "J", NULL);
			else
			    DMETAPH_ADD(code, "K", NULL);
			current += 2;
			break;
		    }
		}
		/* Parker's rule (with some further refinements) - e.g., 'hugh'
		 */
		if ((current > 1
		     && looking_at(buf, current, -2, "B|H|D"))
		    /* e.g., 'bough' */
		    || (current > 2 && looking_at(buf, current, -3, "B|H|D"))
		    /* e.g., 'broughton' */
		    || (current > 3 && looking_at(buf, current, -4, "B|H"))) {
		    current += 2;
		    break;
		} else {
		    /* e.g., 'laugh', 'McLaughlin', 'cough', 'gough', 'rough',
		       'tough'
		    */
		    if (current > 2
			&& buf[current - 1] == 'U'
			&& looking_at(buf, current, -3, "C|G|L|R|T")) {
			DMETAPH_ADD(code, "F", NULL);
		    } else if (current > 0 && buf[current - 1] != 'I')
			DMETAPH_ADD(code, "K", NULL);

		    current += 2;
		    break;
		}
	    }

	    if (buf[current + 1] == 'N') {
		if (current == 1 && ISVOWEL(buf, 0, 0) && !IS_SLAVO_GERMANIC()) {
		    DMETAPH_ADD(code, "KN", "N");
		} else
		    /* not e.g. 'cagney' */
		    if (!looking_at(buf, current, +2, "EY")
			&& (buf[current + 1] != 'Y') && !IS_SLAVO_GERMANIC()) {
			DMETAPH_ADD(code, "N", "KN");
		    } else
			DMETAPH_ADD(code, "KN", NULL);
		current += 2;
		break;
	    }
	    /* 'tagliaro' */
	    if (looking_at(buf, current, +1, "LI") && !IS_SLAVO_GERMANIC()) {
		DMETAPH_ADD(code, "KL", "L");
		current += 2;
		break;
	    }
	    /* -ges-,-gep-,-gel-, -gie- at the beginning */
	    if (current == 0
		&& (buf[current + 1] == 'Y'
		    || looking_at(buf, current, +1, 
				  "ES|EP|EB|EL|EY|IB|IL|IN|IE|EI|ER"))) {
		DMETAPH_ADD(code, "K", "J");
		current += 2;
		break;
	    }
	    /* -ger-,  -gy- */
	    if ((looking_at(buf, current, +1, "ER")
		 || (buf[current + 1] == 'Y'))
		&& !looking_at(buf, 0, 0, "DANGER|RANGER|MANGER")
		&& !looking_at(buf, current, -1, "E|I|RGY|OGY")) {
		DMETAPH_ADD(code, "K", "J");
		current += 2;
		break;
	    }
	    /* italian e.g, 'biaggi' */
	    if (looking_at(buf, current, +1, "E|I|Y")
		|| looking_at(buf, current, -1, "AGGI|OGGI")) {
		/* obvious germanic */
		if ((looking_at(buf, 0, 0, "VAN |VON ")
		     || looking_at(buf, 0, 0, "SCH"))
		    || looking_at(buf, current, +1, "ET"))
		    DMETAPH_ADD(code, "K", NULL);
		else
		    /* always soft if french ending */
		    if (looking_at(buf, current, +1, "IER "))
			DMETAPH_ADD(code, "J", NULL);
		    else
			DMETAPH_ADD(code, "J", "K");
		current += 2;
		break;
	    }

	    ++current;
	    if (buf[current] == 'G')
		++current;
	    DMETAPH_ADD(code, "K", NULL);
	    break;

	case 'H':
	    /* only keep if first & before vowel or btw. 2 vowels */
	    if ((current == 0 || ISVOWEL(buf, current, -1))
		&& ISVOWEL(buf, current, +1)) {
		DMETAPH_ADD(code, "H", NULL);
		current += 2;
	    } else /* also takes care of 'HH' */
		current++;
	    break;

	case 'J':
	    /* obvious spanish, 'jose', 'san jacinto' */
	    if (looking_at(buf, current, 0, "JOSE")
		|| looking_at(buf, 0, 0, "SAN ")) {
		if ((current == 0 && buf[current + 4] == ' ')
		    || looking_at(buf, 0, 0, "SAN "))
		    DMETAPH_ADD(code, "H", NULL);
		else
		    DMETAPH_ADD(code, "J", "H");
	    
		current++;
		break;
	    }

	    if (current == 0 && !looking_at(buf, current, 0, "JOSE"))
		DMETAPH_ADD(code, "J", "A"); /* Yankelovich | Jankelowicz */
	    else
		/* spanish pron. of e.g. 'bajador' */
		if (ISVOWEL(buf, current, -1)
		    && !IS_SLAVO_GERMANIC()
		    && (buf[current + 1] == 'A' || buf[current + 1] == 'O'))
		    DMETAPH_ADD(code, "J", "H");
		else if (current == last)
		    DMETAPH_ADD(code, "J", " ");
		else if (!looking_at(buf, current, +1, "L|T|K|S|N|M|B|Z")
			 && !looking_at(buf, current, -1, "S|K|L"))
		    DMETAPH_ADD(code, "J", NULL);
	    
	    ++current;
	    if (buf[current] == 'J') 
		++current;
	    break;

	case 'K':
	    ++current;
	    if (buf[current] == 'K')
		++current;
	    DMETAPH_ADD(code, "K", NULL);
	    break;

	case 'L':
	    if (buf[current + 1] == 'L') {
		/* spanish e.g. 'cabrillo', 'gallegos' */
		if ((current == length - 3
		     && looking_at(buf, current, -1, "ILLO|ILLA|ALLE"))
		    || ((looking_at(buf, last, -1, "AS|OS")
			 || looking_at(buf, last, 0, "A|O"))
			&& looking_at(buf, current, -1, "ALLE"))) {
		    DMETAPH_ADD(code, "L", " ");
		    current += 2;
		    break;
		}
		current += 2;
	    } else
		current++;
	    DMETAPH_ADD(code, "L", NULL);
	    break;

	case 'M':
	    if ((looking_at(buf, current, -1, "UMB")
		 && (current + 1 == last
		     || looking_at(buf, current, +2, "ER")))
		/* 'dumb','thumb' */
		|| (buf[current + 1] == 'M'))
		current += 2;
	    else
		current++;
	    DMETAPH_ADD(code, "M", NULL);
	    break;

	case 'N':
	    ++current;
	    if (buf[current] == 'N')
		++current;
	    DMETAPH_ADD(code, "N", NULL);
	    break;
	    
	case L'Ń':
	case L'Ñ':
	    current++;
	    DMETAPH_ADD(code, "N", NULL);
	    break;
	    
	case 'P':
	    if (buf[current + 1] == 'H') {
		DMETAPH_ADD(code, "F", NULL);
		current += 2;
		break;
	    }
	    /* also account for "campbell", "raspberry" */
	    if (looking_at(buf, current, +1, "P|B"))
		current += 2;
	    else
		current++;
	    DMETAPH_ADD(code, "P", NULL);
	    break;
	    
	case 'Q':
	    ++current;
	    if (buf[current] == 'Q')
		++current;
	    DMETAPH_ADD(code, "K", NULL);
	    break;
	    
	case 'R':
	    /* french e.g. 'rogier', but exclude 'hochmeier' */
	    if (current == last
		&& !IS_SLAVO_GERMANIC()
		&& looking_at(buf, current, -2, "IE")
		&& !looking_at(buf, current, -4, "ME|MA"))
		DMETAPH_ADD(code, NULL, "R");
	    else
		DMETAPH_ADD(code, "R", NULL);
	    
	    ++current;
	    if (buf[current] == 'R')
		++current;
	    break;
	    
	case 'S':
	    /* special cases 'island', 'isle', 'carlisle', 'carlysle' */
	    if (looking_at(buf, current, -1, "ISL|YSL")) {
		current++;
		break;
	    }
	    /* special case 'sugar-' */
	    if (current == 0 && looking_at(buf, current, 0, "SUGAR")) {
		DMETAPH_ADD(code, "X", "S");
		current++;
		break;
	    }
	    
	    if (looking_at(buf, current, 0, "SH")) {
		/* germanic */
		if (looking_at(buf, current, +1, "HEIM|HOEK|HOLM|HOLZ"))
		    DMETAPH_ADD(code, "S", NULL);
		else
		    DMETAPH_ADD(code, "X", NULL);
		current += 2;
		break;
	    }
	    /* italian & armenian */
	    if (looking_at(buf, current, 0, "SIO|SIA")
		|| looking_at(buf, current, 0, "SIAN")) {
		if (!IS_SLAVO_GERMANIC())
		    DMETAPH_ADD(code, "S", "X");
		else
		    DMETAPH_ADD(code, "S", NULL);
		current += 3;
		break;
	    }
	    /* german & anglicisations, e.g. 'smith' match 'schmidt',
	       'snider' match 'schneider' */
	    /* also, -sz- in slavic language altho in hungarian it is
	       pronounced 's'
	    */
	    if ((current == 0
		 && looking_at(buf, current, +1, "M|N|L|W"))
		|| looking_at(buf, current, +1, "Z")) {
		DMETAPH_ADD(code, "S", "X");
		if (looking_at(buf, current, +1, "Z"))
		    current += 2;
		else
		    current++;
		break;
	    }
	    
	    if (looking_at(buf, current, 0, "SC")) {
		/* Schlesinger's rule */
		if (buf[current + 2] == 'H') {
		    /* Dutch origin, e.g. 'school', 'schooner' */
		    if (looking_at(buf, current, +3, "OO|ER|EN|UY|ED|EM")) {
			/* 'schermerhorn', 'schenker' */
			if (looking_at(buf, current, +3, "ER|EN"))
			    DMETAPH_ADD(code, "X", "SK");
			else
			    DMETAPH_ADD(code, "SK", NULL);
			current += 3;
			break;
		    } else {
			if (current == 0 && !ISVOWEL(buf, 3, 0) && buf[3] != 'W')
			    DMETAPH_ADD(code, "X", "S");
			else
			    DMETAPH_ADD(code, "X", NULL);
			current += 3;
			break;
		    }
		}
		
		if (looking_at(buf, current, +2, "I|E|Y")) {
		    DMETAPH_ADD(code, "S", NULL);
		    current += 3;
		    break;
		}
		DMETAPH_ADD(code, "SK", NULL);
		current += 3;
		break;
	    }
	    /* french e.g. 'resnais', 'artois' */
	    if (current == last && looking_at(buf, current, -2, "AI|OI"))
		DMETAPH_ADD(code, NULL, "S");
	    else
		DMETAPH_ADD(code, "S", NULL);
	    
	    if (looking_at(buf, current, +1, "S|Z"))
		current += 2;
	    else
		current++;
	    break;
	    
	case 'T':
	    if (looking_at(buf, current, 0, "TION")) {
		DMETAPH_ADD(code, "X", NULL);
		current += 3;
		break;
	    }
	    
	    if (looking_at(buf, current, 0, "TIA|TCH")) {
		DMETAPH_ADD(code, "X", NULL);
		current += 3;
		break;
	    }
	    
	    if (looking_at(buf, current, 0, "TH|TTH")) {
		/* special case 'thomas', 'thames' or germanic */
		if (looking_at(buf, current, +2, "OM|AM")
		    || looking_at(buf, 0, 0, "VAN |VON |SCH")) {
		    DMETAPH_ADD(code, "T", NULL);
		} else {
		    DMETAPH_ADD(code, "0", "T");
		}
		current += 2;
		break;
	    }
	    
	    if (looking_at(buf, current, +1, "T|D"))
		current += 2;
	    else
		current++;
	    DMETAPH_ADD(code, "T", NULL);
	    break;
	    
	case 'V':
	    if (buf[current + 1] == 'V')
		current += 2;
	    else
		current++;
	    DMETAPH_ADD(code, "F", NULL);
	    break;
	    
	case 'W':
	    /* can also be in middle of word */
	    if (looking_at(buf, current, 0, "WR")) {
		DMETAPH_ADD(code, "R", NULL);
		current += 2;
		break;
	    }
	    
	    if (current == 0
		&& (ISVOWEL(buf, current, +1)
		    || looking_at(buf, current, 0, "WH"))) {
		/* Wasserman should match Vasserman */
		if (ISVOWEL(buf, current, +1))
		    DMETAPH_ADD(code, "A", "F");
		else
		    /* need Uomo to match Womo */
		    DMETAPH_ADD(code, "A", NULL);
	    }
	    /* Arnow should match Arnoff */
	    if ((current == last && ISVOWEL(buf, current, -1))
		|| looking_at(buf, current, -1, "EWSKI|EWSKY|OWSKI|OWSKY")
		|| looking_at(buf, 0, 0, "SCH")) {
		DMETAPH_ADD(code, NULL, "F");
		current++;
		break;
	    }
	    /* polish e.g. 'filipowicz' */
	    if (looking_at(buf, current, 0, "WICZ|WITZ")) {
		DMETAPH_ADD(code, "TS", "FX");
		current += 4;
		break;
	    }
	    /* else skip it */
	    current++;
	    break;
	    
	case 'X':
	    /* french e.g. breaux */
	    if (!(current == last
		  && (looking_at(buf, current, -3, "IAU|EAU")
		      || looking_at(buf, current, -2, "AU|OU"))))
		DMETAPH_ADD(code, "KS", NULL);
	    
	    if (looking_at(buf, current, +1, "C|X"))
		current += 2;
	    else
		current++;
	    break;
	    
	case 'Z':
	    /* chinese pinyin e.g. 'zhao' */
	    if (buf[current + 1] == 'H') {
		DMETAPH_ADD(code, "J", NULL);
		current += 2;
		break;
	    } else if (looking_at(buf, current, +1, "ZO|ZI|ZA")
		       || (IS_SLAVO_GERMANIC()
			   && (current > 0 && buf[current - 1] != 'T'))) {
		DMETAPH_ADD(code, "S", "TS");
	    } else
		DMETAPH_ADD(code, "S", NULL);
	    
	    ++current;
	    if (buf[current] == 'Z')
		++current;
	    break;
	    
	default:
	    ++current;
	}
    }

    free(buf);

    if (max_length) {
	/* Trim returned codes */
	metaph_code_trim(code[0], max_length);
	metaph_code_trim(code[1], max_length);
    }
    
    return 0;
err:
    free(buf);
    double_metaphone_free(code);
    return -1;
}

static int
double_metaphone_eq(double_metaphone_code a, double_metaphone_code b)
{
    return metaph_code_eq(a[0], b[0])
	|| metaph_code_eq(a[1], b[1])
	|| metaph_code_eq(a[0], b[1])
	|| metaph_code_eq(a[1], b[0]);
}

static size_t double_metaphone_length = 4;

static int
metaphone2_sel(int cmd, struct dico_key *key, const char *dict_word)
{
    double_metaphone_code code;
    int res;
    
    switch (cmd) {
    case DICO_SELECT_BEGIN:
	if (double_metaphone_encode(code, key->word, double_metaphone_length))
	    return 1;
	key->call_data = malloc(sizeof code);
	if (!key->call_data)
	    return 1;
	memcpy(key->call_data, code, sizeof(code));
	break;

    case DICO_SELECT_RUN:
	if (double_metaphone_encode(code, dict_word, double_metaphone_length))
	    return 1;
	res = double_metaphone_eq(*(double_metaphone_code*)key->call_data,
				   code);
	double_metaphone_free(code);
	return res;

    case DICO_SELECT_END:
	double_metaphone_free(*(double_metaphone_code*)key->call_data);
	free(key->call_data);
	break;
    }
    return 0;
}

static struct dico_strategy metaphone2_strat = {
    "metaphone2",
    "Match Double Metaphone encodings",
    metaphone2_sel
};
    
static int
metaphone2_init(int argc, char **argv)
{
    long size = 0;

    struct dico_option metaphone2_option[] = {
	{ DICO_OPTSTR(size), dico_opt_long, &size },
	{ NULL }
    };

    if (dico_parseopt(metaphone2_option, argc, argv, 0, NULL))
	return 1;
    if (size > 0)
	double_metaphone_length = size;
    
    dico_strategy_add(&metaphone2_strat);
    return 0;
}
    
static int
metaphone2_run_test(int argc, char **argv)
{
    char *modname, *cmd;
    
    argc--;
    modname = *argv++;
    dico_set_program_name(modname);
    
    if (argc == 0) {
	dico_log(L_ERR, 0, "bad argument list");
	return 1;
    }
    
    argc--;
    cmd = *argv++;
    if (strcmp(cmd, "build") == 0) {
	struct metaph_code *code;
	
	code = metaph_code_create();
	assert(code != NULL);
	while (argc--) {
	    int res = metaph_code_add(code, *argv++);
	    assert(res == 0);
	}
	metaph_code_dump(code);
	metaph_code_free(code);
    } else if (strcmp(cmd, "compare") == 0) {
	struct metaph_code *a, *b;
	int res;
	
	a = metaph_code_create();
	assert(a != NULL);
	while (argc--) {
	    char *arg = *argv++;
	    if (strcmp(arg, ":") == 0)
		break;
	    else {
		res = metaph_code_add(a, arg);
		assert(res == 0);
	    }
	}

	if (argc <= 0) {
	    dico_log(L_ERR, 0, "bad argument list");
	    return 1;
	}
	
	b = metaph_code_create();
	assert(b != NULL);
	while (argc--) {
	    res = metaph_code_add(b, *argv++);
	    assert(res == 0);
	}
	res = metaph_code_eq(a, b);
	metaph_code_free(a);
	metaph_code_free(b);
	printf("%d\n", res);
    } else if (strcmp(cmd, "encode") == 0) {
	unsigned long len = 0;
	if (strncmp(*argv, "-length=", 8) == 0) {
	    char *p;
	    len = strtoul(*argv + 8, &p, 10);
	    assert(*p == 0);
	    --argc;
	    ++argv;
	}
	while (argc--) {
	    char *arg = *argv++;
	    double_metaphone_code code;
	    
	    if (double_metaphone_encode(code, arg, len))
		dico_log(L_ERR, errno, "can't encode");
	    else {
		printf("%s: ", arg);
		if (code[0]) {
		    printf("'");
		    metaph_code_print(code[0]);
		    printf("'");
		} else {
		    printf("NULL");
		}
		printf(" -- ");
		if (code[1]) {
		    printf("'");
		    metaph_code_print(code[1]);
		    printf("'");
		} else {
		    printf("NULL");
		}
		putchar('\n');
		double_metaphone_free(code);
	    }
	}
    } else {
	dico_log(L_ERR, 0, "unrecognized unit test: %s", cmd);
	dico_log(L_INFO, 0, "usage:");
	printf("usage: %s build SEQ [SEQ...]\n"
	       "   build a metaphone code block from the sequence of letters\n",
	       modname);
	printf("       %s compare SEQ [SEQ...] : SEQ [SEQ...]\n"
	       "   build two blocks and compare them\n",
	       modname);
	printf("       %s encode [-length=N] WORD [WORD...]\n"
	       "   encode the supplied words\n",
	       modname);
	return 1;
    }
    return 0;
}

struct dico_database_module DICO_EXPORT(metaphone2, module) = {
    .dico_version        =  DICO_MODULE_VERSION,
    .dico_capabilities   =  DICO_CAPA_NODB,
    .dico_init           =  metaphone2_init,
    .dico_run_test       =  metaphone2_run_test
};
