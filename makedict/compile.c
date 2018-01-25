/* This file is part of GNU Dico.
   Copyright (C) 1998-2018 Sergey Poznyakoff

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

#include <makedict.h>
#include <ctype.h>

unsigned numberofkanji = 0, highestkanji = 0, lowestkanji = 0;
unsigned nedict = 0; /* number of edict entiries */


#define isws(c) (c == ' ' || c == '\t')

#define TOK_ENGLISH    256
#define TOK_KANA       257
#define TOK_KANJI      258
#define TOK_NUMBER     259
#define TOK_EOL        '\n'

struct dict_token {
    int type;
    size_t length;
    unsigned char *string;
};

static struct obstack stk;
static FILE *infile;
static char *filename;
static int line_num;
static struct dict_token token;

void
skip_to_eol()
{
    int c;
    
    while ((c = getc(infile)) != EOF) {
	if (c == '\n') {
	    ungetc(c, infile);
	    return;
	}
    }
}

void
skip_to_delim(char *str)
{
    int c;
    
    while ((c = getc(infile)) != EOF) {
	if (strchr(str, c) == 0) {
	    ungetc(c, infile);
	    return;
	}
    }
}

void
copy_to(int firstc, int term)
{
    int c;
    
    if (firstc) 
	obstack_1grow(&stk, firstc);
    while ((c = getc(infile)) != EOF && c != term) 
	obstack_1grow(&stk, c);
    obstack_1grow(&stk, 0);
    token.string = obstack_finish(&stk);
    token.length = strlen(token.string);
}

void
copy_kana(int firstc)
{
    int c1, c2;
    int okurigana = 0;
    int done = 0;
    
    c1 = firstc;

    do {
	switch (c1) {
	case '.':
	    okurigana++;
	    obstack_1grow(&stk, 0x21);
	    obstack_1grow(&stk, 0x4a);
	    break;
	case '-':
	    obstack_1grow(&stk, 0x21);
	    obstack_1grow(&stk, 0x41);
	    continue;
	    
	case ' ':
	    done++;
	    break;

	default:
	    if (c1 < 127) {
		if (okurigana) {
		    /* FIXME: emit warning? */
		}
		done++;
	    } else { 
		c2 = getc(infile);
		obstack_1grow(&stk, c1 & 0x7f);
		obstack_1grow(&stk, c2 & 0x7f);
	    }
	    break;
	}

    } while (!done && (c1 = getc(infile)) != EOF);

    if (okurigana) {
	/* add closing parenthesis */
	obstack_1grow(&stk, 0x21);
	obstack_1grow(&stk, 0x4b);
    }
    obstack_1grow(&stk, 0);
    obstack_1grow(&stk, 0);
    
    token.string = obstack_finish(&stk);
    token.length = strlen(token.string);
}

int
nextkn()
{
    int c;
    
 again:
    while ((c = getc(infile)) != EOF && isws(c))
	;

    if (c == EOF)
	return token.type = 0;
	
    if (c == '#') {
	skip_to_eol();
	goto again;
    } else if (c == '\n') {
	return token.type = '\n';
    } else if (c == '{') {
	copy_to(0, '}');
	return token.type = TOK_ENGLISH;
    } else if (c > 127) {
	copy_kana(c);
	return token.type = TOK_KANA;
    } else {
	copy_to(c, ' ');
	if (isdigit(c))
	    return token.type = TOK_NUMBER;
	else
	    return token.type = token.string[0];
    }
    /*NOTREACHED*/
}

int
edict_nextkn()
{
    int c;
    
 again:
    while ((c = getc(infile)) != EOF && isws(c))
	;

    if (c == EOF)
	return token.type = 0;
	
    if (c == '#') {
	skip_to_eol();
	goto again;
    } else if (c == '\n') {
	return token.type = '\n';
    } else if (c == '[') {
	char *p;
	
	copy_to(0, ']');
	/* convert to JIS */
	for (p = token.string; *p; p++)
	    *p &= 0x7f;
	return token.type = TOK_KANA;
    } else if (c > 127) {
	copy_kana(c);
	return token.type = TOK_KANJI;
    } else if (c == '/') {
	if ((c = getc(infile)) == EOF)
	    return token.type = 0;
	else if (c == '\n')
	    return token.type = TOK_EOL;
	
	copy_to(c, '/');
	ungetc('/', infile);
	return token.type = TOK_ENGLISH;
    } else {
	copy_to(c, '\n');
	return token.type = token.string[0];
    }
    /*NOTREACHED*/
}

