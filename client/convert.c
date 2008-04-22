/* $Id$
*/
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/keysym.h>
#include <stdlib.h>

#include <gjdict.h>
#include <client.h>
#include <search.h>
#define obstack_chunk_alloc XtMalloc
#define obstack_chunk_free XtFree
#include <obstack.h>

/* Order of these is very important. DO NOT CHANGE */
/* STATE_A must be 1. Also, STATE_YA must be (STATE_A + 5) */

enum {
    STATE_NONE, STATE_A, STATE_I, STATE_U, STATE_E, STATE_O,
    STATE_HI,			/* for chi or shi */
    STATE_SU,			/* for tsu */
    STATE_SPACE,		/* for lone 'n' at end */
    STATE_OTHER
};

enum {
    ADD_NONE, ADD_YA, ADD_FILL1, ADD_YU, ADD_FILL2, ADD_YO
};


static char *kanamap[128] = {
    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",


    "", "", "", "", "", "", "", "",
    "", "", "", "", "", "", "", "",

    "", "a", "a", "i", "i", "u", "u", "e",
    "e", "o", "o", "ka", "ga", "ki", "gi", "ku",

    "gu", "ke", "ge", "ko", "go", "sa", "za", "shi",
    "zi", "su", "zu", "se", "ze", "so", "zo", "ta",

    "da", "chi", "ji", "tsu", "tsu", "zu", "te", "de",
    "to", "do", "na", "ni", "nu", "ne", "no", "ha",

    "ba", "pa", "hi", "bi", "pi", "fu", "bu", "pu",
    "he", "be", "pe", "ho", "bo", "po", "ma", "mi",

    "mu", "me", "mo", "ya", "ya", "yu", "yu", "yo",
    "yo", "ra", "ri", "ru", "re", "ro", "wa", "wa",

    "i", "u", "o", "n", "", "", "", "",
    "", "", "", "", "", "", "", "",
};


/* Take a kana string as input. Skip along, converting kana to
 * romaji. Assume 0x24 or 0x25 means kana or kanji start.
 * Assume string buffer is "long enough".
 * Note that we also have to special-case punctuation. sigh.
 *  "(",     ")"
 *  0x214a,  0x214b
 *
 * All this, is so we can print out the "reading" entries, for folks
 * who can't read kana.
 */

static struct obstack stk;
static char *oldptr = NULL;
#define BeginString() \
    if (!oldptr) { \
	obstack_init(&stk); \
    } else {\
	obstack_free(&stk, oldptr);\
	oldptr = NULL;\
    }

#define StoreChar(c) obstack_1grow(&stk, c)
#define StoreString(s) obstack_grow(&stk, s, strlen(s))
#define FinishString() oldptr=obstack_finish(&stk)

