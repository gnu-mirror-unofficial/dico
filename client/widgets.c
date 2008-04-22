/* $Id$
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <Xaw.h>

#include <client.h>
#include <search.h>
#include "help.h"

Widget toplevel, form;

XFontStruct *smallkfont;
XFontStruct *englishfont;
void MakeButtons();
void initfonts();

/* delete_calback
 *	sole purpose is to handle window deletion.
 *   still technically selected, via Actions.
 *	<Message>WM_PROTOCOLS
 *   NOT USED, (although it should work),
 *   since handle_delete() works for it.
 */

void
delete_callback(Widget w, XEvent *event, String *params, Cardinal *num_params)
{
    logmsg(L_WARN, 0, "delete_callback() called?!");
    if (event->xclient.data.l[0] == delete_message)
	quit(NULL, NULL, NULL);
}

void
quitaction(Widget w, XEvent *e, String *p, Cardinal *c)
{
    quit(NULL, NULL, NULL);
}

static XtActionsRec gjdictActionList[] = {
    {"quit", quitaction},
    {"delete-window", delete_callback},
    {"bushu-handler", BushuHandler},
    {"lookup", dict_lookup},
    {"help-me", help_me},
    {"switch-mode", switch_mode},
};

static char *gjdictAccel = " \
  <Message>WM_PROTOCOLS: delete-window()\n \
  <Message>WM_DELETE_WINDOW: delete-window()\n \
  ";

XtAccelerators AllAccel;



void
MakeWidgets()
{

    DEBUG(("Starting MakeWidgets"));

    initfonts();

    XtAppAddActions(Context,
		    gjdictActionList, XtNumber(gjdictActionList));
    AllAccel = XtParseAcceleratorTable(gjdictAccel);

    /*XtOverrideTranslations(form,AllAccel);*/
    XtAugmentTranslations(toplevel, AllAccel);

    MakeSearchWidget(toplevel);
    XawSimpleMenuAddGlobalActions(Context);

    DEBUG(("Ending MakeWidgets"));
}

void
initfonts()
{
    DEBUG(("SmallKanjiFont \"%s\"", config.SmallKanjiFont));
    DEBUG(("EnglishFont    \"%s\"", config.EnglishFont));

    smallkfont = XLoadQueryFont(display, config.SmallKanjiFont);
    if (smallkfont == NULL) {
	die(1, L_CRIT, 0, "could not load small kanji font `%s'",
	    config.SmallKanjiFont);
    }
}


static void
relposition(struct popup_info *popup_info)
{
    static XtResource resourceList[] = {
	{ "x",              /* Resource name  */
	  "X",              /* Resource class */
	  XtRPosition,      /* Representation type desired */
	  sizeof(Position), /* Size in bytes of representation  */
	  offsetof(struct popup_info, rel_x),
	  XtRPosition,
	  (XtPointer) 0 },
	    
	{ "y",              /* Resource name  */
	  "Y",              /* Resource class */
	  XtRPosition,      /* Representation type desired */
	  sizeof(Position), /* Size in bytes of representation  */
	  offsetof(struct popup_info, rel_y),
	  XtRPosition,
	  (XtPointer) 0 },
	
	{ "from",           /* Resource name  */
	  "From",           /* Resource class */
	  XtRWidget,        /* Representation type desired */
	  sizeof(Widget),   /* Size in bytes of representation  */
	  offsetof(struct popup_info, rel_from),
	  XtRWidget, (XtPointer) 0 },
    };

    XtGetSubresources(popup_info->widget, popup_info, "rel", "Rel",
		      resourceList, XtNumber(resourceList),
		      NULL, 0);
    if (popup_info->rel_from == NULL)
	popup_info->rel_from = toplevel;
}

