/* $Id$
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <X11/Xos.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <Xaw.h>
#include <X11/keysym.h>

#include <dico.h>
#include <client.h>
#include <search.h>


enum {
    BACKSPACE,
    KANA_TOGGLE,
    FIND_NEXT,
    FIND_PREV,
    STATE_1,
    STATE_2,
    KBD_MAX
};

typedef struct {
    int (*layout)[3][5];
    Widget kw[55];
    Widget w[KBD_MAX];
    int state;
} Keyboard;

XChar2b * get_key_label (Keyboard *, int, int);
void setkbdstate (Keyboard *, int);
void clearkbdstate (Keyboard *);
	
XChar2b std_translations[][2] =
{
    {	{0x24, 0x22},	{0x0, 0x0}},		/* a */
    {	{0x24, 0x24},	{0x0, 0x0}},
    {	{0x24, 0x26},	{0x0, 0x0}},
    {	{0x24, 0x28},	{0x0, 0x0}},
    {	{0x24, 0x2a},	{0x0, 0x0}},

    {	{0x24, 0x2b},	{0x0, 0x0}},		/* ka */
    {	{0x24, 0x2d},	{0x0, 0x0}},
    {	{0x24, 0x2f},	{0x0, 0x0}},
    {	{0x24, 0x31},	{0x0, 0x0}},
    {	{0x24, 0x33},	{0x0, 0x0}},

    {	{0x24, 0x35},	{0x0, 0x0}},		/* sa */
    {	{0x24, 0x37},	{0x0, 0x0}},
    {	{0x24, 0x39},	{0x0, 0x0}},
    {	{0x24, 0x3b},	{0x0, 0x0}},
    {	{0x24, 0x3d},	{0x0, 0x0}},

    {	{0x24, 0x3f},	{0x0, 0x0}},		/* ta */
    {	{0x24, 0x41},	{0x0, 0x0}},
    {	{0x24, 0x44},	{0x0, 0x0}},
    {	{0x24, 0x46},	{0x0, 0x0}},
    {	{0x24, 0x48},	{0x0, 0x0}},

    {	{0x24, 0x4a},	{0x0, 0x0}},		/* na */
    {	{0x24, 0x4b},	{0x0, 0x0}},
    {	{0x24, 0x4c},	{0x0, 0x0}},
    {	{0x24, 0x4d},	{0x0, 0x0}},
    {	{0x24, 0x4e},	{0x0, 0x0}},

    {	{0x24, 0x4f},	{0x0, 0x0}},		/* ha */
    {	{0x24, 0x52},	{0x0, 0x0}},
    {	{0x24, 0x55},	{0x0, 0x0}},
    {	{0x24, 0x58},	{0x0, 0x0}},
    {	{0x24, 0x5b},	{0x0, 0x0}},

    {	{0x24, 0x5e},	{0x0, 0x0}},		/* ma */
    {	{0x24, 0x5f},	{0x0, 0x0}},
    {	{0x24, 0x60},	{0x0, 0x0}},
    {	{0x24, 0x61},	{0x0, 0x0}},
    {	{0x24, 0x62},	{0x0, 0x0}},

    {	{0x24, 0x69},	{0x0, 0x0}},		/* ra */
    {	{0x24, 0x6a},	{0x0, 0x0}},
    {	{0x24, 0x6b},	{0x0, 0x0}},
    {	{0x24, 0x6c},	{0x0, 0x0}},
    {	{0x24, 0x6d},	{0x0, 0x0}},

    {	{0x24, 0x2c},	{0x0, 0x0}},		/* ga */
    {	{0x24, 0x2e},	{0x0, 0x0}},
    {	{0x24, 0x30},	{0x0, 0x0}},
    {	{0x24, 0x32},	{0x0, 0x0}},
    {	{0x24, 0x34},	{0x0, 0x0}},

    {	{0x24, 0x36},	{0x0, 0x0}},		/* za */
    {	{0x24, 0x38},	{0x0, 0x0}},
    {	{0x24, 0x3a},	{0x0, 0x0}},
    {	{0x24, 0x3c},	{0x0, 0x0}},
    {	{0x24, 0x3e},	{0x0, 0x0}},

    {	{0x24, 0x40},	{0x0, 0x0}},		/* da */
    {	{0x24, 0x42},	{0x0, 0x0}},
    {	{0x24, 0x45},	{0x0, 0x0}},
    {	{0x24, 0x47},	{0x0, 0x0}},
    {	{0x24, 0x49},	{0x0, 0x0}},

    {	{0x24, 0x50},	{0x0, 0x0}},		/* ba */
    {	{0x24, 0x53},	{0x0, 0x0}},
    {	{0x24, 0x56},	{0x0, 0x0}},
    {	{0x24, 0x59},	{0x0, 0x0}},
    {	{0x24, 0x5c},	{0x0, 0x0}},

    {	{0x24, 0x51},	{0x0, 0x0}},		/* pa */
    {	{0x24, 0x54},	{0x0, 0x0}},
    {	{0x24, 0x57},	{0x0, 0x0}},
    {	{0x24, 0x5a},	{0x0, 0x0}},
    {	{0x24, 0x5d},	{0x0, 0x0}},

 /* and now the weird stuff */

    {	{0x24, 0x64},	{0x0, 0x0}},		/* ya */
    {	{0x24, 0x79},	{0x0, 0x0}},		/* blank */
    {	{0x24, 0x66},	{0x0, 0x0}},		/* yu */
    {	{0x24, 0x79},	{0x0, 0x0}},		/* blank */
    {	{0x24, 0x68},	{0x0, 0x0}},		/* yo */

    {	{0x24, 0x63},	{0x0, 0x0}},		/* small ya */
    {	{0x24, 0x79},	{0x0, 0x0}},		/* blank */
    {	{0x24, 0x65},	{0x0, 0x0}},		/* small yu */
    {	{0x24, 0x79},	{0x0, 0x0}},		/* blank */
    {	{0x24, 0x67},	{0x0, 0x0}},		/* small yo */

    {	{0x24, 0x6f},	{0x0, 0x0}},		/* wa */
    {	{0x24, 0x70},	{0x0, 0x0}},		/* wi */
    {	{0x24, 0x26},	{0x0, 0x0}},		/* (w)u */
    {	{0x24, 0x71},	{0x0, 0x0}},		/* we */
    {	{0x24, 0x72},	{0x0, 0x0}},		/* wo */

    {   {0x24, 0x21},   {0x0, 0x0}},            /* small `a' series */
    {   {0x24, 0x23},   {0x0, 0x0}},
    {   {0x24, 0x25},   {0x0, 0x0}},
    {   {0x24, 0x27},   {0x0, 0x0}},
    {   {0x24, 0x29},   {0x0, 0x0}},

    {   {0x24, 0x69},   {0x0, 0x0}},            /* small wa */
    {   {0x24, 0x43},   {0x0, 0x0}},            /* small tsu */
    {   {0x24, 0x73},   {0x0, 0x0}},            /* n */
};

