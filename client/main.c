/* $Id$
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <Xaw.h>
#include <X11/Composite.h>

#include <dico.h>
#include <client.h>

Display *display;
int screen;
Window mainwindow, rootwindow;
XtAppContext Context;
unsigned long white, black, default_background, default_foreground;

int broken_pipe = 0;

void clean_up();
void MainLoop();

int
main(int argc, char **argv)
{
    set_program_name(argv[0]);
    
    initsys(argc, argv);
/*    XtAppMainLoop(Context);*/
    logmsg(L_INFO, 0, "starting up...");
    MainLoop(Context);
    return 0;
}

void
MainLoop()
{
    XEvent ev;
    
    while (1) {
	if (broken_pipe) {
	    setstatus(True, "broken pipe");
	    close_connection();
	    reconnect(True);
	    broken_pipe = 0;
	}
	XtAppNextEvent(Context, &ev);
	XtDispatchEvent(&ev);
    }
}

void
quit(Widget w, XtPointer data, XtPointer calldata)
{
    DEBUG(("quitting"));

    XtCloseDisplay(display);

    clean_up();
    exit(0);
}


void
usage()
{
    static char ustr[] =
"usage: gjdict \n";
    printf("%s\n", ustr);
    exit(0);
}

void
clean_up()
{
    close_connection();
}


int
GetSubResourceInt(Widget w, char *res_name, char *class_name)
{
    int retval;
    static XtResource resourceList[] = {
	{ NULL,              /* Resource name  */
	  NULL,              /* Resource class */
	  XtRInt,      /* Representation type desired */
	  sizeof(int), /* Size in bytes of representation  */
	  0,
	  XtRInt,
	  (XtPointer) 0 }
    };
    resourceList[0].resource_name = res_name;
    resourceList[0].resource_class= class_name;
    XtGetSubresources(w, &retval, NULL, NULL,
		      resourceList, XtNumber(resourceList),
		      NULL, 0);
    return retval;
}
