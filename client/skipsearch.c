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

#include <gjdict.h>
#include <client.h>
#include <search.h>

static struct popup_info skip_popup_info = {
    "skip input", NULL, -1, 0, 0, NULL,
};

/* This gets called after clicking on the "SKIP search" button.
 * It pops up the SKIP input window
 */
void
Showpopupskip(Widget w, XtPointer client_data, XtPointer call_data)
{
    Showpopup(w, &skip_popup_info);
}

/* ****************************************************************************
 * SKIP input method
 */
enum {
    SKIP_1, SKIP_2, SKIP_3, SKIP_4,
    SKIP_4_1, SKIP_4_2, SKIP_4_3, SKIP_4_4,
    SKIP_FIND_NEXT, SKIP_FIND_PREV
};

enum {
    SKIP_FIRST_CODE, SKIP_NUM_STROKES_1, SKIP_NUM_STROKES_2
};

static void setskipcode (int, int);
void DoSkipFind (Matchdir);
void acceptskip (Widget, XtPointer, XtPointer);
void Handle_skip_numstrokes (Widget, XtPointer, XEvent *, Boolean *);
static void skip1CallbackProc (Widget, XtPointer, XtPointer);
static void skip2CallbackProc (Widget, XtPointer, XtPointer);

enum {
    TGL, CMD, INP, LBL
};

int skip_code[3];
Incr_input skip_input[2];
Widget skipwidget[3];
struct {
    Widget w;
    int type;
    int prev;
    char *label;
    XtCallbackProc callback;
    int closure;
    int fromhoriz;
    int hdist;
    int fromvert;
    int vdist;
} skipw[] = {
    { NULL, TGL, -1, "八",
	  skip1CallbackProc, 1, -1, 0, -1, 0 },
    { NULL, TGL, SKIP_1, "二", 
	  skip1CallbackProc, 2, SKIP_1, 0, -1, 0 },
    { NULL, TGL, SKIP_2, "回",
	  skip1CallbackProc, 3, SKIP_2, 0, -1, 0 },
    { NULL, TGL, SKIP_3, "？",
	  skip1CallbackProc, 4, SKIP_3, 0, -1, 0 },
    { NULL, TGL, -1, "下",
	  skip2CallbackProc, 1, SKIP_3, 0, SKIP_4, 0 },
    { NULL, TGL, SKIP_4_1, "上",
	  skip2CallbackProc, 2, SKIP_3, 0, SKIP_4_1, 0 },
    { NULL, TGL, SKIP_4_2, "中",
	  skip2CallbackProc, 3, SKIP_3, 0, SKIP_4_2, 0 },
    { NULL, TGL, SKIP_4_3, "？",
	  skip2CallbackProc, 4, SKIP_3, 0, SKIP_4_3, 0 },

    { NULL, CMD, -1, NULL, /* 次 */
	  acceptskip, MatchNext, -1, 0, SKIP_4_3, 0 }, 
    { NULL, CMD, -1, NULL, /* 前 */
	  acceptskip, MatchPrev, SKIP_FIND_NEXT, 0, SKIP_4_3, 0 },
};

void
setskipcode(int n, int val)
{
    char buf[8];
    
    skip_code[n] = val;
    sprintf(buf, "%d", val);
    XtVaSetValues(skipwidget[n], XtNlabel, buf, NULL);
}

void
acceptskip(Widget w, XtPointer data, XtPointer call_data)
{
    UnhighlightSearchWidget(w, data, call_data);
    DoSkipFind((Matchdir)data);
}

#define Handle_skip_numstrokes (XtEventHandler) Handle_numstrokes

static void
SetSkip3(char *str)
{
    load_incr_input(&skip_input[1], str);
    XtVaSetValues(skipwidget[2], XtNlabel, str, NULL);
}

static void
skip1CallbackProc(Widget w, XtPointer data, XtPointer call_data)
{
    int i, n = (int)data;
    Boolean sensitive;
    
    setskipcode(0, n);
    sensitive = n == 4;
    if (!sensitive) {
	if (skip_input[1].num > 0 && skip_input[1].num <= 4)
	    XtVaSetValues(skipw[SKIP_4_1 + skip_input[1].num - 1].w,
			  XtNstate, False,
			  NULL);
	SetSkip3("");
    }
    for (i = 0; i < 4; i++) {
	XtVaSetValues(skipw[i + SKIP_4_1].w,
		      XtNsensitive, sensitive,
		      NULL);
    }
    XtVaSetValues(skipwidget[SKIP_NUM_STROKES_2],
		  XtNsensitive, !sensitive,
		  NULL);
}

static void
skip2CallbackProc(Widget w, XtPointer data, XtPointer call_data)
{
    int n = (int)data;
    char buf[10];

    sprintf(buf, "%d", n);
    SetSkip3(buf);
}

void
DoSkipFind(Matchdir dir)
{
    char code[128];
    
    /* Construct the skip code */
    sprintf(code, "%d-%d-%d",
	    skip_code[0], skip_input[0].num, skip_input[1].num);
    lookup_skip(code, dir);
}

