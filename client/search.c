/* $Id$
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <X11/Xos.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <Xaw.h>

#include <stdarg.h>
#include <ctype.h>

#include <dict.h>
#include <gjdict.h>
#include <client.h>
#include <search.h>
#include <help.h>
#include <cystack.h>

enum {
    G_LABEL, G_INPUT, F_LABEL, F_INPUT, POUND_LABEL, POUND_INPUT,
    H_LABEL, H_INPUT, N_LABEL, N_INPUT, U_LABEL, U_INPUT,
    NUM_OF_N
};

enum {
    B_LABEL, B_INPUT,
    Q_LABEL, Q_INPUT,
    S_LABEL, S_INPUT,
    X_LABEL, X_INPUT,
    NUM_OF_KN
};

enum {
    NFORM_W, SFORM_W,
    NUM_OF_FORMS
};

enum {
    SEARCH_ENG_L, SEARCH_ENG_W,
    SEARCH_KANA_L, SEARCH_KANA_W,
    SEARCH_DROP_L, SEARCH_DROP_W,
    SEARCH_PINYIN_L, SEARCH_PINYIN_W,
    NUM_OF_W
};

enum {
    HB_PREV, HB_NEXT, HB_FINDPREV, HB_FINDNEXT,
    NUM_OF_HB
};

Widget searchshell;
Widget forms[NUM_OF_FORMS];
Widget searchwidgets[NUM_OF_W];
Widget searchnumbers[NUM_OF_N];
Widget keynumbers[NUM_OF_KN];
Widget hotbutton[NUM_OF_HB];
Widget kanji_w, yomi_w, pinyin_w, english_w;

Widget search_widget;
int search_tree;

static Cystack *lookup_ring;

void makenorth();
void makesouth();
void makekeys();
void makenumbers();

#define MAXKANALENGTH 80
static XChar2b kanastring[MAXKANALENGTH];

char *searchAccel = " <Key>Return:  lookup(forward)\n \
  Ctrl<Key>F:   lookup(forward)\n \
  Ctrl<Key>B:   lookup(backward)\n \
";

enum {
    COMMAND_MENU,
    OPTIONS_MENU,
    HELP_MENU,
    MAX_MENU
};

enum {
    CONNECT_MB,
    QUIT_MB,
    EDIT_OPTIONS_MB,
    SAVE_OPTIONS_MB,
    HELP_MB,
    LICENSE_MB,
    MAX_MB
};

Widget menuform;
Widget menu[MAX_MENU];
Widget menu_cmd[MAX_MB];
Widget menubutton[MAX_MENU];
Widget statusline;

#define STATUSDISTANCE 2

DictEntry last_entry;

char *dir_str[] = {
    "NEXT",
    "PREV"
};

void Handle_paste(Widget w, XtPointer closure, XEvent * e, Boolean * cont);

void
ring_alloc(int size)
{
    if (size <= 0)
	size = 16;
    lookup_ring = cystack_create(size + 1, sizeof(DictEntry));
    if (!lookup_ring)
	die(1, L_CRIT, 0, "not enough core");
}

void
ring_insert(DictEntry * entry)
{
    cystack_push(lookup_ring, entry);
}

int
ring_get(int dir, DictEntry * ep)
{
    DictEntry entry;
    if (cystack_pop(lookup_ring, &entry, dir))
	return -1;
    *ep = entry;
    return 0;
}

void
MakeMenu(Widget top)
{
    Widget w;
    XtCallbackRec consoleCallbackRec[] = {
	{ connectCallback, NULL },
	{ NULL }
    };
    XtCallbackRec quitCallbackRec[] = {
	{ quit, NULL },
	{ NULL }
    };
    XtCallbackRec editOptionsCallbackRec[] = {
	{ connectCallback, NULL },
	{ NULL }
    };
    XtCallbackRec saveOptionsCallbackRec[] = {
	{ quit, NULL },
	{ NULL }
    };
    XtCallbackRec licenseCallbackRec[] = {
	{ licenseDialog, NULL },
	{ NULL }
    };
    XtCallbackRec helpCallbackRec[] = {
	{ HelpCallback, NULL },
	{ NULL }
    };

    menuform =
	XtVaCreateManagedWidget("menuForm", formWidgetClass, top, NULL);

    menubutton[COMMAND_MENU] =
	XtVaCreateManagedWidget("commandMenuButton",
				menuButtonWidgetClass,
				menuform,
				XtNmenuName, "commandMenu", NULL);

    menu[COMMAND_MENU] =
	XtVaCreateManagedWidget("commandMenu",
				simpleMenuWidgetClass, top, NULL);

    menu_cmd[CONNECT_MB] =
	XtVaCreateManagedWidget("connect",
				smeBSBObjectClass,
				menu[COMMAND_MENU],
				XtNy, 1,
				XtNcallback, consoleCallbackRec, NULL);
    w = XtVaCreateManagedWidget("line",
				smeLineObjectClass,
				menu[COMMAND_MENU], XtNy, 2, NULL);
    menu_cmd[QUIT_MB] =
	XtVaCreateManagedWidget("quit",
				smeBSBObjectClass,
				menu[COMMAND_MENU],
				XtNy, 3,
				XtNcallback, quitCallbackRec, NULL);

    /* Options menu */

    menubutton[OPTIONS_MENU] =
	XtVaCreateManagedWidget("optionsMenuButton",
				menuButtonWidgetClass,
				menuform,
				XtNfromHoriz, menubutton[COMMAND_MENU],
				XtNmenuName, "optionsMenu", NULL);

    menu[OPTIONS_MENU] =
	XtVaCreateManagedWidget("optionsMenu",
				simpleMenuWidgetClass, top, NULL);

    menu_cmd[EDIT_OPTIONS_MB] =
	XtVaCreateManagedWidget("edit",
				smeBSBObjectClass,
				menu[OPTIONS_MENU],
				XtNy, 1,
				XtNcallback, editOptionsCallbackRec, NULL);
    menu_cmd[SAVE_OPTIONS_MB] =
	XtVaCreateManagedWidget("save",
				smeBSBObjectClass,
				menu[OPTIONS_MENU],
				XtNy, 2,
				XtNcallback, saveOptionsCallbackRec, NULL);

    /* Help menu */

    menubutton[HELP_MENU] =
	XtVaCreateManagedWidget("helpMenuButton",
				menuButtonWidgetClass,
				menuform,
				XtNfromHoriz, menubutton[OPTIONS_MENU],
				XtNmenuName, "helpMenu", NULL);

    menu[HELP_MENU] =
	XtVaCreateManagedWidget("helpMenu",
				simpleMenuWidgetClass, top, NULL);

    menu_cmd[HELP_MB] =
	XtVaCreateManagedWidget("help",
				smeBSBObjectClass,
				menu[HELP_MENU],
				XtNy, 1,
				XtNcallback, helpCallbackRec, NULL);

    w = XtVaCreateManagedWidget("line",
				smeLineObjectClass,
				menu[HELP_MENU], XtNy, 2, NULL);

    menu_cmd[LICENSE_MB] =
	XtVaCreateManagedWidget("license",
				smeBSBObjectClass,
				menu[HELP_MENU],
				XtNy, 3,
				XtNcallback, licenseCallbackRec, NULL);
}