enum text_type {
    TEXT_ENGLISH,
    TEXT_YOMI,
    TEXT_PINYIN
};

struct text_hdr {
    struct text_hdr *next;
    enum text_type type;
    int length;
    char *string;
};


size_t 
coalesce_text(struct text_hdr *text_chain, enum text_type type,
	      char *delim, size_t delimlen)
{
    struct text_hdr *textp;
    size_t len = 0;
    
    for (textp = text_chain; textp; textp = textp->next) {
	if (textp->type == type) {
	    if (len) {
		obstack_grow(&stk, delim, delimlen);
		len += delimlen;
	    }
	    obstack_grow(&stk, textp->string, textp->length);
	    len += textp->length;
	}
    }
    return len;
}

unsigned long
get_number(const char *str, int base, unsigned long maxval, char *delim)
{
    unsigned long val;
    char *p;

    val = strtoul(str, &p, base);
    if (*p && !(delim && strchr(delim, *p))) {
	dico_log(L_ERR, 0, "%s:%d: not a valid number (%s)",
		 filename, line_num, str);
	return 0;
    }
    if (val > maxval) {
	dico_log(L_ERR, 0, "%s:%d: %s: value out of allowed range (0..%lu)",
		 filename, line_num, str, maxval);
	return 0;
    }
    return val;
}

#define ADD_TEXT(t,str,len)                                                 \
                text = obstack_alloc(&stk, sizeof(*text));                  \
		text->next = NULL;                                          \
		if (text_tail)                                              \
		    text_tail->next = text;                                 \
		else                                                        \
		    text_head = text;                                       \
		text_tail = text;                                           \
		text->type = t;                                             \
		text->length = len;                                         \
		text->string = str; 
    
