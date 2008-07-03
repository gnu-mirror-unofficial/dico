%{
/* This file is part of Dico.
   Copyright (C) 2008 Sergey Poznyakoff

   Dico is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   Dico is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Dico.  If not, see <http://www.gnu.org/licenses/>. */

#include <dicod.h>
#include <config-gram.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include <obstack.h>
#include <unistd.h>
#include <fcntl.h>
    
static char *multiline_delimiter;
static size_t multiline_delimiter_len;
static int multiline_unescape;         /* Unescape here-document contents */
static int (*char_to_strip)(char);     /* Strip matching characters of each
					  here-document line */

dicod_locus_t locus;   /* Input file location */
/* Line correction.  Equals to the number of #line directives inserted into
   the input by Dico preprocessor instance.  The external preprocessor, if
   any, counts these as input lines and therefore the line numbers in *its*
   #line directives are offset by the value of XLINES.
   
   Uff, running two preprocessors is confusing...
*/
size_t xlines; 
static struct obstack stk;
    
static void multiline_begin(char *);
static void multiline_add(char *);
static void multiline_finish(void);
static char *multiline_strip_tabs(char *text);
static int ident();
static int isemptystr(int off);

static void parse_line(char *text, dicod_locus_t *ploc, size_t *pxlines);
static void parse_line_cpp(char *text, dicod_locus_t *ploc, size_t *pxlines);


#undef YY_INPUT
#define YY_INPUT(buf,result,max_size) do {                  \
	if (preprocessor)				    \
	    result = fread(buf, 1, max_size, yyin);	    \
	else                                                \
	    result = pp_fill_buffer(buf, max_size);	    \
    } while (0)

%}
    
    
%x COMMENT ML STR

WS [ \t\f][ \t\f]*
ID [a-zA-Z_][a-zA-Z_0-9-]+
P [1-9][0-9]*

%%
         /* C-style comments */
"/*"         BEGIN(COMMENT);
<COMMENT>[^*\n]*        /* eat anything that's not a '*' */
<COMMENT>"*"+[^*/\n]*   /* eat up '*'s not followed by '/'s */
<COMMENT>\n             ++locus.line;
<COMMENT>"*"+"/"        BEGIN(INITIAL);
         /* Line directive */
^[ \t]*#[ \t]*{P}[ \t]+\".*\".*\n parse_line_cpp(yytext, &locus, &xlines);
^[ \t]*#[ \t]*line[ \t].*\n parse_line(yytext, &locus, &xlines);
         /* End-of-line comments */
#.*\n     { locus.line++; }
#.*     /* end-of-file comment */;
"//".*\n  { locus.line++; }
"//".*    /* end-of-file comment */;
        /* Identifiers */
<INITIAL>{ID}           return ident();
         /* Strings */
[a-zA-Z0-9_\./:-]+    { line_begin();
                        line_add(yytext, yyleng);
                        line_finish();
                        return STRING; }
         /* Quoted strings */
\"[^\\"\n]*\"         { line_begin();
                        line_add(yytext + 1, yyleng - 2);
                        line_finish();
                        return QSTRING; }
\"[^\\"\n]*\\. |
\"[^\\"\n]*\\\n        { BEGIN(STR);
                        line_begin();
		        line_add_unescape_last(yytext + 1, yyleng - 1); }
<STR>[^\\"\n]*\\. |
<STR>\"[^\\"\n]*\\\n  { line_add_unescape_last(yytext, yyleng); }
<STR>[^\\"\n]*\"      { BEGIN(INITIAL);
                        if (yyleng > 1) 
                          line_add(yytext, yyleng - 1); 
                        line_finish();
		        return QSTRING; }
         /* Multiline strings */