void
MakeSearchWidget(Widget shell)
{
    Widget searchform, buttonform;
    XtAccelerators Accel;
    XtCallbackRec cbr[] = {
	{ NULL, NULL },
	{ NULL, NULL }
    };

    ring_alloc(config.history_size);

    searchshell = shell;
    searchform =
	XtVaCreateManagedWidget("searchForm",
				formWidgetClass, searchshell, NULL);

    MakeMenu(searchform);

    buttonform =
	XtVaCreateManagedWidget("buttonForm",
				formWidgetClass,
				searchform, XtNfromVert, menuform, NULL);

    cbr[0].callback = histCallback;
    cbr[0].closure = (XtPointer) STACK_DN;
    hotbutton[HB_NEXT] =
	XtVaCreateManagedWidget("next",
				commandWidgetClass,
				buttonform, XtNcallback, cbr, NULL);


    cbr[0].closure = (XtPointer) STACK_UP;
    hotbutton[HB_PREV] =
	XtVaCreateManagedWidget("prev",
				commandWidgetClass,
				buttonform,
				XtNfromHoriz, hotbutton[HB_NEXT],
				XtNcallback, cbr, NULL);

    cbr[0].callback = findNextCallback;
    hotbutton[HB_FINDNEXT] =
	XtVaCreateManagedWidget("findNext",
				commandWidgetClass,
				buttonform,
				XtNfromHoriz, hotbutton[HB_PREV],
				XtNcallback, cbr, NULL);

    cbr[0].callback = findPrevCallback;
    hotbutton[HB_FINDPREV] =
	XtVaCreateManagedWidget("findPrev",
				commandWidgetClass,
				buttonform,
				XtNfromHoriz, hotbutton[HB_FINDNEXT],
				XtNcallback, cbr, NULL);

    statusline =
	XtVaCreateManagedWidget("status",
				labelWidgetClass,
				searchform,
				XtNborderWidth, 2,
				XtNvertDistance, STATUSDISTANCE,
				XtNfromVert, buttonform,
				XtNresize, True, NULL);


    forms[NFORM_W] =
	XtVaCreateManagedWidget("northForm",
				formWidgetClass,
				searchform,
				XtNfromVert, statusline,
				XtNborderWidth, 2, NULL);

    makenorth(forms[NFORM_W]);

    forms[SFORM_W] =
	XtVaCreateManagedWidget("southForm",
				formWidgetClass,
				searchform,
				XtNfromVert, forms[NFORM_W],
				XtNborderWidth, 2, NULL);

    makesouth(forms[SFORM_W]);

    Accel = XtParseAcceleratorTable(searchAccel);
    XtOverrideTranslations(searchwidgets[SEARCH_ENG_W], Accel);
    XtOverrideTranslations(searchwidgets[SEARCH_KANA_W], Accel);
    if (searchwidgets[SEARCH_PINYIN_W])
	XtOverrideTranslations(searchwidgets[SEARCH_PINYIN_W], Accel);

    XtOverrideTranslations(searchnumbers[F_INPUT], Accel);
    XtOverrideTranslations(searchnumbers[POUND_INPUT], Accel);
    XtOverrideTranslations(searchnumbers[H_INPUT], Accel);
    XtOverrideTranslations(searchnumbers[N_INPUT], Accel);
    XtOverrideTranslations(searchnumbers[U_INPUT], Accel);
    XtOverrideTranslations(keynumbers[Q_INPUT], Accel);
    XtOverrideTranslations(keynumbers[S_INPUT], Accel);
    XtOverrideTranslations(keynumbers[B_INPUT], Accel);

    MakeKanaInputPopup();
    MakeCornerInputPopup();
    MakeBushuInputPopup();
    MakeSkipInputPopup();
    MakeXrefPopup();
}

