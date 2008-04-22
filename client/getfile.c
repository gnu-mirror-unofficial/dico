/* $Id$
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdlib.h>
#include <stdarg.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <Xaw.h>
#include <stdio.h>
#include <client.h>

void YesDlgButtonCallbackProc(Widget, XtPointer, XtPointer);
void NoDlgButtonCallbackProc(Widget, XtPointer, XtPointer);

enum {
    LABEL, OK, CANCEL, INPUT, NUMWIDGETS
};

#define RET_NONE -1
#define RET_YES 0
#define RET_NO  1

static XtCallbackRec CallbackRec[] = {
    { YesDlgButtonCallbackProc, NULL },
    { NULL },
    { NoDlgButtonCallbackProc, NULL },
    { NULL }
};

void
setRelativePosition(Widget w)
{
    struct rel_pos {
	int x;
	int y;
	Widget from;
    } rel_pos;
    Position x, y;
    static XtResource resourceList[] = {
	{ "x",              /* Resource name  */
	  "X",              /* Resource class */
	  XtRPosition,      /* Representation type desired */
	  sizeof(Position), /* Size in bytes of representation  */
	  offsetof(struct rel_pos, x),
	  XtRPosition,
	  (XtPointer) 0 },
	    
	{ "y",              /* Resource name  */
	  "Y",              /* Resource class */
	  XtRPosition,      /* Representation type desired */
	  sizeof(Position), /* Size in bytes of representation  */
	  offsetof(struct rel_pos, y),
	  XtRPosition,
	  (XtPointer) 0 },

	{ "from",           /* Resource name  */
	  "From",           /* Resource class */
	  XtRWidget,        /* Representation type desired */
	  sizeof(Widget),   /* Size in bytes of representation  */
	  offsetof(struct rel_pos, from),
	  XtRWidget, (XtPointer) 0 },
    };
    
    XtGetSubresources(w, &rel_pos, "rel", "Rel",
		      resourceList, XtNumber(resourceList),
		      NULL, 0);
    if (rel_pos.from == NULL)
	rel_pos.from = toplevel;

    XtTranslateCoords(rel_pos.from,
		      rel_pos.x,
		      rel_pos.y,
		      &x, &y);
    XtVaSetValues(w,
		  XtNx, x,
		  XtNy, y,
		  NULL);
}

int
getFileName(Widget w, char *instr, char **outstr)
{
    Widget shell, form, btn[NUMWIDGETS];
    int answer = RET_NONE;	
    XtAppContext context;
    Arg args[1];
    char *name = NULL;
    
    CallbackRec[0].closure = CallbackRec[2].closure = &answer;
    shell = XtCreatePopupShell("getFileNameShell",
			       transientShellWidgetClass,
			       w, NULL, 0);
    setRelativePosition(shell);

    form = XtCreateWidget("form", formWidgetClass, shell, NULL, 0);
    btn[LABEL] = XtVaCreateWidget("file-name", labelWidgetClass, form,
				  XtNborderWidth, 0,
				  NULL);

    btn[INPUT] = XtVaCreateWidget("input", asciiTextWidgetClass, form,
				  XtNstring, instr,
				  XtNeditType, XawtextEdit,
				  XtNfromHoriz, btn[LABEL],
				  NULL);
    
    btn[OK] = XtVaCreateWidget("ok", commandWidgetClass, form,
			       XtNcallback, &CallbackRec[0],
			       XtNfromVert, btn[INPUT],
			       NULL);

    btn[CANCEL] = XtVaCreateWidget("cancel", commandWidgetClass, form,
				   XtNcallback, &CallbackRec[2],
				   XtNfromVert, btn[INPUT],
				   XtNfromHoriz, btn[OK],
				   NULL);
    XtManageChildren(btn, NUMWIDGETS);
    XtManageChild(form);
    XawFormDoLayout(form, 0);
    XtPopup(shell, XtGrabExclusive);
    XtSetKeyboardFocus(shell, btn[INPUT]);

    context = XtWidgetToApplicationContext(shell);
	
    for (answer = RET_NONE; answer == RET_NONE || XtAppPending(context);) {
	XtAppProcessEvent (context, XtIMAll);
    }
    if (answer == RET_YES) {
	XtSetArg(args[0], XtNstring, &name);
	XtGetValues(btn[INPUT], args, 1);
	if (name[0] == 0)
	    *outstr = NULL;
	else
	    *outstr = strdup(name);
    } 
    
    XtPopdown(shell);
    XtDestroyWidget(shell);
	
    return answer != RET_YES;
}


int
Xgetyn(char *fmt, ...)
{
    Widget shell, form, btn[NUMWIDGETS];
    int answer = RET_NONE;	
    XtAppContext context;
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);
    
    CallbackRec[0].closure = CallbackRec[2].closure = &answer;
    shell = XtCreatePopupShell("getynShell",
			       transientShellWidgetClass,
			       toplevel, NULL, 0);
    setRelativePosition(shell);

    form = XtCreateWidget("form", formWidgetClass, shell, NULL, 0);
    btn[LABEL] = XtVaCreateWidget("text",
				  labelWidgetClass,
				  form,
				  XtNborderWidth, 0,
				  XtNlabel, buf,
				  NULL);

    btn[OK] = XtVaCreateWidget("ok",
			       commandWidgetClass,
			       form,
			       XtNcallback, &CallbackRec[0],
			       XtNfromVert, btn[LABEL],
			       NULL);

    btn[CANCEL] = XtVaCreateWidget("cancel",
				   commandWidgetClass,
				   form,
				   XtNcallback, &CallbackRec[2],
				   XtNfromVert, btn[LABEL],
				   XtNfromHoriz, btn[OK],
				   NULL);
    XtManageChildren(btn, NUMWIDGETS-1);
    XtManageChild(form);
    XawFormDoLayout(form, 0);
    XtPopup(shell, XtGrabExclusive);
    
    context = XtWidgetToApplicationContext(shell);
	
    for (answer = RET_NONE; answer == RET_NONE || XtAppPending(context);) {
	XtAppProcessEvent (context, XtIMAll);
    }
    XtPopdown(shell);
    XtDestroyWidget(shell);
	
    return answer == RET_YES;
}


void 
YesDlgButtonCallbackProc(Widget widget, XtPointer closure, XtPointer call_data)
{
    int *answer = (int *) closure;
    *answer = RET_YES;	
}

void 
NoDlgButtonCallbackProc(Widget widget, XtPointer closure, XtPointer call_data)
{
    int *answer = (int *) closure;
    *answer = RET_NO;	
}