"<<"(-" "?)?\\?{ID}[ \t]*#.*\n |
"<<"(-" "?)?\\?{ID}[ \t]*"//".*\n |
"<<"(-" "?)?\\?{ID}[ \t]*\n |
"<<"(-" "?)?\"{ID}\"[ \t]*#.*\n |
"<<"(-" "?)?\"{ID}\"[ \t]*"//".*\n |
"<<"(-" "?)?\"{ID}\"[ \t]*\n {
                        BEGIN(ML); multiline_begin(yytext+2); locus.line++; }
<ML>.*\n { char *p = multiline_strip_tabs(yytext);
	   
           if (!strncmp(p, multiline_delimiter, multiline_delimiter_len)
	       && isemptystr(p + multiline_delimiter_len - yytext))
	     {
	       free(multiline_delimiter);
	       multiline_delimiter = NULL;
	       BEGIN(INITIAL);
	       multiline_finish();
	       return MSTRING;
	     }
           locus.line++;
	   multiline_add(p); } 
{WS}     ;
         /* Other tokens */
\n       { locus.line++; } 
[,;{}()] return yytext[0];
.        config_error(&locus, 0, _("stray character \\%03o"), yytext[0]);
%%

pid_t pp_pid;

int
yywrap()
{
    if (yyin)
	pp_extrn_shutdown(pp_pid);
    else
	pp_done();
    locus.file = NULL;
    return 1;
}

int
config_lex_begin(const char *name)
{
    obstack_init(&stk);
    if (preprocessor) {
	int fd;
	int argc;
	const char **argv;
	
	fd = open(name, O_RDONLY);
	if (fd == -1) {
	    dico_log(L_ERR, errno, _("Cannot open `%s'"), name);
	    return 1;
	}
	close(fd);

	pp_make_argcv(&argc, &argv);
	yyin = pp_extrn_start(argc, argv, &pp_pid);
	if (!yyin) {
	    dico_log(L_ERR, errno,
		     _("Unable to start external preprocessor `%s'"),
		     preprocessor);
	    return 1;
	}
    } else
	return pp_init(name);

    return 0;
}

void
config_lex_end()
{
}

int
isemptystr(int off)
{
    for (; yytext[off] && isspace(yytext[off]); off++)
	;
    if (yytext[off] == ';') {
	int i;
	for (i = off + 1; yytext[i]; i++) 
	    if (!isspace(yytext[i]))
		return 0;
	yyless(off);
	return 1;
    }
    return yytext[off] == 0;
}

char *
multiline_strip_tabs(char *text)
{
    if (char_to_strip)
	for (; *text && char_to_strip(*text); text++)
	    ;
    return text;
}

static char quote_transtab[] = "\\\\\"\"a\ab\bf\fn\nr\rt\t";

int
unquote_char(int c)
{
    char *p;

    for (p = quote_transtab; *p; p += 2) {
	if (*p == c)
	    return p[1];
    }
    return 0;
}

int
quote_char(int c)
{
    char *p;

    for (p = quote_transtab; *p; p += 2) {
	if (p[1] == c)
	    return p[0];
    }
    return 0;
}

static void
unescape_to_obstack(int c)
{
    if (c != '\n') {
	int t = unquote_char(c);
	if (t)
	    obstack_1grow(&stk, t);
	else {
	    config_warning(&locus, 0,
			   _("unknown escape sequence '\\%c'"),
			   c);
	    obstack_1grow(&stk, c);
	}
    }
}

void
line_add(char *text, size_t len)
{
    obstack_grow(&stk, text, len);
}

/* Same, but unescapes the last character from yytext */
void
line_add_unescape_last(char *text, size_t len)
{
    obstack_grow(&stk, text, len - 2);
    unescape_to_obstack(text[len - 1]);
}

void
multiline_add(char *s)
{
    if (multiline_unescape) {
	for (; *s; s++) {
	    if (*s == '\\') {
		unescape_to_obstack(s[1]);
		++s;
	    } else
		obstack_1grow(&stk, *s);
	}
    } else
	line_add(s, strlen(s));
}

void
line_begin()
{
    /* FIXME: nothing so far. Maybe prepare stk by calling obstack_finish? */
}