/* make "north" widgets */
void
makenorth(Widget parent)
{
    Widget keyform, numform;
    int n, height;

    kanji_w =
	XtVaCreateManagedWidget("kanjilarge",
				labelWidgetClass,
				parent,
				XtNlabel, "", NULL);

    yomi_w =
	XtVaCreateManagedWidget("yomiText",
				asciiTextWidgetClass,
				parent,
				XtNstring, "                        ",
				XtNinternational, True,
				XtNfromHoriz, kanji_w,
				XtNdisplayCaret, False,
				XtNwrap, XawtextWrapWord,
				XtNscrollVertical, myXawtextScrollWhenNeeded,
				NULL);


    pinyin_w =
	XtVaCreateManagedWidget("pinyinText",
				asciiTextWidgetClass,
				parent,
				XtNlabel, "                           ",
				XtNfromHoriz, yomi_w,
				XtNdisplayCaret, False,
				XtNscrollVertical, myXawtextScrollWhenNeeded,
				NULL);

    keyform =
	XtVaCreateManagedWidget("keyform",
				formWidgetClass,
				parent, XtNfromVert, kanji_w, NULL);

    makekeys(keyform);

    numform =
	XtVaCreateManagedWidget("numform",
				formWidgetClass,
				parent,
				XtNfromHoriz, keyform,
				XtNfromVert, kanji_w, NULL);
    makenumbers(numform);

    english_w =
	XtVaCreateManagedWidget("englishText",
				asciiTextWidgetClass,
				parent,
				XtNstring,
				"                                       ",
				XtNfromVert, kanji_w,
				XtNfromHoriz, numform,
				XtNdisplayCaret, False,
				XtNwrap, XawtextWrapWord,
				XtNscrollVertical, myXawtextScrollWhenNeeded,
				NULL);

    n = GetSubResourceInt(english_w, "numrows", "Numrows");
    if (n < 2)
	n = 2;
    XtVaGetValues(english_w, XtNheight, &height, NULL);
    XtVaSetValues(english_w, XtNheight, n * height, NULL);
}


void
makenumbers(Widget parent)
{
    searchnumbers[G_LABEL] =
	XtVaCreateWidget("gradeLabel",
			 labelWidgetClass, parent,
			 XtNborderWidth, 0, NULL);

    searchnumbers[G_INPUT] =
	XtVaCreateWidget("gradeInput",
			 asciiTextWidgetClass, parent,
			 XtNfromHoriz, searchnumbers[G_LABEL],
			 XtNstring, "   ", XtNdisplayCaret, False, NULL);

    searchnumbers[F_LABEL] =
	XtVaCreateWidget("freqLabel",
			 labelWidgetClass, parent,
			 XtNfromVert, searchnumbers[G_INPUT],
			 XtNborderWidth, 0, NULL);

    searchnumbers[F_INPUT] =
	XtVaCreateWidget("freqInput",
			 asciiTextWidgetClass, parent,
			 XtNfromVert, searchnumbers[G_INPUT],
			 XtNfromHoriz, searchnumbers[F_LABEL],
			 XtNstring, "", XtNeditType, XawtextEdit, NULL);

    searchnumbers[H_LABEL] =
	XtVaCreateWidget("halpernLabel",
			 labelWidgetClass, parent,
			 XtNfromVert, searchnumbers[F_LABEL],
			 XtNborderWidth, 0, NULL);

    searchnumbers[H_INPUT] =
	XtVaCreateWidget("halpernInput",
			 asciiTextWidgetClass, parent,
			 XtNfromVert, searchnumbers[F_LABEL],
			 XtNfromHoriz, searchnumbers[H_LABEL],
			 XtNstring, " ", XtNeditType, XawtextEdit, NULL);


    searchnumbers[N_LABEL] =
	XtVaCreateWidget("nelsonLabel",
			 labelWidgetClass, parent,
			 XtNfromVert, searchnumbers[H_LABEL],
			 XtNborderWidth, 0, NULL);

    searchnumbers[N_INPUT] =
	XtVaCreateWidget("nelsonInput",
			 asciiTextWidgetClass, parent,
			 XtNfromVert, searchnumbers[H_LABEL],
			 XtNfromHoriz, searchnumbers[N_LABEL],
			 XtNstring, " ", XtNeditType, XawtextEdit, NULL);

    searchnumbers[POUND_LABEL] =
	XtVaCreateWidget("jisLabel",
			 labelWidgetClass, parent,
			 XtNfromVert, searchnumbers[N_LABEL],
			 XtNborderWidth, 0, NULL);

    searchnumbers[POUND_INPUT] =
	XtVaCreateWidget("jisInput",
			 asciiTextWidgetClass, parent,
			 XtNfromVert, searchnumbers[N_LABEL],
			 XtNfromHoriz, searchnumbers[POUND_LABEL],
			 XtNstring, "", XtNeditType, XawtextEdit, NULL);

    searchnumbers[U_LABEL] =
	XtVaCreateWidget("unicodeLabel",
			 labelWidgetClass, parent,
			 XtNfromVert, searchnumbers[POUND_LABEL],
			 XtNborderWidth, 0, NULL);

    searchnumbers[U_INPUT] =
	XtVaCreateWidget("unicodeInput",
			 asciiTextWidgetClass, parent,
			 XtNfromVert, searchnumbers[POUND_LABEL],
			 XtNfromHoriz, searchnumbers[U_LABEL],
			 XtNstring, " ", XtNeditType, XawtextEdit, NULL);


    XtManageChildren(searchnumbers, NUM_OF_N);
}

