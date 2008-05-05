#include <dict.h>

#define DEBUG(c)
/*#define DEBUG(c) do {printf c; printf("\n");} while (0)*/

#define NOKANJI 0x2266

enum gojuon {
    KANA_A   ,	
    KANA_I   ,
    KANA_U   ,
    KANA_E   ,
    KANA_O   ,

    KANA_KA  ,
    KANA_KI  ,
    KANA_KU  ,
    KANA_KE  ,
    KANA_KO  ,

    KANA_SA  ,
    KANA_SHI ,
    KANA_SU ,
    KANA_SE  ,
    KANA_SO  ,

    KANA_TA  ,
    KANA_CHI ,
    KANA_TSU ,
    KANA_TE  ,
    KANA_TO  ,

    KANA_NA  ,
    KANA_NI  ,
    KANA_NU  ,
    KANA_NE  ,
    KANA_NO  ,

    KANA_HA  ,
    KANA_HI  ,
    KANA_FU  ,
    KANA_HE  ,
    KANA_HO  ,

    KANA_MA ,
    KANA_MI  ,
    KANA_MU  ,
    KANA_ME  ,
    KANA_MO  ,

    KANA_RA  ,
    KANA_RI  ,
    KANA_RU  ,
    KANA_RE  ,
    KANA_RO  ,

    KANA_GA  ,
    KANA_GI  ,
    KANA_GU  ,
    KANA_GE  ,
    KANA_GO  ,

    KANA_ZA  ,
    KANA_ZI  ,
    KANA_ZU  ,
    KANA_ZE  ,
    KANA_ZO  ,

    KANA_DA  ,
    KANA_DI  ,
    KANA_DU  ,
    KANA_DE  ,
    KANA_DO  ,

    KANA_BA  ,
    KANA_BI  ,
    KANA_BU  ,
    KANA_BE  ,
    KANA_BO  ,

    KANA_PA  ,
    KANA_PI  ,
    KANA_PU  ,
    KANA_PE  ,
    KANA_PO  ,

    KANA_YA,
    KANA_BLANK1,
    KANA_YU,
    KANA_BLANK2,
    KANA_YO,

    KANA_SMALL_YA,
    KANA_BLANK3	,
    KANA_SMALL_YU,
    KANA_BLANK4	,
    KANA_SMALL_YO,

    KANA_WA  ,
    KANA_WI  ,
    KANA_WU  ,
    KANA_WE  ,
    KANA_WO  ,

    KANA_SMALL_A ,
    KANA_SMALL_I ,
    KANA_SMALL_U ,
    KANA_SMALL_E ,
    KANA_SMALL_O ,

    KANA_SMALL_WA,
    KANA_SMALL_TSU,
    KANA_N
};

struct popup_info {
    char *name;
    Widget widget;
    int up;
    Position rel_x;
    Position rel_y;
    Widget rel_from;
};

typedef struct {
    String server;
    String port;
    String SmallKanjiFont;
    Boolean transcript;
    int verbose;
    Boolean romaji;
    struct dict_entry {
	int buf_size;
	int buf_incr;
    } dict_entry;
    Boolean bell;
    int help_history_size;
    int history_size;
    String helpfilename;
    struct {
	Pixel foreground;
	Pixel background;
    } highlight;
} Config;


#include <dico.h>
#include "options.h"

extern IPADDR server_addr;
extern int port_no;
extern char *progname;
extern char reply_buf[];
extern Config config;
extern int broken_pipe;

extern XtAppContext Context;
extern Display *display;
extern int screen;
extern Window mainwindow, rootwindow;
extern unsigned long white, black, default_background, default_foreground;
extern Atom wm_message, delete_message;
extern GC gc, cleargc;
extern Widget toplevel, form;
extern DictEntry last_entry;
extern XFontStruct *smallkfont;
extern XFontStruct *englishfont;

/* main.c */
void quit(Widget w, XtPointer data, XtPointer calldata);
void clean_up(void);
int GetSubResourceInt(Widget w, char *res_name, char *class_name);
void usage(void);

/* init.c */
void initsys(int argc, char **argv);
void closeCallback(Widget w, XtPointer data, XtPointer calldata);
void MakeConnectWidget(void);
void connectCallback(Widget w, XtPointer data, XtPointer calldata);
void reconnect(Boolean cancel_quits);
void setup_server_options(void);
void set_server_name(String servername, String port);
void translate_options(void);
void handle_delete(Widget w, XtPointer closure, XEvent *event, Boolean *cont);
void setup_deletewindow(void);

/* cmdline.opt */
void get_options(int argc, char **argv);

/* client.c */
int init_connection(void);
void drop_connection(void);
void close_connection(void);
int open_data_connection(void);
void close_data_connection(void);
void lostpeer(void);
int get_lines(char *buf, size_t bufsize);
int get_reply(void);
void tryagain(char *str);
unsigned read_data(void *buf, size_t size);
int get_entry(DictEntry *entry, int num);
int get_recno(int *cnt_ptr, Recno *data);
char * get_string(Offset offset);
void drain_input(void);
void sendf(char *fmt, ...);

