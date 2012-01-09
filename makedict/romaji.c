/* This file is part of GNU Dico.
   Copyright (C) 1998-2000, 2008, 2010, 2012 Sergey Poznyakoff

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

enum kana_state {
    state_initial,
    state_n,
    state_tsu,
};

static const char *kanamap[128] = {
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",


    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",

    "", "a", "a", "i", "i", "u", "u", "e",
    "e", "o", "o", "ka", "ga", "ki", "gi", "ku",

    "gu", "ke", "ge", "ko", "go", "sa", "za", "shi",
    "ji", "su", "zu", "se", "ze", "so", "zo", "ta",

    "da", "chi", "ji", "tsu", "tsu", "zu", "te", "de",
    "to", "do", "na", "ni", "nu", "ne", "no", "ha",

    "ba", "pa", "hi", "bi", "pi", "fu", "bu", "pu",
    "he", "be", "pe", "ho", "bo", "po", "ma", "mi",

    "mu", "me", "mo", "ya", "ya", "yu", "yu", "yo",
    "yo", "ra", "ri", "ru", "re", "ro", "wa", "wa",

    "i", "u", "o", "n", "", "", "", "",
    "", "", "", "", "", "", "", "",
};

#define IS_KATAKANA_BAR(s) ((s).byte1 == 0x21 && (s).byte2 == 0x3c)

int
kana_to_romaji(const XChar2b *input, size_t size, romaji_out_fn fun,
	       void *closure)
{
    enum kana_state state = state_initial; /* Machine state */
    char buf[2][6]; /* syllable buffers */
    int bn = 0; /* buffer number */
    const char *last = NULL; /* pointer to the last read syllable */
    size_t count = 0; /* Number of bytes output */
    const char *syl;

#define FLUSH()                         \
    if (last)                           \
      {                                 \
        int rc = fun(closure, last);    \
        if (rc == 0)                    \
	   size = 0;                    \
	else                            \
	   count += rc;                 \
        last = NULL;                    \
      }
      
    while (input->byte1 != HIRAGANA_BYTE && input->byte1 != KATAKANA_BYTE) {
	if (input->byte1 == 0x0)
	    return 0;
	input++;
	size--;
    }
    
    while (size) {
	size_t len;
	XChar2b kana;
	char *p;
	
	if (input->byte1 != HIRAGANA_BYTE && input->byte1 != KATAKANA_BYTE
	    && !IS_KATAKANA_BAR(input[0])
	    && input->byte1 != 0x0) 
	    break;

	kana = *input++;
	size--;
	
	if (kana.byte2 > 128) 
	    continue;

	if (kana.byte2 == 0x43) {
	    /* small tsu */
	    FLUSH();
	    state = state_tsu;
	    continue;
	} else if (kana.byte2 == 0x73) {
	    /* n */
	    FLUSH();
	    last = kanamap[kana.byte2];
	    state = state_n;
	    continue;
	}
	
	syl = kanamap[kana.byte2];
	switch (state) {
	case state_initial:
	    switch (kana.byte2) {
	    case 0x25: /* u */         
	    case 0x26: /* U */
		if (last && last[strlen(last)-1] == 'o') {
		    strcpy(buf[bn], last);
		    strcat(buf[bn], "o");
		    last = buf[bn];
		    bn = !bn;
		} else {
		    FLUSH();
		    last = syl;
		}
		break;

	    case 0x63:
	    case 0x65:
	    case 0x67:
		/* small ya,yu,yo: */
		len = strlen(last);
		if (last && len > 1 && last[len - 1] == 'i') {
		    switch (last[len-2]) {
		    case 'j':
			strcpy(buf[bn], last);
			p = buf[bn];
			bn = !bn;
			strcpy(&p[len-1], &syl[1]);
			last = p;
			break;
			
		    case 'h':
			if (len > 2) {
			    switch (last[len - 3]) {
			    case 's':
			    case 'c':
				strcpy(buf[bn], last);
				p = buf[bn];
				bn = !bn;
				strcpy(&p[len-1], &syl[1]);
				last = p;
				continue;
			    }
			}
			/* FALL THROUGH */

		    case 'k':
		    case 'g':
		    case 'n':
		    case 'b':
		    case 'p':
		    case 'm':
		    case 'r':
			strcpy(buf[bn], last);
			strcpy(buf[bn] + len - 1, syl);
			last = buf[bn];
			bn = !bn;
			break;

		    default:
			FLUSH();
			last = syl;
		    }
		} else {
		    FLUSH();
		    last = syl;
		}
		break;

	    default:
		if (IS_KATAKANA_BAR(kana)) {
		    len = strlen(last);
		    if (memchr("aiueo", last[len-1], 5)) {
			p = buf[bn];
			bn = !bn;
			strcpy(p, last);
			strcat(p, &last[len-1]);
			last = p;
		    } else {
			FLUSH();
		    }
		} else {
		    FLUSH();
		    last = syl;
		}
		break;
	    }
	    break;
	    
	case state_n:
	    state = state_initial;
	    if (memchr("mbp", syl[0], 3)) {
		buf[bn][0] = 'm';
		strcpy(buf[bn] + 1, syl);
		last = buf[bn];
		bn = !bn;
	    } else {
		FLUSH();
		last = syl;
	    }
	    break;
	    
	case state_tsu:
	    state = state_initial;
	    FLUSH();
	    if (memchr("kstpb", syl[0], 5)) {
		buf[bn][0] = syl[0];
		strcpy(buf[bn] + 1, syl);
		last = buf[bn];
		bn = !bn;
	    } else {
		last = syl;
	    }
	    break;
	}
    }	
    FLUSH();
    
    return count;
}