void
makesouth(Widget parent)
{
    Widget w;
    XtCallbackRec cbr[] = {
	{ UnhighlightSearchWidget, NULL },
	{ NULL, NULL },
	{ NULL, NULL }
    };

    w = XtVaCreateManagedWidget("searchLabel",
				labelWidgetClass,
				parent, XtNborderWidth, 0, NULL);

    searchwidgets[SEARCH_ENG_L] =
	XtVaCreateWidget("searchEnglishLabel",
			 labelWidgetClass,
			 parent, XtNjustify, XtJustifyRight,
			 /*XtNvertDistance, 20, */
			 XtNborderWidth, 0, XtNfromVert, w, NULL);

    searchwidgets[SEARCH_ENG_W] =
	XtVaCreateWidget("searchEnglish",
			 asciiTextWidgetClass,
			 parent,
			 XtNfromVert, w,
			 XtNfromHoriz, searchwidgets[SEARCH_ENG_L],
			 /*XtNvertDistance, 20, */
			 XtNeditType, XawtextEdit, NULL);

    cbr[1].callback = Showpopupkana;
    searchwidgets[SEARCH_KANA_L] =
	XtVaCreateWidget("searchKanaLabel",
			 toggleWidgetClass,
			 parent,
			 XtNcallback, cbr,
			 XtNjustify, XtJustifyRight,
			 XtNborderWidth, 0,
			 XtNfromVert, searchwidgets[SEARCH_ENG_L], NULL);

    searchwidgets[SEARCH_KANA_W] =
	XtVaCreateWidget("searchKana",
			 labelWidgetClass, parent, XtNlabel, "\0",
			 /*XtNlabel, "!z", *//* 0x217a */
			 XtNinternational, False,
			 XtNfromVert, searchwidgets[SEARCH_ENG_L],
			 XtNfromHoriz, searchwidgets[SEARCH_KANA_L],
			 XtNfont, smallkfont,
			 XtNencoding, XawTextEncodingChar2b,
			 XtNjustify, XtJustifyLeft, NULL);

    searchwidgets[SEARCH_DROP_L] =
	XtVaCreateWidget("searchDropLabel",
			 labelWidgetClass,
			 parent,
			 XtNjustify, XtJustifyLeft,
			 XtNborderWidth, 0,
			 XtNfromVert, searchwidgets[SEARCH_KANA_W], NULL);

    searchwidgets[SEARCH_DROP_W] =
	XtVaCreateWidget("searchDropInput",
			 asciiTextWidgetClass,
			 parent,
			 XtNstring, "",
			 XtNdisplayCaret, False,
			 XtNeditType, XawtextEdit,
			 XtNfromHoriz, searchwidgets[SEARCH_KANA_L],
			 XtNfromVert, searchwidgets[SEARCH_KANA_W], NULL);

    XtAddEventHandler(searchwidgets[SEARCH_DROP_W],
		      ButtonReleaseMask, False, Handle_paste, NULL);

    searchwidgets[SEARCH_PINYIN_L] =
	XtVaCreateWidget("searchPinyinLabel",
			 labelWidgetClass,
			 parent, XtNjustify, XtJustifyRight,
			 /*XtNvertDistance, 20, */
			 XtNborderWidth, 0,
			 XtNfromVert, searchwidgets[SEARCH_DROP_W], NULL);

    searchwidgets[SEARCH_PINYIN_W] =
	XtVaCreateWidget("searchPinyin",
			 asciiTextWidgetClass,
			 parent,
			 XtNfromVert, searchwidgets[SEARCH_DROP_W],
			 XtNfromHoriz, searchwidgets[SEARCH_PINYIN_L],
			 /*XtNvertDistance, 20, */
			 XtNeditType, XawtextEdit, NULL);

    XtAddEventHandler(searchwidgets[SEARCH_KANA_W],
		      KeyPressMask, False, Handle_romajikana, NULL);

    XtManageChildren(searchwidgets, NUM_OF_W);
}