/* make the widgets for the SKIP kanji search method */
void
makeskipinput(Widget parent)
{
    int i;
    XtCallbackRec callbackRec[] = {
	{ NULL, NULL },
	{ NULL, NULL },
    };
    char namebuf[24];
    Arg args[12];
    int num_args;
    WidgetClass wclass;
    Widget first = NULL;
    
    for (i = 0; i < XtNumber(skipw); i++) {
	sprintf(namebuf, "skip%d", i);
	if (first && skipw[i].prev >= 0) {
	    XtVaSetValues(skipw[i-1].w,
			  XtNradioGroup, first,
			  NULL);
	    first = NULL;
	}
	
	num_args =  0;
	if (skipw[i].label) {
	    XtSetArg(args[num_args], XtNlabel, skipw[i].label);
	    num_args++;
	}
	callbackRec[0].callback = skipw[i].callback;
	callbackRec[0].closure = (XtPointer) skipw[i].closure;
	XtSetArg(args[num_args], XtNcallback, callbackRec); num_args++;
	if (skipw[i].fromhoriz >= 0) {
	    XtSetArg(args[num_args], XtNfromHoriz, skipw[skipw[i].fromhoriz].w);
	    num_args++;
	}
	if (skipw[i].fromvert >= 0) {
	    XtSetArg(args[num_args], XtNfromVert, skipw[skipw[i].fromvert].w);
	    num_args++;
	}
	if (skipw[i].hdist > 0) {
	    XtSetArg(args[num_args], XtNhorizDistance, skipw[i].hdist);
	    num_args++;
	}
	if (skipw[i].vdist > 0) {
	    XtSetArg(args[num_args], XtNvertDistance, skipw[i].vdist);
	    num_args++;
	}

	switch (skipw[i].type) {
	case TGL:
	    wclass = toggleWidgetClass;
	    if (i && skipw[i].prev >= 0) {
		XtSetArg(args[num_args], XtNradioGroup, skipw[skipw[i].prev].w);
		num_args++;
	    }
	    break;
	case CMD:
	    wclass = commandWidgetClass;
	    break;
	case INP:
	    wclass = 0;
	    break;
	}
	skipw[i].w =
	    XtCreateManagedWidget(namebuf, wclass, parent, args, num_args);
	if (skipw[i].type == TGL && skipw[i].prev == -1)
	    first = skipw[i].w;
    }

    skipwidget[SKIP_FIRST_CODE] =
	XtVaCreateManagedWidget("code",
				labelWidgetClass,
				parent,
				XtNfromVert, skipw[0].w,
				XtNvertDistance, 12,
				XtNlabel, "   ",
				XtNjustify, XtJustifyLeft,
				NULL);

    skipwidget[SKIP_NUM_STROKES_1] =
	XtVaCreateManagedWidget("numStrokes1",
				labelWidgetClass,
				parent,
				XtNfromVert, skipw[0].w,
				XtNvertDistance, 12,
				XtNfromHoriz, skipwidget[SKIP_FIRST_CODE],
				XtNlabel, "   ",
				XtNjustify, XtJustifyLeft,
				XtNcallback, callbackRec,
				NULL);
    XtAddEventHandler(skipwidget[SKIP_NUM_STROKES_1],
		      KeyPressMask, False,
		      Handle_skip_numstrokes, (XtPointer)&skip_input[0]);

    skipwidget[SKIP_NUM_STROKES_2] =
	XtVaCreateManagedWidget("numStrokes2",
				labelWidgetClass,
				parent,
				XtNfromVert, skipw[0].w,
				XtNvertDistance, 12,
				XtNfromHoriz, skipwidget[SKIP_NUM_STROKES_1],
				XtNlabel, "   ",
				XtNjustify, XtJustifyLeft,
				XtNcallback, callbackRec,
				NULL);
    XtAddEventHandler(skipwidget[SKIP_NUM_STROKES_2],
		      KeyPressMask, False,
		      Handle_skip_numstrokes, (XtPointer)&skip_input[1]);
    /* Init */
    for (i = 0; i < 4; i++) {
	XtVaSetValues(skipw[i + SKIP_4_1].w,
		      XtNsensitive, False,
		      NULL);
    }

    INIT_INCR_INPUT(&skip_input[0]);
    INIT_INCR_INPUT(&skip_input[1]);
}

void
MakeSkipInputPopup()
{
    Widget form;

    skip_popup_info.widget =
	XtVaCreatePopupShell("skipMethodShell",
			     transientShellWidgetClass,
			     toplevel,
			     NULL);

    form =
	XtVaCreateManagedWidget("skipInputForm",
				formWidgetClass,
				skip_popup_info.widget,
				NULL);

    makeskipinput(form);
}

/* Local variables: */
/* buffer-file-coding-system: japanese-iso-8bit-unix */
/* End: */
  