XChar2b service_keys[KBD_MAX][2] = { 
    {	{0x22, 0x2b},	{0x0, 0x0}},		/* <- */
    {	{0x22, 0x4e},	{0x0, 0x0}},		/* kata/hiragana mode toggle */
    {   {0x3c, 0x21},   {0x0, 0x0}},            /* tsugi [accept input >]*/
    {   {0x41, 0x30},   {0x0, 0x0}},            /* mae [accept input <]*/
    {   {0x21, 0x2b},   {0x0, 0x0}},            /* nigori */
    {   {0x21, 0x2c},   {0x0, 0x0}},            /* o */
};

XChar2b blank[] = { {0x24, 0x79}, {0x0, 0x0}};	/* blank */

int gojuon_layout[11][3][5] = {
    {{  KANA_A,       KANA_I,       KANA_U,       KANA_E,       KANA_O }, 
     {  KANA_SMALL_A, KANA_SMALL_I, KANA_SMALL_U, KANA_SMALL_E, KANA_SMALL_O },
     { -1, -1, -1, -1, -1 }},

    {{  KANA_KA,      KANA_KI,      KANA_KU,      KANA_KE,      KANA_KO }, 
     {  KANA_GA,      KANA_GI,      KANA_GU,      KANA_GE,      KANA_GO },  
     { -1, -1, -1, -1, -1 }},
    
    {{  KANA_SA,      KANA_SHI,     KANA_SU,      KANA_SE,      KANA_SO },
     {  KANA_ZA,      KANA_ZI,      KANA_ZU,      KANA_ZE,      KANA_ZO }, 
     { -1, -1, -1, -1, -1 }},
    
    {{  KANA_TA,      KANA_CHI,     KANA_TSU,     KANA_TE,      KANA_TO }, 
     {  KANA_DA,      KANA_DI,      KANA_DU,      KANA_DE,      KANA_DO }, 
     { -1, -1, 77, -1, -1 }},
    
    {{  KANA_NA,      KANA_NI,      KANA_NU,      KANA_NE,      KANA_NO }, 
     { -1, -1, -1, -1, -1 },
     { -1, -1, -1, -1, -1 }},

    {{  KANA_HA,      KANA_HI,      KANA_FU,      KANA_HE,      KANA_HO }, 
     {  KANA_BA,      KANA_BI,      KANA_BU,      KANA_BE,      KANA_BO }, 
     {  KANA_PA,      KANA_PI,      KANA_PU,      KANA_PE,      KANA_PO }},
    
    {{  KANA_MA,      KANA_MI,      KANA_MU,      KANA_ME,      KANA_MO }, 
     { -1, -1, -1, -1, -1 },
     { -1, -1, -1, -1, -1 }},

    {{  KANA_RA,      KANA_RI,      KANA_RU,      KANA_RE,      KANA_RO }, 
     { -1, -1, -1, -1, -1 },
     { -1, -1, -1, -1, -1 }},

    {{  KANA_YA,      KANA_BLANK1,  KANA_YU,      KANA_BLANK2,  KANA_YO }, 
     {  KANA_SMALL_YA,KANA_BLANK1,  KANA_SMALL_YU,KANA_BLANK2,  KANA_SMALL_YO }, 
     { -1, -1, -1, -1, -1 }},

    {{  KANA_WA,      KANA_WI,      KANA_WU,      KANA_WE,      KANA_WO }, 
     {  KANA_SMALL_WA,     -1,           -1,           -1,           -1 },
     { -1, -1, -1, -1, -1 }},

    {{  KANA_N, -1, -1, -1, -1, },
     {  KANA_SMALL_TSU, -1, -1, -1, -1 },
     { -1, -1, -1, -1, -1 }},
};

