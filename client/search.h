#define NUM_OF_KW 100
#define MAX_STROKEINPUT 2
#define INIT_INCR_INPUT(ip) (ip)->cur = (ip)->buf

typedef struct {
    char buf[MAX_STROKEINPUT+1];
    char *cur;
    int num;
} Incr_input;

extern XChar2b std_translations[][2];
extern XChar2b accept_label[][2];
extern Widget romajiinput;

void load_incr_input(Incr_input *input, char *str);
void backspace_incr_input(Incr_input *input);
void Showpopupskip(Widget, XtPointer, XtPointer);
void Showpopupbushu(Widget, XtPointer, XtPointer);
void Showpopupkana(Widget, XtPointer, XtPointer);
void Showpopupkanji(Widget, XtPointer, XtPointer);
void Showxrefs(Widget, XtPointer, XtPointer);
int Handle_numstrokes (Widget, XtPointer, XEvent *, Boolean *);
void Handle_romajikana (Widget, XtPointer, XEvent *, Boolean *);
void BushuHandler (Widget, XEvent *, String *, Cardinal *);
void dict_lookup (Widget, XEvent *, String *, Cardinal *);
void process_backspace();