struct string_buf {
    char *buf;
    char *cur;
    size_t size;
};
	
static int
romaji_out_to_string(void *closure, const char *str)
{
    struct string_buf *sb = closure;
    size_t len = strlen(str);

    if (len > sb->size)
	return 0;
    memcpy(sb->cur, str, len);
    sb->cur += len;
    sb->size -= len;
    return len;
}

int
kana_to_romaji_str(const XChar2b *input, size_t size, char **sptr)
{
    size_t outsize = 3 * size;
    struct string_buf sb;
    int rc;
    
    sb.buf = xmalloc(outsize + 1);
    sb.cur = sb.buf;
    sb.size = outsize;
    rc = kana_to_romaji(input, size, romaji_out_to_string, &sb);
    *sb.cur = 0;
    *sptr = sb.buf;
    return rc;
}

#if 0
int
main()
{
    char *text[] = {
	"...$8$c$/$7$s...",
	"しんぶん",
	"りっぱだ",
	"にほんがっか",
	"にっぽん",
	"しゅっせき",
	"いっしょ",
	"ろっぴゃく",
	"しゅっぽつ",
	"きっさてん",
	"けっか",
	"ちょっと",
	"たってください",
	"おんな",
	/* おかあさん */
	"ちいさい",
	"くうこう",
	"にゅうがく",
	"おねさん",
	"いもうと",
	"しょくどう",
	"どようび",
	"きょうだい",
	"りょうしん",
	"きょうしつ",
	"こうこう",
	"きょう",
	"おおきい",
	"コーヒー",
	"ヨーロッパ",
	"ビール",
	/* "アンケート", */
	"ポーランド",
	NULL
    };
    char *buf;
    int i;
    int n;
    
    for (i = 0; text[i]; i++) {
 	n = kana_to_romaji_str(text[i]+3, (strlen(text[i])-6)/2, &buf);
	printf("%2d: (%2d) %s\n",i, n, buf);
    }
}
#endif

/* Local variables: */
/* buffer-file-coding-system: iso-2022-jp-unix */
/* End: */