Keyboard keyboard = {
    gojuon_layout,
};
Widget romajiinput;

XChar2b accept_label[][2] = {
    {{0x3c, 0x21}, {0, 0}}, /* tsugi */
    {{0x41, 0x30}, {0, 0}}, /* mae */
};

#if 0
static char *romaji_translations[NUM_OF_KW] = {
    "A", "I", "U", "E", "O",
    "KA", "KI", "KU", "KE", "KO",
    "SA", "SHI", "SU", "SE", "SO",
    "TA", "CHI", "TSU", "TE", "TO",
    "NA", "NI", "NU", "NE", "NO",
    "HA", "HI", "FU", "HE", "HO",
    "MA", "MI", "MU", "ME", "MO",
    "RA", "RI", "RU", "RE", "RO",
    "GA", "GI", "GU", "GE", "GO",
    "ZA", "ZI", "ZU", "ZE", "ZO",
    "DA", "DI", "DU", "DE", "DO",
    "BA", "BI", "BU", "BE", "BO",
    "PA", "PI", "PU", "PE", "PO",
    "YA", "", "YU", "", "YO",
    "ya", "", "yu", "", "yo",
    "WA", "wa", "tsu", "N", "O"
};
#endif

/************************************************************/

/*
 * static char *searchAccel =
 * " <Key>Return:  do-find()";
 */

static struct popup_info kana_popup_info = {
    "kana input", NULL, -1, 0, 0, NULL,
};


XChar2b
kana_vowel(int c)
{
    switch (c) {
    case 'a':
	return std_translations[KANA_A][0];
    case 'i':
	return std_translations[KANA_I][0];
    case 'u':
	return std_translations[KANA_U][0];
    case 'e':
	return std_translations[KANA_E][0];
    case 'o':
	return std_translations[KANA_O][0];
    }
    return blank[0]; 
}