void
Showpopup(Widget w, struct popup_info *popup_info)
{
    Position x, y;
    
    if (popup_info->up == -1) {
	/* first time init.. */
	relposition(popup_info);
	popup_info->up = 0;
    }
    if (popup_info->up == 0) {
	XtTranslateCoords(popup_info->rel_from,
			  popup_info->rel_x,
			  popup_info->rel_y,
			  &x, &y);
	XtVaSetValues(popup_info->widget,
		      XtNx, x,
		      XtNy, y,
		      NULL);
	XtPopup(popup_info->widget, XtGrabNone);
	popup_info->up = 1;
    } else {
#if 0
	/* Unsuccessfull attempt to save relative widget position
	 * Seems like it's completely impossible :(
	 */
	XtVaGetValues(popup_info->rel_from, XtNx, &x, XtNy, &y, NULL);
	XtVaGetValues(popup_info->widget,
		      XtNx, &popup_info->rel_x,
		      XtNy, &popup_info->rel_y,
		      NULL);
	popup_info->rel_x -= x;
	popup_info->rel_y -= y;
#endif
	XtPopdown(popup_info->widget);
	popup_info->up = 0;
    }
}


void
popdown(struct popup_info *popup_info)
{
    if (popup_info->up == 1) 
	Showpopup(NULL, popup_info);
}

void
ShowExclusivePopup(Widget w, struct popup_info *popup_info)
{
    Position x, y;
    
    if (popup_info->up == -1) {
	/* first time init.. */
	relposition(popup_info);
	popup_info->up = 0;
    }
    if (popup_info->up == 0) {
	XtTranslateCoords(popup_info->rel_from,
			  popup_info->rel_x,
			  popup_info->rel_y,
			  &x, &y);
	XtVaSetValues(popup_info->widget,
		      XtNx, x,
		      XtNy, y,
		      NULL);
	XtPopup(popup_info->widget, XtGrabExclusive);
	popup_info->up = 1;
    }
}


Widget 
CreateNameList(Widget w,
	       int cnt,
	       String *name,
	       Boolean multsel,
	       XtCallbackProc callback,
	       int argc,
	       Arg *item_args,
	       Widget **return_widgets)
{
    Widget child, prev=NULL;
    Arg *args;
    int num_args;
    char namebuf[10];
    int i;
    Widget box, *rw;
    XtCallbackRec callback_rec[] = {
	{ NULL, NULL },
	{ NULL, NULL },
    };

    args = calloc(7 + argc, sizeof(args[0]));
    if (!args) {
	logmsg(L_ERR, 0, "%s:%d:memory!", __FILE__, __LINE__);
	return NULL;
    }
    if (argc)
	memcpy(args, item_args, argc*sizeof(args[0]));

    
    callback_rec[0].callback = callback;
    box = XtVaCreateManagedWidget("box",
				  boxWidgetClass, w,
				  XtNright, XawChainRight,
				  XtNleft, XawChainRight,
				  NULL);
    if (cnt) {
	rw = calloc(cnt, sizeof(Widget));
	if (!rw)
	    return NULL;
	for (i = 0; i < cnt; i++) {
	    sprintf(namebuf, "%d", i);

	    num_args = argc;
	    XtSetArg(args[num_args], XtNlabel, name[i]); num_args++;
	    XtSetArg(args[num_args], XtNresize, False); num_args++;
	    XtSetArg(args[num_args], XtNborderWidth, 0); num_args++;
	    XtSetArg(args[num_args], XtNjustify, XtJustifyLeft); num_args++; 
	    callback_rec[0].closure = (XtPointer) i;
	    XtSetArg(args[num_args], XtNcallback, callback_rec); num_args++;
	    if (prev) {
		XtSetArg(args[num_args], XtNfromVert, prev);
		num_args++;
		if (!multsel) {
		    XtSetArg(args[num_args], XtNradioGroup, prev);
		    num_args++;
		}
	    }
	    child = XtCreateManagedWidget(namebuf,
					  toggleWidgetClass,
					  box,
					  args,
					  num_args);
	    rw[i] = child;
	    prev = child;
	}
	if (!multsel) {
	    XtVaSetValues(rw[0], 
                          XtNradioGroup, rw[i-1],
                          NULL);
	}
    } else
	rw = NULL;
    
    free(args);
    if (return_widgets)
	*return_widgets = rw;
    else if (rw)
	free(rw);
    return box;
}