char *
kanatoromaji(XChar2b *kana)
{
    int lastchar = 0;		/* for state machine */

    BeginString();
    do {
	/* how boring.. punctuation */
	if (kana->byte1 == 0x21) {
	    int val = kana->byte2;
	    /* print character before this punc */
	    StoreString(kanamap[lastchar]);

	    lastchar = 0;
	    kana++;
	    switch (val) {
	    case 0x21:
		StoreChar(' ');
		continue;
	    case 0x41:
		StoreChar('-');
		continue;
	    case 0x4a:
	    case 0x5a:
		StoreChar('(');
		continue;
	    case 0x4b:
	    case 0x5b:
		StoreChar(')');
		continue;
	    }
	    logmsg(L_WARN, 0, "kanatoromaji found 0x21,%#x", val);
	    continue;

	}
	if (kana->byte1 != 0x24 && kana->byte1 != 0x25
	    && kana->byte1 != 0x0) {
	    logmsg(L_WARN, 0, "0x%x%x found in kanatoromaji", kana->byte1, kana->byte2);
	    kana++;
	    continue;
	}

	switch (kana->byte2) {
	case 0x25: /* u */         
	case 0x26: /* U */
	    StoreString(kanamap[lastchar]);
	    if (kanamap[lastchar][strlen(kanamap[lastchar])-1] == 'o')
		StoreChar('o');
	    break;

	case 0x63:
	case 0x65:
	case 0x67:
	    /* small ya,yu,yo: */
	    /* Put the appropriate prefix */
	    /* Later, will append "a", "u", or "o" */

	    switch (lastchar) {
	    case 0x2d:		/*ki*/
		StoreString("ky");
		break;
	    case 0x2e:		/*gi*/
		StoreString("gy");
		break;
	    case 0x37:		/*shi*/
		StoreString("sh");
		break;
	    case 0x38:		/* shi''*/
		StoreChar('j');
		break;
	    case 0x41:		/*chi*/
		StoreString("ch");
		break;
	    case 0x4b:		/*ni*/
		StoreString("ny");
		break;
	    case 0x52:		/*hi*/
		StoreString("hy");
		break;
	    case 0x53:		/*bi*/
		StoreString("by");
		break;
	    case 0x54:		/*pi*/
		StoreString("py");
		break;
	    case 0x5f:		/*mi*/
		StoreString("my");
		break;
	    case 0x6a:		/*ri*/
		StoreString("ry");
		break;
	    default:
		/* oh well.. just print it as-is */
		/* but mark it as small */
		StoreString("_y");
		break;
	    }
	    switch (kana->byte2) {
	    case 0x63:
		StoreChar('a');
		break;
	    case 0x65:
		StoreChar('u');
		break;
	    case 0x67:
		StoreChar('o');
		break;
		/* must be ya,yu,yo */
	    }
	    break;

	default:
	    switch (lastchar) {
	    case 0:
		break;
	    case 0x43: /* small tsu */         
		switch (kanamap[kana->byte2][0]) {
		case 'k':
		case 's':
		case 't':
		case 'p':
		    StoreChar(kanamap[kana->byte2][0]);
		    break;
		case 'c': /* chi */
		    StoreChar('t');
		    /* fall thru */
		default:
		    StoreString(kanamap[lastchar]);
		}
		break;
	    case 0x73: /* n */
		switch (kanamap[kana->byte2][0]) {
		case 'm':
		case 'b':
		case 'p':
		    StoreChar('m');
		    break;
		default:
		    StoreString(kanamap[lastchar]);
		}
		break;
	    default:
		/* print previous char!... */
		/* then save THIS char,
		 * and continue through loop..
		 */
		StoreString(kanamap[lastchar]);
	    }
	}
	lastchar = kana->byte2;
	kana++;
    } while (kana->byte1 != 0);
    StoreString(kanamap[lastchar]);
    StoreChar(0);
    return FinishString();
}

int
romc(XChar2b *p)
{
    if (p->byte1 == '#')
	return p->byte2;
    return 0;
}