XChar2b
kana_syllable(int cons, int vowel)
{
    int ind;
    
    switch (vowel) {
    case 'a':
	ind = 0;
	break;
    case 'i':
	ind = 1;
	break;
    case 'u':
	ind = 2;
	break;
    case 'e':
	ind = 3;
	break;
    case 'o':
	ind = 4;
	break;
    default:
	return blank[0];
    }
    
    switch (cons) {
    case 'k':
	return std_translations[KANA_KA+ind][0];
    case 'g':
	return std_translations[KANA_GA+ind][0];
    case 's':
	return std_translations[KANA_SA+ind][0];
    case 'z':
	return std_translations[KANA_ZA+ind][0];
    case 't':
	return std_translations[KANA_TA+ind][0];
    case 'd':
	return std_translations[KANA_DA+ind][0];
    case 'n':
	return std_translations[KANA_NA+ind][0];
    case 'h':
    case 'f':
	return std_translations[KANA_HA+ind][0];
    case 'p':
	return std_translations[KANA_PA+ind][0];
    case 'b':
	return std_translations[KANA_BA+ind][0];
    case 'm':
	return std_translations[KANA_MA+ind][0];
    case 'r':
	return std_translations[KANA_RA+ind][0];
    case 'y':
	return std_translations[KANA_YA+ind][0];
    case 'w':
	return std_translations[KANA_WA+ind][0];
    }
    return blank[0];
}

void 
stopCallback(Widget widget, XtPointer closure, XtPointer call_data)
{
    struct popup_info * popup = (struct popup_info *)closure;
    XtPopdown(popup->widget);
    popup->up = 0;
}

void
AddCloseCommand(struct popup_info *popup, Widget w)
{
    XtAddCallback(w, XtNcallback, stopCallback, (XtPointer)popup);
}

/* This gets called after clicking on the "Kana search" button.
 * It pops up the kana input window
 */
void
Showpopupkana(Widget w, XtPointer client_data, XtPointer call_data)
{
    Showpopup(w, &kana_popup_info);
}

void
kanacallback(Widget w, XtPointer data, XtPointer call_data)
{
    XChar2b ch;
    int n = (int)data;

    ch = get_key_label(&keyboard, n/5, n%5)[0];
    clearkbdstate(&keyboard);
    
    DEBUG(("Kana input clicked on."));

    if (((ch.byte1 == 0x21) && (ch.byte2 == 0x21)) || ch.byte1 == 0) {
        DEBUG(("Got blank... skipping... %x %x", ch.byte1, ch.byte2));
	return;
    }

    DEBUG(("Got data %x", data));

    process_kinput(ch);
}


enum kana_mode {
    HK_Toggle = -1,
    HK_Katakana,
    HK_Hiragana
};
static enum kana_mode popup_hirakana_mode = HK_Hiragana;

void
set_kana_mode(enum kana_mode mode)
{
    int kcount;
#if 0
    if (config.romaji) {
	/* if romaji is set, it doesn't matter what the other is */
	return;
    }
#endif    
    if (popup_hirakana_mode == mode)
	return;
    if (mode == HK_Toggle)
	popup_hirakana_mode = !popup_hirakana_mode;
    else
	popup_hirakana_mode = mode;
    if (popup_hirakana_mode == HK_Hiragana) {
	/* need to switch to hiragana */
	for (kcount = 0; kcount < XtNumber(std_translations); kcount++) 
	    std_translations[kcount][0].byte1 = 0x24;
    } else {
	/* bump all the std_translations to katakana */
	for (kcount = 0; kcount < XtNumber(std_translations); kcount++) 
	    std_translations[kcount][0].byte1 = 0x25;
    }
    setkbdstate(&keyboard, keyboard.state);
}

/*
 * ksearch_toggles() sets the labels for the point-n-click popup window
 *
 * It has to deal with romaji, hiragana, or kanakana
 *
 * If call_data == 1, just listen to config.romaji( romaji vs kana),
 *   and change all the labels
 * If call_data == 2,
 *    actually set global variable to toggle between hiragana and katakana
 *    and go set all the labels.
 *    (or leave as romaji, if config.romaji)
 */

void
ksearch_toggles(Widget w, XtPointer data, XtPointer call_data)
{
    if ((int) data == 2) {
#if 0
	if (config.romaji) {
	    /* nothing left to do. Stick with romaji*/
	    return;
	}
#endif	
    }
    /* assume data == 1 */
    set_kana_mode(HK_Toggle);
}