static int
is_tab(char c)
{
    return c == '\t';
}
 
static int
is_ws(char c)
{
    return c == '\t' || c == ' ';
}

void
multiline_begin(char *p)
{
    if (*p == '-') {
	if (*++p == ' ') {
	    char_to_strip = is_ws;
	    p++;
	} else
	    char_to_strip = is_tab;
    } else
	char_to_strip = NULL;
    if (*p == '\\') {
	p++;
	multiline_unescape = 0;
    } else if (*p == '"') {
	char *q;
 
	p++;
	multiline_unescape = 0;
	q = strchr(p, '"');
	multiline_delimiter_len = q - p;
    } else {
	multiline_delimiter_len = strcspn(p, " \t");
	multiline_unescape = 1;
    }

    /* Remove trailing newline */
    multiline_delimiter_len--;
    multiline_delimiter = xmalloc(multiline_delimiter_len + 1);
    memcpy(multiline_delimiter, p, multiline_delimiter_len);
    multiline_delimiter[multiline_delimiter_len] = 0;
    line_begin();
}

void
line_finish()
{
    obstack_1grow(&stk, 0);
    yylval.string = obstack_finish(&stk);
}

char *
line_finish0()
{
    obstack_1grow(&stk, 0);
    return obstack_finish(&stk);
}

void
multiline_finish()
{
    line_finish();
}

int
ident()
{
    char *p;
    
    for (p = yytext; *p && isspace(*p); p++)
	;
    obstack_grow(&stk, p, strlen(p));
    obstack_1grow(&stk, 0);
    yylval.string = obstack_finish(&stk);
    return IDENT;
}

void
config_lex_trace(int n)
{
    yy_flex_debug = n;
}

config_value_t *
config_value_dup(config_value_t *input)
{
    config_value_t *ptr = obstack_alloc(&stk, sizeof(*ptr));
    *ptr = *input;
    return ptr;
}


static int
assign_locus(dicod_locus_t *ploc, char *name, char *line, size_t *pxlines)
{
    char *p;

    if (name) {
	if (pxlines && (!ploc->file || strcmp(name, ploc->file)))
	    *pxlines = 0;
	ploc->file = install_text(name);
    }
    ploc->line = strtoul(line, &p, 10) - (pxlines ? *pxlines : 0);
    return *p != 0;
}

static void
parse_line(char *text, dicod_locus_t *ploc, size_t *pxlines)
{
    int rc = 1;
    int argc;
    char **argv;

    while (*text && isspace (*text))
	text++;
    text++;

    if (dico_argcv_get(text, "", NULL, &argc, &argv)) 
	config_error(ploc, 0, _("cannot parse #line line"));
    else {
	if (argc == 2)
	    rc = assign_locus(ploc, NULL, argv[1], pxlines);
	else if (argc == 3) 
	    rc = assign_locus(ploc, argv[2], argv[1], pxlines);
	else if (argc == 4) {
	    rc = assign_locus(ploc, argv[2], argv[1], 0);
	    if (rc == 0) {
		char *p;
		unsigned long x = strtoul(argv[3], &p, 10);
		rc = *p != 0;
		if (rc == 0)
		    *pxlines = x;
	    }
	} else 
	    config_error(ploc, 0, _("invalid #line statement"));
	
	if (rc) 
	    config_error(ploc, 0, _("malformed #line statement"));
    }
    dico_argcv_free(argc, argv);
}

static void
parse_line_cpp(char *text, dicod_locus_t *ploc, size_t *pxlines)
{
    int argc;
    char **argv;

    if (dico_argcv_get(text, "", NULL, &argc, &argv)) 
	config_error(ploc, 0, _("cannot parse #line line"));
    else if (argc < 3)
	config_error(ploc, 0, _("invalid #line statement"));
    else {
	if (assign_locus(ploc, argv[2], argv[1], pxlines))
	    config_error(ploc, 0, _("malformed #line statement"));
    }
    dico_argcv_free(argc, argv);
}