/* widgets.c */
void MakeWidgets(void);
void initfonts(void);
void Showpopup(Widget w, struct popup_info *popup_info);
void popdown(struct popup_info *popup_info);
void ShowExclusivePopup(Widget w, struct popup_info *popup_info);
Widget CreateNameList(Widget w, int cnt, String *name, Boolean multsel,
		      XtCallbackProc callback, int argc, Arg *item_args,
		      Widget **return_widgets);

/* search.c */
void MakeMenu(Widget top);
void MakeSearchWidget(Widget shell);
void process_backspace(void);
void process_kinput(XChar2b in_char);
void display_entry(DictEntry *entry);
int set_search_key(char *str);
int lookup_record(Recno recno);
int do_match_string(char *skey, char *string, Matchdir dir);
int do_match_number(char *skey, unsigned value, Matchdir dir, Boolean closest);
void lookup_corner(int code, Matchdir dir);
void lookup_skip(char *code, Matchdir dir);
void dict_lookup(Widget w, XEvent *event, String *params, Cardinal *num_params);
int handle_lookup_english(Widget w, Matchdir dir);
int lookup_yomi(Matchdir dir);
int lookup_kanji(char *str, Matchdir dir);
int lookup_bushu(char *str, Matchdir dir);
int handle_lookup_pinyin(Widget w, Matchdir dir);
int handle_lookup_number(Widget w, Matchdir dir, char *skey, int fmt_let, Boolean closest);
int handle_lookup_skip(Widget w, Matchdir dir);
int handle_lookup_bushu(Widget w, Matchdir dir);
void histCallback(Widget w, XtPointer data, XtPointer calldata);
void findPrevCallback(Widget w, XtPointer data, XtPointer calldata);
void findNextCallback(Widget w, XtPointer data, XtPointer calldata);
void NewSearchWidget(Widget w);
void UnhighlightSearchWidget(Widget w, XtPointer data, XtPointer calldata);

void setstatus(Boolean beep, char *fmt, ...);


/* bushusearch.c */
void MakeBushuInputPopup(void);
void Showpopupbushu(Widget w, XtPointer client_data, XtPointer call_data);
/*void load_incr_input(Incr_input *input, char *str);
void backspace_incr_input(Incr_input *input);*/
int Handle_numstrokes(Widget w, XtPointer closure, XEvent *e, Boolean *cont);
Matchdir getMatchDir(char *func, String *params, Cardinal *num_params);
void BushuHandler(Widget w, XEvent *event, String *params, Cardinal *num_params);
void Handle_bushu_numstrokes(Widget w, XtPointer closure, XEvent *e, Boolean *cont);

/* kanasearch.c */
void MakeKanaInputPopup(void);
void Showpopupkana(Widget w, XtPointer client_data, XtPointer call_data);
XChar2b kana_vowel(int);
XChar2b kana_syllable(int, int);
void switch_mode(Widget w, XEvent *event, String *params, Cardinal *num_parms);
void AddCloseCommand(struct popup_info *popup, Widget w);

/* cornersearch.c */
void Showpopupcorner(Widget w, XtPointer client_data, XtPointer call_data);
void MakeCornerInputPopup(void);

/* skipsearch.c */
void Showpopupskip(Widget w, XtPointer client_data, XtPointer call_data);
void MakeSkipInputPopup(void);

/* convert.c */
void Handle_romajikana(Widget w, XtPointer closure, XEvent *e, Boolean *cont);
char * kanatoromaji(XChar2b *kana);
void romajitokana(XChar2b *kstart);

/* jiscvt.c */
int shift_to_shin(char *text, int len);
int euc_to_shin(Uchar *text, int len);
int shin_to_euc(Uchar *text, int len);

/* xref.c */
void MakeXrefPopup(void);
void Showxrefs(Widget w, XtPointer client_data, XtPointer call_data);
void popdownxref(void);
void updatexref(void);
int find_by_recno(Recno recno, DictEntry *entry);
void setxref(void);

/* getfile.c */
int getFileName(Widget w, char *instr, char **outstr);
int Xgetyn(char *fmt, ...);

#define GetString get_string

/* util.c */
void Beep(void);
void parm_error(char *func_name, int parm_no, char *val);
void parm_cnt_error(char *func_name, int parm_cnt, int passed_cnt);
int GetWidgetNumber(Widget w, int fl);
int GetWidgetSkipCode(Widget w, String *);
void SetWidgetNumber(Widget w, char *fmt, int val);
void SetWidgetSkipCode(Widget w, int code);
void SetWidgetBushu(Widget w, int radical, int strokes);
XChar2b * dup_16(XChar2b *str);
char * escape_string(char *str);
char * format_string(char *str);
char * format_string16(char *str);
void init_dict_entry (void);

/* license.c */
void licenseDialog(Widget w, XtPointer data, XtPointer calldata);

/* bushu.c */
void print_bushu(char *buf, int radical, int strokes);
int GetBushu(int ind);