void
kana_backspace(Widget w, XtPointer data, XtPointer call_data)
{
    process_backspace();
}

void
state_toggles(Widget w, XtPointer data, XtPointer call_data)
{
    setkbdstate(&keyboard, (int)data);
}

/*************************************************************
 *    Widget Creation Below Here                             *
 *************************************************************/

XChar2b *
get_key_label(Keyboard *kbd, int i, int j)
{
    int  n = kbd->layout[i][kbd->state][j];
    if (n < 0)
	return blank;
    else
	return std_translations[n];
}

void
setkbdstate(Keyboard *kbd, int state)
{
    if (state >= 0 && state < 3) {
	int i, j, num;
	XChar2b *str;
	
	kbd->state = state;
	num = 0;
	for (i = 0; i < 11; i++) {
	    for (j = 0; j < 5; j++) {
		str = get_key_label(kbd, i, j);
		XtVaSetValues(kbd->kw[num], XtNlabel, str, NULL);
		num++;
	    }
	}
    }
}

void
clearkbdstate(Keyboard *kbd)
{
    if (kbd->state)
	setkbdstate(kbd, 0);
}

void
acceptkana(Widget w, XtPointer data, XtPointer call_data)
{
    UnhighlightSearchWidget(w, data, call_data);
    lookup_yomi((Matchdir)data);
}

void
switch_mode(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    if (*num_params == 0) {
	set_kana_mode(HK_Toggle);
	return;
    }

    if (*num_params != 1) {
	parm_cnt_error("switch_mode", 1, *num_params);
	return;
    }
    if (strcasecmp(params[0], "katakana") == 0)
	set_kana_mode(HK_Katakana);
    else if (strcasecmp(params[0], "hiragana") == 0)
	set_kana_mode(HK_Hiragana);
    else if (strcasecmp(params[0], "toggle") == 0)
	set_kana_mode(HK_Toggle);
    else
	logmsg(L_WARN, 0, "switch-mode(): invalid mode: %s", params[0]);
}

/* This is for the kana input window */