void
makekeys(Widget parent)
{
    XtCallbackRec cbr[] = {
	{ UnhighlightSearchWidget, NULL },
	{ NULL, NULL },
	{ NULL, NULL }
    };

    cbr[1].callback = Showpopupbushu;
    keynumbers[B_LABEL] =
	XtVaCreateWidget("bushuLabel",
			 toggleWidgetClass, parent,
			 XtNcallback, cbr,
			 XtNjustify, XtJustifyRight,
			 XtNborderWidth, 0, NULL);

    keynumbers[B_INPUT] =
	XtVaCreateWidget("bushuInput",
			 asciiTextWidgetClass, parent,
			 XtNfromHoriz, keynumbers[B_LABEL],
			 XtNstring, "   ", XtNeditType, XawtextEdit, NULL);

    cbr[1].callback = Showpopupcorner;
    keynumbers[Q_LABEL] =
	XtVaCreateWidget("cornerLabel",
			 toggleWidgetClass, parent,
			 XtNcallback, cbr,
			 XtNfromVert, keynumbers[B_INPUT],
			 XtNjustify, XtJustifyRight,
			 XtNborderWidth, 0, NULL);

    keynumbers[Q_INPUT] =
	XtVaCreateWidget("cornerInput",
			 asciiTextWidgetClass, parent,
			 XtNfromVert, keynumbers[B_LABEL],
			 XtNfromHoriz, keynumbers[Q_LABEL],
			 XtNstring, "   ", XtNeditType, XawtextEdit, NULL);

    cbr[1].callback = Showpopupskip;
    keynumbers[S_LABEL] =
	XtVaCreateWidget("skipLabel",
			 toggleWidgetClass, parent,
			 XtNcallback, cbr,
			 XtNfromVert, keynumbers[Q_INPUT],
			 XtNborderWidth, 0, NULL);

    keynumbers[S_INPUT] =
	XtVaCreateWidget("skipInput",
			 asciiTextWidgetClass, parent,
			 XtNfromVert, keynumbers[Q_LABEL],
			 XtNfromHoriz, keynumbers[S_LABEL],
			 XtNstring, " ", XtNeditType, XawtextEdit, NULL);

    cbr[1].callback = Showxrefs;
    keynumbers[X_LABEL] =
	XtVaCreateWidget("xrefLabel",
			 toggleWidgetClass, parent,
			 XtNcallback, cbr,
			 XtNfromVert, keynumbers[S_INPUT],
			 XtNborderWidth, 0,
			 XtNjustify, XtJustifyRight,
			 XtNsensitive, False, NULL);

    keynumbers[X_INPUT] =
	XtVaCreateWidget("xrefInput",
			 asciiTextWidgetClass, parent,
			 XtNfromVert, keynumbers[S_LABEL],
			 XtNfromHoriz, keynumbers[X_LABEL],
			 XtNstring, " ", XtNdisplayCaret, False, NULL);

    XtManageChildren(keynumbers, NUM_OF_KN);
}

void
process_backspace()
{
    XChar2b *kparse = kanastring;
    int kanalength;

    for (kanalength = 0; kparse->byte1 != 0; kanalength++) {
	kparse++;
    }
    if (kanalength == 0)
	return;
    kanalength--;
    kanastring[kanalength].byte1 = 0;
    kanastring[kanalength].byte2 = 0;

    XtVaSetValues(searchwidgets[SEARCH_KANA_W],
		  XtNlabel, kanastring, NULL);
    XtVaSetValues(romajiinput, XtNlabel, kanastring, NULL);
}


/* process_kinput
 *	accepts a single 2-byte char as input.
 *	Adjusts the SEARCH_KANA_W widget labelstring appropriately
 *	(allowing backspacing)
 *	Also handles the mirror widget on the popup window
 *
 *
 *	We call the convert routine "romajitokana" on our internal
 *	string, to handle romaji-to-kana stuffs.
 *	Note that we get called by both the point-n-click-kana window,
 *	AND by the keypress-event handler for the window.
 */
void
process_kinput(XChar2b in_char)
{
    XChar2b *kparse = kanastring;
    int kanalength;

    for (kanalength = 0; kparse->byte1 != 0; kanalength++) {
	kparse++;
    }

    if (kanalength < MAXKANALENGTH - 1) {
	kanastring[kanalength].byte1 = in_char.byte1;
	kanastring[kanalength].byte2 = in_char.byte2;

	kanalength++;
	kanastring[kanalength].byte1 = 0;
	kanastring[kanalength].byte1 = 0;
    }

    if (!config.romaji)
	romajitokana(kanastring);

    XtVaSetValues(searchwidgets[SEARCH_KANA_W],
		  XtNlabel, kanastring, NULL);
    XtVaSetValues(romajiinput, XtNlabel, kanastring, NULL);

}

