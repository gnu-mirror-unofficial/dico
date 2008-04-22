/* $Id$
 */
typedef struct {
    int cnt;
    String *str;
    long *offs;
} TopicXref;

void HelpCallback(Widget w, XtPointer data, XtPointer call_data);
int LoadHelpFile (String **);
int LoadTopicXref (int, TopicXref *);
String GetTopicTitle (int);
String GetTopicText (int);
int FindTopic (String);
void help_me (Widget, XEvent *, String *, Cardinal *);