void 
compile_kanjidic(DB *dbp)
{
    DictEntry entry, *ep;
    struct text_hdr *text_head, *text_tail, *text;
    size_t length, offset;
    int kanji;
    int rc;
    XChar2b buff[2];
    int pflag;
    void *stkroot;
    char *p;
    long num;

    infile = open_compressed(kanjidict, &pflag);
    if (infile == NULL) {
	dico_die(1, L_ERR, errno, "cannot open kanjidic file `%s'", kanjidict);
	return;
    }
    if (verbose)
	printf("processing dictionary %s\n", kanjidict);
    filename = kanjidict;
    line_num = 0;
    
    obstack_init(&stk);
    obstack_1grow(&stk, 0); 
    stkroot = obstack_finish(&stk);

    text_head = text_tail = NULL;
    while (nextkn() != 0) {
	line_num++;

	if (token.type == TOK_EOL)
	    continue;
	
	if (token.type != TOK_KANA || token.length != 2) {
	    dico_log(L_ERR, 0, "%s:%d: unrecognized line",
		     filename, line_num);
	    skip_to_eol();
	    continue;
	}
	
	if (nextkn() != TOK_NUMBER) {
	    dico_log(L_ERR, 0, "%s:%d: expected JIS code but found `%s'",
		     filename, line_num, token.string);
	    skip_to_eol();
	    continue;
	}
		
	kanji = strtol(token.string, &p, 16);
	if (!isws(*p) && *p != 0) {
	    dico_log(L_ERR, 0, "%s:%d: unrecognized line\n",
		     filename, line_num);
	    skip_to_eol();
	    continue;
	}    

	memset(&entry, 0, sizeof(entry));

	while (nextkn() != TOK_EOL && token.type != 0) {
	    switch (token.type) {
	    case 'F':
		entry.frequency = get_number(token.string + 1, 10, USHRT_MAX,
					     NULL);
		break;
		
	    case 'G':
		entry.grade_level = get_number(token.string + 1, 10,
					       UCHAR_MAX, NULL);
		break;
		
	    case 'H':
		entry.Hindex = get_number(token.string + 1, 10,
					  USHRT_MAX, NULL);
		break;
		
	    case 'N':
		entry.Nindex = get_number(token.string + 1, 10,
					  USHRT_MAX, NULL);
		break;
		
	    case 'Q':
		/* FIXME: 5th corner is ignored */
		entry.Qindex = get_number(token.string + 1, 10,
					  USHRT_MAX, ".");
		break;
		
	    case 'U':
		entry.Uindex = get_number(token.string + 1, 16,
					  USHRT_MAX, NULL);
		break;
		
	    case 'B':
		entry.bushu.bushu = get_number(token.string + 1, 10,
					       USHRT_MAX, NULL);
		break;
		
	    case 'S':
		entry.bushu.numstrokes = get_number(token.string + 1, 10,
						    USHRT_MAX, NULL);
		break;
		
	    case 'Y':
		ADD_TEXT(TEXT_PINYIN, token.string + 1, token.length-1);
		break;
		
	    case 'P': /* skip code */
		p = token.string;
		num = strtol(p+1, &p, 10);
		if (*p != '-')
		    break;
		num <<= 8;
		num |= strtol(p+1, &p, 10);
		if (*p != '-')
		    break;
		num <<= 8;
		num |= strtol(p+1, &p, 10);
		entry.skip = num;
		break;
		
	    case TOK_ENGLISH:
		ADD_TEXT(TEXT_ENGLISH, token.string, token.length);
		break;
		
	    case TOK_KANA:
		ADD_TEXT(TEXT_YOMI, token.string, token.length);
		break;
	    }
	}		

	if (lowestkanji == highestkanji && highestkanji == 0) {
	    lowestkanji = highestkanji = kanji;
	} else {
	    if (kanji < lowestkanji)
		lowestkanji = kanji;
	    if (kanji > highestkanji)
		highestkanji = kanji;
	}

	/* Begin preparing final entry */
	obstack_grow(&stk, &entry, sizeof(entry));
	obstack_1grow(&stk, 0);
	offset = 1;

	buff[0].byte1 = (kanji & 0xff00) >> 8;
	buff[0].byte2 = (kanji & 0xff);
	buff[1].byte1 = 0;
	buff[1].byte2 = 0;
	obstack_grow(&stk, buff, sizeof(buff));
	entry.kanji = offset;
	offset += sizeof(buff);
	entry.Jindex = kanji;

	/* Finally, collect and save text blocks */
	/* 1. English */
	length = coalesce_text(text_head, TEXT_ENGLISH, "|", 1);
	/*FIXME: setmaxlen_8(length);*/
	obstack_1grow(&stk, 0);
	entry.english = offset;
	offset += length + 1;
	
	/* 2. Pinyin */
	length = coalesce_text(text_head, TEXT_PINYIN, "|", 1);
	/*FIXME: setmaxlen_8(length);*/
	obstack_1grow(&stk, 0);
	entry.pinyin = offset;
	offset += length + 1;

	/* 3. Yomi */
	length = coalesce_text(text_head, TEXT_YOMI, "\x21\x21", 2);
	/* FIXME: setmaxlen_yomi((XChar2b*)buffer+1);*/
	obstack_1grow(&stk, 0);
	obstack_1grow(&stk, 0);
	entry.yomi = offset;
	offset += length + 2;

	ep = obstack_finish(&stk);
	memcpy(ep, &entry, sizeof(entry));
	
	rc = insert_dict_entry(dbp, numberofkanji, ep, sizeof(entry) + offset);
	if (rc)
	    dico_log(L_ERR, 0, "%s:%d: failed to insert entry: %s",
		     filename, line_num, db_strerror(rc));

	numberofkanji++;
	if (verbose > 1 && numberofkanji % 1000 == 0) {
	    putchar('.');
	    fflush(stdout);
	}

	obstack_free(&stk, stkroot);
	obstack_1grow(&stk, 0); 
	stkroot = obstack_finish(&stk);
	text_head = text_tail = NULL;
    }			      
    if (verbose > 1)
	puts("");
    
    if (pflag)
	pclose(infile);
    else
	fclose(infile);

    obstack_free(&stk, NULL);
}