void
handle_lookup_drop(Widget w, Matchdir dir)
{
    String str;

    XtVaGetValues(w, XtNstring, &str, NULL);
    if (!str)
	return;
    NewSearchWidget(w);
    euc_to_shin(str, strlen(str));
    lookup_kanji(str, dir);
}

void
Handle_paste(Widget w, XtPointer closure, XEvent * e, Boolean * cont)
{
    handle_lookup_drop(w, MatchNext);
}

void
display_entry(DictEntry * entry)
{
    char *kanjiptr;
    int kindex = 0;
    Boolean state;

    memcpy(&last_entry, entry, sizeof(last_entry));

    if (entry == NULL) {
	/* BLANK EVERYTHING */
	XtVaSetValues(kanji_w,
		      XtNlabel, "                               ", NULL);
	XtVaSetValues(english_w, XtNstring, "", NULL);
	XtVaSetValues(yomi_w, XtNstring, "", NULL);
	XtVaSetValues(pinyin_w, XtNstring, "", NULL);

	SetWidgetNumber(searchnumbers[G_INPUT], "%d", 0);
	SetWidgetNumber(searchnumbers[F_INPUT], "%d", 0);
	SetWidgetNumber(searchnumbers[POUND_INPUT], "%d", 0);
	SetWidgetNumber(searchnumbers[H_INPUT], "%d", 0);
	SetWidgetNumber(searchnumbers[N_INPUT], "%d", 0);
	SetWidgetNumber(searchnumbers[U_INPUT], "%d", 0);
	SetWidgetBushu(keynumbers[B_INPUT], 0, 0);
	SetWidgetNumber(keynumbers[Q_INPUT], "%04u", 0);
	SetWidgetSkipCode(keynumbers[S_INPUT], 0);
	SetWidgetNumber(keynumbers[X_INPUT], "%d", 0);
	XtVaGetValues(keynumbers[X_LABEL], XtNstate, &state, NULL);
	if (!state)
	    XtVaSetValues(keynumbers[X_LABEL], XtNsensitive, False, NULL);
	XtVaSetValues(kanji_w, XtNlabel, "", NULL);
	XtVaSetValues(keynumbers[X_LABEL], XtNstate, False, NULL);
	popdownxref();
    } else {
	if (entry->kanji == 0) {
	    kanjiptr = "";
	    kindex = 0;
	} else {
	    XChar2b *p;
	    kanjiptr = GetString(entry->kanji);
	    p = (XChar2b *) kanjiptr;
	    if (p[1].byte1 == 0) {
		kindex = p[0].byte1;
		kindex = kindex << 8;
		kindex |= p[0].byte2;
	    }
	    shin_to_euc(kanjiptr, strlen(kanjiptr));
	}
	XtVaSetValues(kanji_w, XtNlabel, kanjiptr, NULL);

	SetWidgetNumber(searchnumbers[G_INPUT], "%d",
			(int) entry->grade_level);
	SetWidgetNumber(searchnumbers[F_INPUT], "%d",
			(int) entry->frequency);
	SetWidgetNumber(searchnumbers[H_INPUT], "%d", (int) entry->Hindex);
	SetWidgetNumber(searchnumbers[N_INPUT], "%d", (int) entry->Nindex);
	SetWidgetNumber(searchnumbers[U_INPUT], "%x", (int) entry->Uindex);
	if (search_widget != keynumbers[B_INPUT])
	    SetWidgetBushu(keynumbers[B_INPUT], entry->bushu,
			   entry->numstrokes);
	SetWidgetNumber(keynumbers[Q_INPUT], "%04u", (int) entry->Qindex);
	SetWidgetSkipCode(keynumbers[S_INPUT], (int) entry->skip);
	SetWidgetNumber(keynumbers[X_INPUT], "%d", (int) entry->refcnt);
	XtVaGetValues(keynumbers[X_LABEL], XtNstate, &state, NULL);
	if (!state)
	    XtVaSetValues(keynumbers[X_LABEL],
			  XtNsensitive, entry->refcnt > 0, NULL);

	SetWidgetNumber(searchnumbers[POUND_INPUT], "%x", kindex);

	if (config.romaji) {
	    /* translate all kana into romaji */
	    /* translate to buffer, then set widget string */
	    char *romajiptr =
		kanatoromaji((XChar2b *) GetString(entry->pronunciation));
	    XtVaSetValues(yomi_w, XtNstring, romajiptr, NULL);
	} else {

	    XtVaSetValues(yomi_w,
			  XtNstring,
			  format_string16(GetString(entry->pronunciation)),
			  NULL);
	}

	XtVaSetValues(english_w,
		      XtNstring,
		      format_string(GetString(entry->english)), NULL);

	XtVaSetValues(pinyin_w,
		      XtNstring, format_string(GetString(entry->pinyin)),
		      NULL);
    }
}

int
set_search_key(char *str)
{
    sendf("KEY %s", str);
    if (get_reply() != RC_OK) {
	setstatus(True, reply_buf);
	return -1;
    }
    return 0;
}