void
romajitokana(XChar2b *kstart)
{
    XChar2b *kparse;
    int lead_char, v;
    
    /* Search for zenkaku romaji */
    while (kstart->byte1 != '#') {
	if (kstart->byte1 == 0)
	    return;
	kstart++;
    }

    kparse = kstart;
    
    switch (lead_char = romc(kstart++)) {
    case 'a':
    case 'i':
    case 'u':
    case 'e':
    case 'o':
	*kparse++ = kana_vowel(lead_char);
	return;

    case '-':
	kparse->byte1 = 0x21;
	kparse->byte2 = 0x3c;
	++kparse;
	kparse->byte1 = 0;
	return;
	    
    case 'y':
	switch (v = romc(kstart++)) {
	case 'a':
	case 'i':
	case 'u':
	case 'e':
	case 'o':
	    *kparse++ = kana_syllable('y', v);
	    kparse->byte1 = 0;
	    return;
	default:
	    return;
	}
	/*NOTREACHED*/

    case 'w':
	switch (v = romc(kstart++)) {
	case 'a':
	case 'i':
	case 'u':
	case 'e':
	case 'o':
	    *kparse++ = kana_syllable('w', v);
	    kparse->byte1 = 0;
	    return;
	default:
	    return;
	}
	/*NOTREACHED*/

    }


    if ((v = romc(kstart++)) == 0) {
	return;
    }
    
    /* If we are here, then `lead_char' is a consonant.
     * First, let's see if it is a long consonant or an
     * `MB', `MP' cluster .
     */
    if (v == lead_char) {
	/* long consonant */
	if (lead_char == 'n')
	    *kparse++ = std_translations[KANA_N][0];
	else
	    *kparse++ = std_translations[KANA_SMALL_TSU][0];
	if ((v = romc(kstart++)) == 0) {
	    return;
	}
    } else if (lead_char == 'm' && (v == 'b' || v == 'p')) {
	*kparse++ = std_translations[KANA_N][0];
	lead_char = v;
	if ((v = romc(kstart++)) == 0) {
	    return;
	}
    }
    
    /* Check for possible `SH-' syllable */
    if (lead_char == 's' && v == 'h') {
	switch (v = romc(kstart++)) {
	case 'a':
	    *kparse++ = std_translations[KANA_SHI][0];
	    *kparse++ = std_translations[KANA_SMALL_YA][0];
	    break;
	case 'i':
	    *kparse++ = std_translations[KANA_SHI][0];
	    break;
	case 'u':
	    *kparse++ = std_translations[KANA_SHI][0];
	    *kparse++ = std_translations[KANA_SMALL_YU][0];
	    break;
	case 'e':
	    *kparse++ = std_translations[KANA_SHI][0];
	    *kparse++ = std_translations[KANA_SMALL_E][0];
	    break;
	case 'o':
	    *kparse++ = std_translations[KANA_SHI][0];
	    *kparse++ = std_translations[KANA_SMALL_YO][0];
	    break;
	default:
	    return;
	}
	kparse->byte1 = 0;
	return;
    }

    /* check for `J-' syllable */
    if (lead_char == 'j') {
	switch (v) {
	case 'a':
	    *kparse++ = std_translations[KANA_ZI][0];
	    *kparse++ = std_translations[KANA_SMALL_YA][0];
	    break;
	case 'i':
	    *kparse++ = std_translations[KANA_ZI][0];
	    break;
	case 'u':
	    *kparse++ = std_translations[KANA_ZI][0];
	    *kparse++ = std_translations[KANA_SMALL_YU][0];
	    break;
	case 'e':
	    *kparse++ = std_translations[KANA_ZI][0];
	    *kparse++ = std_translations[KANA_SMALL_E][0];
	    break;
	case 'o':
	    *kparse++ = std_translations[KANA_ZI][0];
	    *kparse++ = std_translations[KANA_SMALL_YO][0];
	    break;
	default:
	    return;
	}
	kparse->byte1 = 0;
	return;
    }

    /* Check for possible `CH-' syllable */
    if (lead_char == 'c' && v == 'h') {
	switch (v = romc(kstart++)) {
	case 'a':
	    *kparse++ = std_translations[KANA_CHI][0];
	    *kparse++ = std_translations[KANA_SMALL_YA][0];
	    break;
	case 'i':
	    *kparse++ = std_translations[KANA_CHI][0];
	    break;
	case 'u':
	    *kparse++ = std_translations[KANA_CHI][0];
	    *kparse++ = std_translations[KANA_SMALL_YU][0];
	    break;
	case 'e':
	    *kparse++ = std_translations[KANA_CHI][0];
	    *kparse++ = std_translations[KANA_SMALL_E][0];
	    break;
	case 'o':
	    *kparse++ = std_translations[KANA_CHI][0];
	    *kparse++ = std_translations[KANA_SMALL_O][0];
	    break;
	default:
	    return;
	}
	kparse->byte1 = 0;
	return;
    }

    /* Check for `TS-' syllable */
    if (lead_char == 't' && v == 's') {
	switch (v = romc(kstart++)) {
	case 'a':
	    *kparse++ = std_translations[KANA_TSU][0];
	    *kparse++ = std_translations[KANA_SMALL_A][0];
	    break;
	case 'i':
	    *kparse++ = std_translations[KANA_TSU][0];
	    *kparse++ = std_translations[KANA_SMALL_I][0];
	    break;
	case 'u':
	    *kparse++ = std_translations[KANA_TSU][0];
	    break;
	case 'e':
	    *kparse++ = std_translations[KANA_TSU][0];
	    *kparse++ = std_translations[KANA_SMALL_E][0];
	    break;
	case 'o':
	    *kparse++ = std_translations[KANA_TSU][0];
	    *kparse++ = std_translations[KANA_SMALL_O][0];
	    break;
	default:
	    return;
	}
	kparse->byte1 = 0;
	return;
    }

    
    /* `lead_char' is a consonant.
     * Check if `v' is a semi-consonant
     */
    if (v == 'y') {
	XChar2b c;

	switch (v = romc(kstart++)) {
	default:
	    return;
	case 'a':
	    c = std_translations[KANA_SMALL_YA][0];
	    break;
	case 'i':
	    c = std_translations[KANA_SMALL_I][0];
	    break;
	case 'u':
	    c = std_translations[KANA_SMALL_YU][0];
	    break;
	case 'e':
	    c = std_translations[KANA_SMALL_E][0];
	    break;
	case 'o':
	    c = std_translations[KANA_SMALL_YO][0];
	    break;
	}
	*kparse++ = kana_syllable(lead_char, 'i');
	*kparse++ = c;
	kparse->byte1 = 0;
	return;
    }

    /* `N' + consonant */
    if (lead_char == 'n') {
	switch (v) {
	case 'a':
	case 'i':
	case 'u':
	case 'e':
	case 'o':
	    *kparse++ = kana_syllable(lead_char, v);
	    kparse->byte1 = 0;
	    break;
	case ' ':
	    *kparse++ = std_translations[KANA_N][0];
	    kparse->byte1 = 0;
	    break;
	default:
	    *kparse++ = std_translations[KANA_N][0];
	}
	return;
    }

    /* `F-' gets special treatment */
    if (lead_char == 'f') {
	switch (v) {
	case 'a':
	    *kparse++ = std_translations[KANA_FU][0];
	    *kparse++ = std_translations[KANA_SMALL_A][0];
	    break;
	case 'i':
	    *kparse++ = std_translations[KANA_FU][0];
	    *kparse++ = std_translations[KANA_SMALL_I][0];
	    break;
	case 'u':
	    *kparse++ = std_translations[KANA_FU][0];
	    break;
	case 'e':
	    *kparse++ = std_translations[KANA_FU][0];
	    *kparse++ = std_translations[KANA_SMALL_E][0];
	    break;
	case 'o':
	    *kparse++ = std_translations[KANA_FU][0];
	    *kparse++ = std_translations[KANA_SMALL_O][0];
	    break;
	default:
	    return;
	}
	kparse->byte1 = 0;
	return;
    }
	    
    /* Final stage. */
    switch (v) {
    case 'a':
    case 'i':
    case 'u':
    case 'e':
    case 'o':
	/* It is a regular gojuon syllable */
	*kparse++ = kana_syllable(lead_char, v);
	kparse->byte1 = 0;
    }
    return;
}