void
makeinputwidgets(Widget parent)
{
    int i, j, max_kana = 0;
    char namestr[10];

    for (i = 0; i < 11; i++) {
	for (j = 0; j < 5; j++) {
	    sprintf(namestr, "%d", max_kana);
	    keyboard.kw[max_kana] =
		XtVaCreateWidget(namestr,
				 commandWidgetClass,
				 parent,
				 XtNlabel, blank,
				 XtNinternational, False,
				 XtNencoding, XawTextEncodingChar2b,
				 XtNfont, smallkfont,
				 NULL);
	    if (max_kana > 4) {
		XtVaSetValues(keyboard.kw[max_kana],
			      XtNfromHoriz, keyboard.kw[max_kana - 5],
			      NULL);
	    }
	    if (max_kana % 5 > 0) {
		XtVaSetValues(keyboard.kw[max_kana],
			      XtNfromVert, keyboard.kw[max_kana - 1],
			      NULL);
	    }
	    XtAddCallback(keyboard.kw[max_kana],
			  XtNcallback, kanacallback,
			  (XtPointer) max_kana);
	    max_kana++;
	}
    }

    XtManageChildren(keyboard.kw, max_kana);
    setkbdstate(&keyboard, 0);

    keyboard.w[BACKSPACE] =
	XtVaCreateManagedWidget("bs", commandWidgetClass,
				parent,
				XtNlabel, service_keys[BACKSPACE],
				XtNinternational, False,
				XtNencoding, XawTextEncodingChar2b,
				XtNfont, smallkfont,
				XtNfromHoriz, keyboard.kw[max_kana-1],
				XtNhorizDistance, 40,
				NULL);
    XtAddCallback(keyboard.w[BACKSPACE],
		  XtNcallback, kana_backspace,
		  (XtPointer) 0);

    /* create the kana/hiragana toggle */
    keyboard.w[KANA_TOGGLE] =
	XtVaCreateManagedWidget("kanahiratoggle", commandWidgetClass,
				parent,
				XtNlabel, service_keys[KANA_TOGGLE],
				XtNinternational, False,
				XtNencoding, XawTextEncodingChar2b,
				XtNfont, smallkfont,
				XtNfromHoriz, keyboard.w[BACKSPACE],
				NULL);
    XtAddCallback(keyboard.w[KANA_TOGGLE],
		  XtNcallback, ksearch_toggles,
		  (XtPointer) 2);

    keyboard.w[STATE_1] =
	XtVaCreateManagedWidget("state1", commandWidgetClass,
				parent,
				XtNlabel, service_keys[STATE_1],
				XtNinternational, False,
				XtNencoding, XawTextEncodingChar2b,
				XtNfont, smallkfont,
				XtNfromHoriz, keyboard.kw[max_kana-1],
				XtNhorizDistance, 40,
				XtNfromVert, keyboard.w[KANA_TOGGLE],
				NULL);
    keyboard.w[STATE_2] =
	XtVaCreateManagedWidget("state2", commandWidgetClass,
				parent,
				XtNlabel, service_keys[STATE_2],
				XtNinternational, False,
				XtNencoding, XawTextEncodingChar2b,
				XtNfont, smallkfont,
				XtNfromHoriz, keyboard.w[STATE_1],
				XtNfromVert, keyboard.w[KANA_TOGGLE],
				NULL);

    XtAddCallback(keyboard.w[STATE_1],
		  XtNcallback, state_toggles,
		  (XtPointer) 1);

    XtAddCallback(keyboard.w[STATE_2],
		  XtNcallback, state_toggles,
		  (XtPointer) 2);

    romajiinput =
	XtVaCreateManagedWidget("romajiInput", labelWidgetClass,
				parent,
				XtNlabel, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!",
				XtNresize, False,
				XtNfromVert, keyboard.kw[max_kana-1],
				XtNvertDistance, 40,
				XtNinternational, False,
				XtNencoding, XawTextEncodingChar2b,
				XtNfont, smallkfont,
				NULL);
    /* make this a duplicate of the searchwidgets[SEARCH_KANA_W],
     * so user can see output of point-n-click on same window.
     * (or, user can even input romaji on that window!)
     */
    XtAddEventHandler(romajiinput,
		      KeyPressMask, False,
		      Handle_romajikana, NULL);
/*    XtOverrideTranslations(romajiinput, XtParseAcceleratorTable(searchAccel));
 */

    /* create the accept (find next) widget */
    keyboard.w[FIND_NEXT] =
	XtVaCreateManagedWidget("findNext", commandWidgetClass,
				parent,
				XtNlabel, service_keys[FIND_NEXT],
				XtNinternational, False,
				XtNencoding, XawTextEncodingChar2b,
				XtNfont, smallkfont,
				XtNfromHoriz, romajiinput,
/*				XtNhorizDistance, 40,*/
				XtNfromVert, keyboard.kw[max_kana-1],
				XtNvertDistance, 40,
				NULL);
    keyboard.w[FIND_PREV] =
	XtVaCreateManagedWidget("findPrev", commandWidgetClass,
				parent,
				XtNlabel, service_keys[FIND_PREV],
				XtNinternational, False,
				XtNencoding, XawTextEncodingChar2b,
				XtNfont, smallkfont,
				XtNfromHoriz, keyboard.w[FIND_NEXT],
				XtNfromVert, keyboard.kw[max_kana-1],
				XtNvertDistance, 40,
				NULL);

    XtAddCallback(keyboard.w[FIND_NEXT], 
		  XtNcallback, acceptkana,
		  (XtPointer) MatchNext);

    XtAddCallback(keyboard.w[FIND_PREV],
		  XtNcallback, acceptkana,
		  (XtPointer) MatchPrev);
}

/* This is an exported "make widgets" routine */

void
MakeKanaInputPopup()
{
    Widget kanainputform;
/*    XtAccelerators Accel; */

    kana_popup_info.widget =
	XtVaCreatePopupShell("kanaInputShell",
			     transientShellWidgetClass,
			     toplevel,
			     NULL);

    kanainputform =
	XtVaCreateManagedWidget("kanaInputForm",
				formWidgetClass,
				kana_popup_info.widget,
				NULL);

    makeinputwidgets(kanainputform);

/*    Accel = XtParseAcceleratorTable(searchAccel);
    XtOverrideTranslations(kanainputform, Accel);*/
}