int
lookup_record(Recno recno)
{
    DictEntry entry;

    if (open_data_connection()) {
	setstatus(False, "Data connection failed");
	return -1;
    }

    sendf("REC %u", recno);

    if (get_reply() == RC_OK && get_entry(&entry, 1) == 1) {
	ring_insert(&entry);
	display_entry(&entry);
	setstatus(False, "Ready");
    } else
	setstatus(False, "NOT found: %s", reply_buf);
    close_data_connection();
    return 0;
}

int
do_match_string(char *skey, char *string, Matchdir dir)
{
    DictEntry entry;

    if (set_search_key(skey))
	return -1;
    if (open_data_connection()) {
	setstatus(False, "Data connection failed");
	return -1;
    }

    sendf("ENTRY \"%s\" %s", escape_string(string), dir_str[dir]);
    if (get_reply() == RC_OK && get_entry(&entry, 1) == 1) {
	ring_insert(&entry);
	display_entry(&entry);
	setstatus(False, "Ready");
    } else
	setstatus(False, "NOT found: %s", reply_buf);
    close_data_connection();
    return 0;
}

int
do_match_number(char *skey, unsigned value, Matchdir dir, Boolean closest)
{
    DictEntry entry;

    if (set_search_key(skey))
	return -1;

    if (open_data_connection()) {
	setstatus(False, "Data connection failed");
	return -1;
    }

    if (closest)
	sendf("ENTRY \"%u\" CLOSEST", value);
    else
	sendf("ENTRY \"%u\" %s", value, dir_str[dir]);

    if (get_reply() == RC_OK && get_entry(&entry, 1) == 1) {
	ring_insert(&entry);
	display_entry(&entry);
	setstatus(False, "Ready");
    } else
	setstatus(False, "NOT found: %s", reply_buf);
    close_data_connection();
    return 0;
}


void
lookup_corner(int code, Matchdir dir)
{
    DictEntry entry;

    if (set_search_key("CORNER"))
	return;

    if (open_data_connection()) {
	setstatus(False, "Data connection failed");
	return;
    }

    sendf("ENTRY \"%04u\" %s", code, dir_str[dir]);

    if (get_reply() == RC_OK && get_entry(&entry, 1) == 1) {
	ring_insert(&entry);
	display_entry(&entry);
	setstatus(False, "Ready");
    } else
	setstatus(False, "NOT found: %s", reply_buf);
    close_data_connection();
}


void
lookup_skip(char *code, Matchdir dir)
{
    do_match_string("SKIP", code, dir);
}

void
UnhighlightSearchWidget(Widget w, XtPointer data, XtPointer calldata)
{
    if (search_widget) {
	XtVaSetValues(search_widget,
		      XtNbackground, default_background,
		      XtNforeground, default_foreground, NULL);
	search_widget = NULL;
    }
}

void
NewSearchWidget(Widget w)
{
    if (search_widget != w) {
	if (search_widget) {
	    XtVaSetValues(search_widget,
			  XtNbackground, default_background,
			  XtNforeground, default_foreground, NULL);
	}
	search_widget = w;
	XtVaSetValues(w,
		      XtNbackground, config.highlight.background,
		      XtNforeground, config.highlight.foreground, NULL);
    }
}

/* dict_lookup: main working horse
 */
void
lookup(Widget w, Matchdir dir)
{
    setstatus(False, "Searching...");

    NewSearchWidget(w);
    if (w == searchwidgets[SEARCH_ENG_W]) {
	handle_lookup_english(w, dir);
    } else if (w == searchwidgets[SEARCH_KANA_W]) {
	lookup_yomi(dir);
    } else if (searchwidgets[SEARCH_PINYIN_W]
	       && w == searchwidgets[SEARCH_PINYIN_W]) {
	handle_lookup_pinyin(w, dir);
    } else if (w == searchnumbers[POUND_INPUT]) {
	handle_lookup_number(w, dir, "JIS", 'x', True);
    } else if (w == searchnumbers[F_INPUT]) {
	handle_lookup_number(w, dir, "FREQ", 'd', False);
    } else if (w == keynumbers[Q_INPUT]) {
	handle_lookup_number(w, dir, "CORNER", 'd', False);
    } else if (w == keynumbers[S_INPUT]) {
	handle_lookup_skip(w, dir);
    } else if (w == keynumbers[B_INPUT]) {
	handle_lookup_bushu(w, dir);
    } else if (w == searchnumbers[H_INPUT]) {
	handle_lookup_number(w, dir, "HALPERN", 'd', True);
    } else if (w == searchnumbers[N_INPUT]) {
	handle_lookup_number(w, dir, "NELSON", 'd', True);
    } else if (w == searchnumbers[U_INPUT]) {
	handle_lookup_number(w, dir, "UNICODE", 'x', True);
    } else if (w == searchwidgets[SEARCH_DROP_W]) {
	handle_lookup_drop(w, dir);
    } else {
	setstatus(True, "Unexpected input");
	logmsg(L_WARN, 0, "Where did that input come from??");
	return;
    }
}