void
compile_edict(DB *dbp)
{
    DictEntry entry, *ep;
    int pflag;
    struct text_hdr *text_head, *text_tail, *text;
    char *kanji = NULL, *yomi = NULL;
    size_t kanji_len, yomi_len;
    char *stkroot;
    size_t offset, length;
    int rc;
    
    infile = open_compressed(edict, &pflag);
    if (infile == NULL) {
	dico_die(1, L_ERR, errno, "cannot open edict file `%s'", edict);
	return;
    }
    if (verbose)
	printf("processing dictionary %s\n", edict);

    line_num = 0;
    filename = edict;
    
    obstack_init(&stk);
    obstack_1grow(&stk, 0); 
    stkroot = obstack_finish(&stk);
    text_head = text_tail = NULL;
    
    while (edict_nextkn()) {
	line_num++;

	if (token.type == TOK_EOL)
	    continue;

	if (token.type != TOK_KANJI) {
	    dico_log(L_ERR, 0, "%s:%d: unrecognized line",
		     filename, line_num);
	    skip_to_eol();
	    continue;
	}

	if (strcmp(token.string, "\x21\x29\x21\x29\x21\x29\x21\x29") == 0) {
	    /* copyright mark: "？？？？" */
	    skip_to_eol();
	    continue;
	}
	
	if (verbose > 1 && nedict % 1000 == 0) {
	    putchar('.');
	    fflush(stdout);
	}

	memset(&entry, 0, sizeof(entry));

	kanji = token.string;
	kanji_len = token.length;
	
	if (edict_nextkn() == TOK_KANA) {
	    yomi = token.string;
	    yomi_len = token.length;
	    /*FIXME setmaxlen_16((XChar2b*)token.string);*/
	} else {
	    yomi = kanji;
	    yomi_len = kanji_len;
	    
	    if (token.type == TOK_ENGLISH) {
		ADD_TEXT(TEXT_ENGLISH, token.string, token.length);
	    } else {
		dico_log(L_ERR, 0, "%s:%d: unrecognized string",
			 filename, line_num);
	    }
	}
 	
	while (edict_nextkn() == TOK_ENGLISH) {
	    ADD_TEXT(TEXT_ENGLISH, token.string, token.length);
	}

	if (token.type != TOK_EOL && token.type != 0) {
	    dico_log(L_ERR, 0, "%s:%d: junk after end of line",
		     filename, line_num);
	    skip_to_eol();
	}

	obstack_grow(&stk, &entry, sizeof(entry));
	obstack_1grow(&stk, 0);
	offset = 1;
	obstack_grow(&stk, kanji, kanji_len);
	obstack_1grow(&stk, 0);
	obstack_1grow(&stk, 0);
	entry.kanji = offset;
	offset += kanji_len + 2;

	obstack_grow(&stk, yomi, yomi_len);
	obstack_1grow(&stk, 0);
	obstack_1grow(&stk, 0);
	entry.yomi = offset;
	offset += yomi_len + 2;

	length = coalesce_text(text_head, TEXT_ENGLISH, "|", 1);
	obstack_1grow(&stk, 0);
	entry.english = offset;
	offset += length + 1;
	/*FIXME: setmaxlen_8(length+1);*/

	ep = obstack_finish(&stk);
	memcpy(ep, &entry, sizeof(entry));

	rc = insert_dict_entry(dbp, numberofkanji + nedict,
			       ep, sizeof(entry) + offset);
	nedict++;
	if (rc)
	    dico_log(L_ERR, 0, "%s:%d: failed to insert entry: %s",
		     filename, line_num, db_strerror(rc));
	
	obstack_free(&stk, stkroot);
	obstack_1grow(&stk, 0); 
	stkroot = obstack_finish(&stk);
	text_head = text_tail = NULL;
    }

    obstack_free(&stk, NULL);
    if (pflag)
	pclose(infile);
    else
	fclose(infile);

    if (verbose > 1)
	puts("");
}

int
compile()
{
    DB *dbp;

    dbp = open_db();
    compile_kanjidic(dbp);
    compile_edict(dbp);
    iterate_db(dbp);
    count_xref(dbp);
    close_db(dbp);
    if (verbose) {
	printf("%d kanji (from %4X to %4X)\n%d dictionary entries\n",
	       numberofkanji, lowestkanji, highestkanji, nedict);
    }
    return 0;
}
