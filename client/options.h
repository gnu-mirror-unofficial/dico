#ifndef __options_h
#define __options_h

struct keyword {
    char *name;
    int code;
};

typedef struct {
    String server;
    String port;
    String SmallKanjiFont;
    Boolean transcript;
    String verbose;
    Boolean romaji;
    struct {
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
} OptionsRec;


int decode_keyword(struct keyword *kw, char *string);

#endif