void
dict_lookup(Widget w, XEvent * event, String * params,
	    Cardinal * num_params)
{
    Matchdir dir;

    dir = getMatchDir("lookup", params, num_params);
    lookup(w, dir);
}

int
handle_lookup_english(Widget w, Matchdir dir)
{
    String str;

    XtVaGetValues(searchwidgets[SEARCH_ENG_W], XtNstring, &str, NULL);

    if (!*str || *str == '\n') {
	return 0;
    }

    return do_match_string("ENGLISH", str, dir);
}

int
lookup_yomi(Matchdir dir)
{
    /*FIXME: add romaji */

    return do_match_string("YOMI", (char *) kanastring, dir);
}

int
lookup_kanji(char *str, Matchdir dir)
{
    return do_match_string("KANJI", str, dir);
}

int
lookup_bushu(char *str, Matchdir dir)
{
    DictEntry entry;
    String p, q;
    static char bushu[128];
    static char old_bushu[128];
    static Boolean have_off;
    static int off;

    p = str;
    q = bushu;
    while (*p && isdigit(*p))
	*q++ = *p++;
    if (*p != '.' && *p != ':') {
	setstatus(True, "invalid bushu pattern: '.' or ':' expected");
	return -1;
    }
    *q++ = *p++;
    while (*p && isdigit(*p))
	*q++ = *p++;
    *q = 0;

    if (set_search_key("BUSHU"))
	return -1;
    if (open_data_connection()) {
	setstatus(False, "Data conn. failed");
	return -1;
    }

    if (strcmp(bushu, old_bushu) == 0) {
	if (have_off)
	    sendf("ENTRY \"%s,%d\" %s", bushu, off, dir_str[dir]);
	else
	    sendf("ENTRY \"%s\" %s", bushu, dir_str[dir]);
    } else {
	strcpy(old_bushu, bushu);
	sendf("ENTRY \"%s\"", bushu);
    }

    if (get_reply() == RC_OK && get_entry(&entry, 1) == 1) {
	p = strchr(reply_buf, '(');
	if (p && *p++ && (isdigit(*p) || *p == '-')) {
	    have_off = True;
	    off = strtol(p, NULL, 10);
	} else
	    have_off = False;
	ring_insert(&entry);
	display_entry(&entry);
	setstatus(False, "Ready");
    } else
	setstatus(False, "NOT found: %s", reply_buf);
    close_data_connection();
    return 0;
}

int
handle_lookup_pinyin(Widget w, Matchdir dir)
{
    String str, p;

    XtVaGetValues(searchwidgets[SEARCH_PINYIN_W], XtNstring, &str, NULL);
    for (p = str; *p && isalpha(*p) && *p != 'v' && *p != 'V'; p++);
    if (*p) {
	if (isspace(*p))
	    *p = 0;
	switch (*p) {
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	    p++;
	    break;
	case '0':
	    *p++ = '5';
	    break;
	case '-':
	    *p++ = '1';
	    break;
	case '\'':
	case '/':
	    *p++ = '2';
	    break;
	case '~':
	case '^':
	case '<':
	case '>':
	case 'v':
	case 'V':
	    *p++ = '3';
	    break;
	case '`':
	case '\\':
	    *p++ = '4';
	    break;
	case '.':
	    *p++ = '5';
	    break;
	default:
	    setstatus(True, "`%s' is not a tone mark: ignored", p);
	}
	*p = 0;
    }
    return do_match_string("PINYIN", str, dir);
}

int
handle_lookup_number(Widget w, Matchdir dir,
		     char *skey,	/* search key */
		     int fmt_let,	/* number format letter */
		     Boolean closest)   /* do we want the closest match */
{				
    ushort number;

    number = GetWidgetNumber(w, fmt_let);
    return do_match_number(skey, number, dir, closest);
}


int
handle_lookup_skip(Widget w, Matchdir dir)
{
    int code;
    String str;

    code = GetWidgetSkipCode(w, &str);
    if (code == 0)
	return -1;
    return do_match_string("SKIP", str, dir);
}

int
handle_lookup_bushu(Widget w, Matchdir dir)
{
    String str;

    XtVaGetValues(w, XtNstring, &str, NULL);
    return lookup_bushu(str, dir);
}

void
histCallback(Widget w, XtPointer data, XtPointer calldata)
{
    DictEntry entry;
    if (ring_get((int) calldata, &entry))
	return;

    if (open_data_connection()) {
	setstatus(False, "Data connection failed");
	return;
    }
    display_entry(&entry);
    close_data_connection();
}

void
findPrevCallback(Widget w, XtPointer data, XtPointer calldata)
{
    if (!search_widget) {
	return;
    }

    lookup(search_widget, MatchPrev);
}

void
findNextCallback(Widget w, XtPointer data, XtPointer calldata)
{
    if (!search_widget) {
	return;
    }

    lookup(search_widget, MatchNext);
}

/* setstatus:
 */
void
setstatus(Boolean beep, char *fmt, ...)
{
    static char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    XtVaSetValues(statusline, XtNlabel, buf, NULL);
    if (beep)
	Beep();
}
