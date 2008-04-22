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


static struct popup_info kanji_popup_info = {
    "4-corner method input", NULL, -1, 0, 0, NULL,
};

static Widget kanjiwidgets[14];
static Widget kanjiquarters[4];
static Widget codewidget;
static int quarter_up = -1;
static int corner_code[4];

char *quarterchars[] = {
	"Ðµ", "°ì", "¡Ã", "Ð¦", "½½", "¥­", "¸ý", "ÒÌ", "È¬", "¾®",
	"¡¡", /* "µõ¤í" */
};

static void update_code(int quarter, int code);

/* This gets called after clicking on the "Q" (4-corner search) button.
 * It pops up the 4-corner method input window
 */
void
Showpopupcorner(Widget w, XtPointer client_data, XtPointer call_data)
{
    Showpopup(w, &kanji_popup_info);
}

void
nextquarter()
{
    XtSetSensitive(kanjiquarters[quarter_up], True);
    if (++quarter_up == 4)
	quarter_up = 0;
    XtSetSensitive(kanjiquarters[quarter_up], False);
}

/* select a particular quarter for the "four corners" search method */
/* mark our internal counter, plus make that one "selected" */
static void
selectquarter(Widget w, XtPointer data, XtPointer call_data)
{
    int num;
    int kcount;

    num = (int) data;

    for (kcount = 0; kcount < 4; kcount++) {
	XtSetSensitive(kanjiquarters[kcount], !(num == kcount));
    }
    quarter_up = num;
}

/* change the active quarter for the "four corners" method, to
 * the symbol just clicked on
 */
void
changequarter(Widget w, XtPointer data, XtPointer call_data)
{
    int code = (int) data;
    
    if (quarter_up == -1) {
	logmsg(L_ERR, 0, "internal error: changequarter called with no quarter valid");
	return;
    }
    XtVaSetValues(kanjiquarters[quarter_up],
		  XtNlabel, quarterchars[code],
		  NULL);

    update_code(quarter_up, code);
    
    nextquarter();
}

void
update_code(int quarter, int code)
{
    char buf[7];
    
    if (code >= 10)
	code = 0;
    corner_code[quarter_up] = code;
    sprintf(buf, "%d%d%d%d",
	    corner_code[0],
	    corner_code[1],
	    corner_code[2],
	    corner_code[3]);
    XtVaSetValues(codewidget,
		  XtNlabel, buf,
		  NULL);
}

/* we have to go indirectly to "docornerfind" because
 * we first have to gather the "four corner" information from the
 * widgets, and put it into a single number
 */
void
Docornerfind(Widget w, XtPointer data, XtPointer call_data)
{
    int searchnumber = 0;

    UnhighlightSearchWidget(w, data, call_data);
    searchnumber = corner_code[0]*1000 +
	           corner_code[1]*100  +
		   corner_code[2]*10   +
		   corner_code[3];

    DEBUG(("searching for Q value %d", searchnumber));

    lookup_corner(searchnumber, (Matchdir)data);
}


/* make the widgets for the "four quarter" kanji search method */
void
makekanjiInput(Widget parent)
{
    int kcount;
    Widget displaything;
    Widget searchNext, searchPrev;

    for (kcount = 0; kcount < 11; kcount++) {
	char namestr[10];
	sprintf(namestr, "%d", kcount);
	kanjiwidgets[kcount] =
	    XtVaCreateWidget(namestr,
			     commandWidgetClass,
			     parent,
			     XtNlabel, quarterchars[kcount],
			     XtNencoding, XawTextEncodingChar2b,
			     XtNfont, smallkfont,
			     NULL);
	if (kcount > 0) {
	    XtVaSetValues(kanjiwidgets[kcount],
			  XtNfromHoriz, kanjiwidgets[kcount - 1],
			  NULL);
	}
	XtAddCallback(kanjiwidgets[kcount],
		      XtNcallback, changequarter,
		      (XtPointer) kcount);
    }


    
    displaything =
	XtVaCreateManagedWidget("displayfourkanji",
				formWidgetClass,
				parent,
				XtNfromVert, kanjiwidgets[0],
				XtNvertDistance, 20,
				XtNfromHoriz, kanjiwidgets[2],
				NULL);


    kanjiquarters[0] =
	XtVaCreateWidget("nw",
			 commandWidgetClass,
			 displaything,
			 XtNlabel, quarterchars[0],
			 XtNencoding, XawTextEncodingChar2b,
			 XtNfont, smallkfont, NULL);
    kanjiquarters[1] =
	XtVaCreateWidget("ne",
			 commandWidgetClass,
			 displaything,
			 XtNlabel, quarterchars[0],
			 XtNencoding, XawTextEncodingChar2b,
			 XtNfromHoriz, kanjiquarters[0],
			 XtNfont, smallkfont, NULL);
    kanjiquarters[2] =
	XtVaCreateWidget("sw",
			 commandWidgetClass,
			 displaything,
			 XtNlabel, quarterchars[0],
			 XtNencoding, XawTextEncodingChar2b,
			 XtNfromVert, kanjiquarters[0],
			 XtNfont, smallkfont, NULL);
    kanjiquarters[3] =
	XtVaCreateWidget("se",
			 commandWidgetClass,
			 displaything,
			 XtNlabel, quarterchars[0],
			 XtNencoding, XawTextEncodingChar2b,
			 XtNfromVert, kanjiquarters[0],
			 XtNfromHoriz, kanjiquarters[2],
			 XtNfont, smallkfont, NULL);

    codewidget =
	XtVaCreateManagedWidget("code",
				labelWidgetClass,
				parent,
				XtNfromHoriz, kanjiwidgets[6],
				XtNfromVert, kanjiwidgets[0],
				XtNvertDistance, 20,
				XtNlabel, "0000",
				NULL);

    searchNext =
	XtVaCreateManagedWidget("findNext",
				commandWidgetClass,
				parent,
				XtNencoding, XawTextEncodingChar2b,
				XtNfont, smallkfont,
				XtNfromVert, codewidget,
				/*XtNvertDistance, 30,*/
				XtNfromHoriz, kanjiwidgets[6],
				NULL);
    searchPrev =
	XtVaCreateManagedWidget("findPrev",
				commandWidgetClass,
				parent,
				XtNencoding, XawTextEncodingChar2b,
				XtNfont, smallkfont,
				XtNfromVert, codewidget,
				/*XtNvertDistance, 30,*/
				XtNfromHoriz, searchNext,
				NULL);

    XtManageChildren(kanjiwidgets, 11);
    XtManageChildren(kanjiquarters, 4);


    for (kcount = 0; kcount < 4; kcount++) {
	XtAddCallback(kanjiquarters[kcount],
		      XtNcallback, selectquarter,
		      (XtPointer) kcount);
    }
    XtAddCallback(searchNext, XtNcallback, Docornerfind, (XtPointer) MatchNext);
    XtAddCallback(searchPrev, XtNcallback, Docornerfind, (XtPointer) MatchPrev);
}


/* exported routine */
void
MakeCornerInputPopup()
{
    Widget kanjiinputform;

    kanji_popup_info.widget =
	XtVaCreatePopupShell("cornerMethodShell",
			     transientShellWidgetClass,
			     toplevel,
			     NULL);

    kanjiinputform =
	XtVaCreateManagedWidget("kanjiInputForm",
				formWidgetClass,
				kanji_popup_info.widget,
				NULL);

    /* make the 10 input things, plus 4 display things */
    makekanjiInput(kanjiinputform);
    selectquarter(NULL, 0, NULL);
}

/* Local variables: */
/* buffer-file-coding-system: japanese-iso-8bit-unix */
/* End: */