/* romajitoascii(): Convert a 16-bit string containing ONLY english
 * letters to equivalent ASCII string.
 */
char *
romajitoascii(char *buf, XChar2b *src, int buflen)
{
    int i;
    
    for (i = 0; i < buflen-1; i++, src++) {
	if (src->byte1 == 0)
	    break;
	else if (src->byte1 == '#')
	    buf[i] = src->byte2;
	else {
	    logmsg(L_WARN, 0, "romajitoascii() choked at {%#x, %#x} :(",
		    src->byte1, src->byte2);
	    buf[0] = 0;
	    break;
	}
    }
    buf[i] = 0;
    return buf;
}



/* FIXME: Fixed buffer size! */
#define MAXROMAJI 50

void
Handle_romajikana(Widget w, XtPointer closure, XEvent *e, Boolean *cont)
{
    XKeyEvent *key_event;
    KeySym inbetweenK;
    char *charpressed;
    XChar2b addchar;

    if (e->type != KeyPress) {
	if (e->type == KeyRelease) {
	    logmsg(L_WARN, 0, "key released");
	    return;
	}
	logmsg(L_WARN, 0, "Some other strange event found in for romaji: %d\n", e->type);
	return;
    }
    key_event = (XKeyEvent *) e;

    if (key_event->state & ControlMask)
	return;
/*	(ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
	return;
*/	
    inbetweenK = XKeycodeToKeysym(XtDisplay(w), key_event->keycode, 0);
    if (inbetweenK == (KeySym) NULL) {
	logmsg(L_WARN, 0, "NULL keysym on kana input???");
	return;
    }
    switch (inbetweenK) {
    case XK_BackSpace:
    case XK_Delete:
	process_backspace();
	return;

    case XK_Return:
	/* pass our strange "accept" char*/
/*	process_accept();*/
	return;

    case XK_space:
	/* add " ", but in kana range */
	/* This is a nasty hack to get "n" right */
	addchar.byte1 = 0x23;
	addchar.byte2 = ' ';
	process_kinput(addchar);
	return;
    case XK_minus:
	/* add "-", but in kana range */
	/* Used to represent katakana `long vowel' mark 
	 */
	addchar.byte1 = 0x23;
	addchar.byte2 = '-';
	process_kinput(addchar);
	return;

    }
    charpressed = XKeysymToString(inbetweenK);
    if (charpressed == NULL)
	return;


    DEBUG(("got string \"%s\"", charpressed));

    /* now use process_kinput, 222b is erase */
    if ((*charpressed < 0x61) || (*charpressed > 0x7a)) {
	/* outside range of ascii chars we like */
	DEBUG(("ignoring.. not in normal ascii range"));
	return;
    }
    addchar.byte1 = 0x23;
    addchar.byte2 = *charpressed;
    process_kinput(addchar);

}




