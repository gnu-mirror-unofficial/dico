/* $Id$
 */
/*  Gjdict
 *  Copyright (C) 1999-2000, 2008 Sergey Poznyakoff
 *
 *  Dico is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Dico is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Dico.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Shell.h>
#include <X11/Xlib.h>
#include <Xaw.h>
#include <client.h>

static char help_str[] = 
PACKAGE_STRING " - a Japanese Dictionary\n\
Copyright (C) 1999-2000, 2008 Sergey Poznyakoff\n\
\n\
Dico is free software; you can redistribute it and/or modify\n\
it under the terms of the GNU General Public License as published by\n\
the Free Software Foundation; either version 2 of the License, or\n\
(at your option) any later version.\n\
\n\
Dico is distributed in the hope that it will be useful,\n\
but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
GNU General Public License for more details.\n\
\n\
You should have received a copy of the GNU General Public License\n\
along with Dico.  If not, see <http://www.gnu.org/licenses/>.\n\
"; 


static void
popdownCallback(Widget w, XtPointer data, XtPointer calldata)
{
    popdown((struct popup_info*)data);
}

void
licenseDialog(Widget w, XtPointer data, XtPointer calldata)
{
    Widget shell, form, btn[2]; 
    struct popup_info popup_info;
    XtAppContext    context;
    XtCallbackRec closeCallbackRec[] = {
	{ popdownCallback, &popup_info },
	{ NULL }
    };

    shell = XtVaCreatePopupShell("licenseShell",
				 transientShellWidgetClass,
				 toplevel,
				 NULL);
    
    form = XtCreateWidget("form", formWidgetClass, shell, NULL, 0);

    btn[0] = XtVaCreateWidget("info",
			      labelWidgetClass,
			      form,
			      XtNlabel, help_str,
			      NULL);

    btn[1] = XtVaCreateWidget("close",
			      commandWidgetClass,
			      form,
			      XtNcallback, closeCallbackRec,
			      NULL);
    
    XtManageChildren(btn, 2);
    XtManageChild(form);
    XawFormDoLayout(form, 0);

    popup_info.name = "License";
    popup_info.widget = shell;
    popup_info.up = -1;
    popup_info.rel_x = 0;
    popup_info.rel_y = 0;
    popup_info.rel_from = NULL;

    ShowExclusivePopup(w, &popup_info);

    context = XtWidgetToApplicationContext(shell);
    while (popup_info.up == 1 || XtAppPending(context)) {
	XtAppProcessEvent (context, XtIMAll);
    }
    
    XtDestroyWidget(shell);  /* blow away the dialog box and shell */
}

