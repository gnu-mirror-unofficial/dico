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
#include <dict.h>
#include <bushu.h>
#include <client.h>
#include <search.h>


#define BushuEncoding XawTextEncoding8bit

#define BUSHU_PER_ROW 10

enum {
    NUM_STROKES_INPUT,
    RADICAL_GLYPH,
    REST_STROKES_INPUT,
    ACCEPT_BUTTON,
    ACCEPT_BUTTON_PREV,
    NUM_OF_BUSHU_WIDGETS,
};

Widget bushuwidget[NUM_OF_BUSHU_WIDGETS];
Widget *bushubtn;


static struct popup_info bushu_popup_info = {
    "bushu input", NULL, -1, 0, 0, NULL,
};


/* This gets called after clicking on the "Bushu search" button.
 * It pops up the bushu input window
 */
void
Showpopupbushu(Widget w, XtPointer client_data, XtPointer call_data)
{
    Showpopup(w, &bushu_popup_info);
}

/*************************************************************
 *                       Bushu input                         *
 *************************************************************/

void
load_incr_input(Incr_input *input, char *str)
{
    strcpy(input->buf, str);
    input->cur = input->buf + strlen(input->buf);
    input->num = atoi(str);
}

void
backspace_incr_input(Incr_input *input)
{
    if (input->cur > input->buf) 
	*--input->cur = 0;
}
	
int
Handle_numstrokes(Widget w, XtPointer closure, XEvent *e, Boolean *cont)
{
    XKeyEvent *key_event;
    KeySym keysym;
    char *charpressed, *p;
    Incr_input *input;
    int num;

    input = (Incr_input*)closure;
    if (e->type != KeyPress) {
	if (e->type == KeyRelease) {
	    logmsg(L_WARN, 0, "Handle_numstrokes(): key released");
	    return -1;
	}
	logmsg(L_WARN, 0, "Some other strange event found in Handle_numstrokes(): %d\n",
		e->type);
	return -1;
    }
    key_event = (XKeyEvent *) e;

    keysym = XKeycodeToKeysym(XtDisplay(w), key_event->keycode, 0);
    if (keysym == (KeySym) NULL) {
	logmsg(L_WARN, 0, "Handle_numstrokes(): NULL keysym???");
	return -1;
    }
    switch (keysym) {
    case XK_BackSpace:
    case XK_Delete:
	if (input->cur > input->buf)
	    input->cur--;
	break;
    default:
	if (input->cur >= input->buf + MAX_STROKEINPUT) {
	    Beep();
	    return -1;
	}
	charpressed = XKeysymToString(keysym);
	if (charpressed == NULL)
	    return -1;

	DEBUG(("Handle_numstrokes(): got string \"%s\"", charpressed));

	if (!isdigit(*charpressed)) {
	    Beep();
	    return -1;
	}
	*input->cur++ = *charpressed;
    }
    
    num = 0;
    for (p = input->buf; p < input->cur; p++)
	num = 10*num + *p - '0';
    *input->cur = 0;
    input->num = num;
    XtVaSetValues(w, XtNlabel, input->buf, NULL);
    return 0;
}

static char *bushuAccel =
" <Key>Return:  bushu-handler(forward)\n \
  Ctrl<Key>F:   bushu-handler(forward)\n \
  Ctrl<Key>B:   bushu-handler(backward)\n \
";

static struct bushu_page {
    int numstrokes;
    int index;
    int count;
} bushu_page;
static Bushu cur_bushu;
static char empty[] = { 255, 0 };
static Incr_input bushu_input;

void acceptkana (Widget, XtPointer, XtPointer);
void changebushu (Widget, XtPointer, XtPointer);
void acceptbushu (Widget, XtPointer, XtPointer);
void Handle_bushu_numstrokes (Widget, XtPointer, XEvent *, Boolean *);

void
change_bushu_page(int numstrokes)
{
    int i;
    
    if (numstrokes < 0 || numstrokes > max_bushu_strokes) {
	setstatus(True, "Invalid number of strokes for bushu: max %d",
		  max_bushu_strokes);
	return;
    }
    bushu_page.numstrokes = numstrokes;
    bushu_page.index = bushu_index[numstrokes];
    bushu_page.count = bushu_count[numstrokes];
    for (i = 0; i < bushu_page.count; i++) {
	XtVaSetValues(bushubtn[i],
		      XtNlabel, bushu_string[bushu_page.index+i],
		      NULL);
    }
    for (; i < max_bushu_count; i++) {
	XtVaSetValues(bushubtn[i],
		      XtNlabel, empty,
		      NULL);
    }
}

Matchdir
getMatchDir(char *func, String *params, Cardinal *num_params)
{
    Matchdir dir = MatchNext;

    if (*num_params == 0)
	dir = MatchNext;
    else if (*num_params == 1) {
	if (strcmp(params[0], "forward") == 0 || strcmp(params[0], "1") == 0)
	    dir = MatchNext;
	else if (strcmp(params[0], "backward")==0 || strcmp(params[0], "0")==0)
	    dir = MatchPrev;
	else
	    parm_error(func, 1, params[0]);
    } else 
	parm_cnt_error(func, 1, *num_params);
    return dir;
}

void
BushuHandler(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    char bushu_buf[64];
    Matchdir dir = MatchNext;
    
    /* w == bushuwidget[REST_STROKES_INPUT] */
    cur_bushu.numstrokes = GetWidgetNumber(w, 'd') + bushu_page.numstrokes;
    print_bushu(bushu_buf, cur_bushu.bushu, cur_bushu.numstrokes);
    lookup_bushu(bushu_buf, dir);
}

void
makebushuinput(Widget parent)
{
    int i;
    XtAccelerators Accel;
    Widget prev_widget;

    bushuwidget[NUM_STROKES_INPUT] =
	XtVaCreateWidget("numstrokesInput",
			 labelWidgetClass,
			 parent,
			 XtNlabel, "   ",
			 XtNjustify, XtJustifyLeft,
			 NULL);

    bushuwidget[RADICAL_GLYPH] =
	XtVaCreateWidget("radicalGlyph",
			 labelWidgetClass,
			 parent,
			 XtNfromHoriz,
			 bushuwidget[NUM_STROKES_INPUT],
			 XtNlabel, "",
			 XtNinternational, False,
			 NULL);
    
    bushuwidget[REST_STROKES_INPUT] =
	XtVaCreateWidget("reststrokesInput",
			 asciiTextWidgetClass,
			 parent,
			 XtNfromHoriz,
			 bushuwidget[RADICAL_GLYPH],
			 XtNstring, "   ",
			 XtNeditType, XawtextEdit,
			 NULL);
    
    bushuwidget[ACCEPT_BUTTON] =
	XtVaCreateWidget("acceptNext",
			 commandWidgetClass,
			 parent,
			 XtNfromHoriz, bushuwidget[REST_STROKES_INPUT],
			 XtNencoding, XawTextEncodingChar2b,
			 NULL);
    bushuwidget[ACCEPT_BUTTON_PREV] =
	XtVaCreateWidget("acceptPrev",
			 commandWidgetClass,
			 parent,
			 XtNfromHoriz, bushuwidget[ACCEPT_BUTTON],
			 NULL);

	
    bushubtn = (Widget*) XtCalloc(max_bushu_count, sizeof(bushubtn[0]));
    prev_widget = bushuwidget[ACCEPT_BUTTON_PREV];
    for (i = 0; i < max_bushu_count; i++) {
	char namestr[10];
	sprintf(namestr, "%d", i);
	if (i % BUSHU_PER_ROW) {
	    bushubtn[i] =
		XtVaCreateWidget(namestr,
				 commandWidgetClass,
				 parent,
				 XtNfromVert, prev_widget,
				 XtNfromHoriz, bushubtn[i-1],
				 XtNlabel, empty,
				 XtNinternational, False,
				 NULL);
	} else {
	    bushubtn[i] =
		XtVaCreateWidget(namestr,
				 commandWidgetClass,
				 parent,
				 XtNfromVert, prev_widget,
				 XtNlabel, empty,
				 XtNinternational, False,
				 NULL);
	}

	if ((i+1) % BUSHU_PER_ROW == 0)
	    prev_widget = bushubtn[i];

	XtAddCallback(bushubtn[i],
		      XtNcallback, changebushu,
		      (XtPointer) i);
    }
    XtManageChildren(bushuwidget, NUM_OF_BUSHU_WIDGETS);
    XtManageChildren(bushubtn, max_bushu_count);
    Accel = XtParseAcceleratorTable(bushuAccel);
    XtOverrideTranslations(bushuwidget[REST_STROKES_INPUT], Accel);
    XtAddCallback(bushuwidget[ACCEPT_BUTTON], 
		      XtNcallback, acceptbushu,
		      (XtPointer)MatchNext);
    XtAddCallback(bushuwidget[ACCEPT_BUTTON_PREV], 
		      XtNcallback, acceptbushu,
		      (XtPointer)MatchPrev);
    INIT_INCR_INPUT(&bushu_input);
    XtAddEventHandler(bushuwidget[NUM_STROKES_INPUT],
		      KeyPressMask, False,
		      Handle_bushu_numstrokes, (XtPointer)&bushu_input);
}

void
Handle_bushu_numstrokes(Widget w, XtPointer closure, XEvent *e, Boolean *cont)
{
    Incr_input *input;

    input = (Incr_input*)closure;
    if (Handle_numstrokes(w, closure, e, cont))
	return;
    if (input->num > max_bushu_strokes) {
	setstatus(True, "Too many strokes");
	backspace_incr_input(input);
	XtVaSetValues(w, XtNlabel, input->buf, NULL);
	return;
    }
    change_bushu_page(input->num);
}


void
acceptbushu(Widget w, XtPointer data, XtPointer call_data)
{
    char bushu_buf[64];
    
    UnhighlightSearchWidget(w, data, call_data);
    cur_bushu.numstrokes =
	GetWidgetNumber(bushuwidget[REST_STROKES_INPUT], 'd')
	    + bushu_page.numstrokes;
    print_bushu(bushu_buf, cur_bushu.bushu, cur_bushu.numstrokes);
    lookup_bushu(bushu_buf, (Matchdir)data);
}

void
changebushu(Widget w, XtPointer data, XtPointer call_data)
{
    int num;

    if ((int)data > bushu_page.count) {
	Beep();
	return;
    }
    num = bushu_page.index + (int)data;
    XtVaSetValues(bushuwidget[RADICAL_GLYPH],
		  XtNlabel, bushu_string[num],
		  NULL);
    cur_bushu.bushu = GetBushu(num);
}


void
MakeBushuInputPopup()
{
    Widget w;

    bushu_popup_info.widget = XtVaCreatePopupShell("bushuMethodShell",
					    transientShellWidgetClass,
					    toplevel,
					    NULL);

    w = XtVaCreateManagedWidget("bushuInputForm",
				formWidgetClass,
				bushu_popup_info.widget,
				NULL);
    makebushuinput(w);
}

/* Local variables: */
/* buffer-file-coding-system: japanese-iso-8bit-unix */
/* End: */






