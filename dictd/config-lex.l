%{
/* This file is part of Gjdict.
   Copyright (C) 2008 Sergey Poznyakoff

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <dictd.h>
#include <config-gram.h>
#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include <obstack.h>

static char *multiline_delimiter;
static size_t multiline_delimiter_len;
static int multiline_unescape;         /* Unescape here-document contents */
static int (*char_to_strip)(char);     /* Strip matching characters of each
					  here-document line */

gd_locus_t locus;
static ino_t source_inode;
static struct obstack stk;
    
static int number(void);
static int string(void);
static void multiline_begin(char *);
static void multiline_add(char *);
static void multiline_finish(void);
static char *multiline_strip_tabs(char *text);
static int ident();
static void config_include(void);
static int isemptystr(char *text);

static int
fillbuf(char *buf, int max_size)
{
    if (feof(yyin))
	return 0;
    else
	return fread(buf, 1, max_size, yyin);
} 
 
#undef YY_INPUT
#define YY_INPUT(buf,result,max_size) result = fillbuf(buf, max_size)
#define LEX_BUFFER_STATE YY_BUFFER_STATE
#define SET_BUFFER_STATE(s) do { \
        (s) = YY_CURRENT_BUFFER; \
        yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE)); \
} while (0)
#define RESTORE_BUFFER_STATE(s) do { \
        yy_delete_buffer(YY_CURRENT_BUFFER); \
        yy_switch_to_buffer(s); \
} while (0)

struct buffer_ctx {
    struct buffer_ctx *prev;
    gd_locus_t locus;
    ino_t i_node;
    FILE *yyin;
    LEX_BUFFER_STATE state;
};

static struct buffer_ctx *context_stack;

static struct buffer_ctx *ctx_lookup(ino_t ino);
static int push_source(const char *name);
static int pop_source(void);

struct buffer_ctx *
ctx_lookup(ino_t ino)
{
    struct buffer_ctx *ctx;

    for (ctx = context_stack; ctx; ctx = ctx->prev)
	if (ctx->i_node == ino)
	    break;
    return ctx;
}
        
int
push_source(const char *name)
{
    FILE *fp;
    struct buffer_ctx *ctx;
    struct stat st;
        
    if (stat(name, &st)) {
	config_error(&locus, errno, _("cannot stat `%s'"), name);
	return 1;
    }

    if (locus.file && st.st_ino == source_inode) {
	yyerror(_("recursive inclusion"));
	return 1;
    }
    if ((ctx = ctx_lookup(st.st_ino))) {
	yyerror(_("recursive inclusion"));
	if (ctx->prev)
	    config_error(&ctx->locus, 0,
			 _("`%s' already included here"), name);
      else
	  config_error(&locus, 0,
		       _("`%s' already included at top level"), name);
      return 1;
    }
                
    fp = fopen(name, "r");
    if (!fp) {
	config_error(&locus, errno, _("cannot open `%s'"), name);
	return 1;
    }

    /* Push current context */
    if (locus.file) {
	ctx = xmalloc(sizeof (*ctx));
	ctx->locus = locus;
	ctx->i_node = source_inode;
	ctx->yyin = yyin;
	ctx->prev = context_stack;
	context_stack = ctx;
      
	/* Switch to the new context */
	yyin = fp;
	SET_BUFFER_STATE(ctx->state);
    } else
	yyrestart(fp);
  
    locus.file = strdup(name);
    locus.line = 1;
    source_inode = st.st_ino;

    return 0;
}

int
pop_source()
{
    struct buffer_ctx *ctx;

    if (yyin)
	fclose(yyin);

    if (locus.file)
	free(locus.file);
    if (!context_stack) {
	yyin = NULL;
	locus.file = NULL;
	return 1;
    }
    /* Restore previous context */
    locus = context_stack->locus;
    locus.line++; /* because the #include rule did not increment it */
    source_inode = context_stack->i_node;
    RESTORE_BUFFER_STATE(context_stack->state);
    ctx = context_stack->prev;
    free(context_stack);
    context_stack = ctx;

    return 0;
}


%}
    
    
%x COMMENT ML STR

WS [ \t][ \t]*
ID [a-zA-Z_][a-zA-Z_0-9-]+

%%
         /* C-style comments */
"/*"         BEGIN(COMMENT);
<COMMENT>[^*\n]*        /* eat anything that's not a '*' */
<COMMENT>"*"+[^*/\n]*   /* eat up '*'s not followed by '/'s */
<COMMENT>\n             ++locus.line;
<COMMENT>"*"+"/"        BEGIN(INITIAL);
         /* Preprocessor directives (an extension) */
#[ \t]*include.*\n      { config_include(); }
         /* End-of-line comments */
#.*\n     { locus.line++; }
#.*     /* end-of-file comment */;
"//".*\n  { locus.line++; }
"//".*    /* end-of-file comment */;
        /* Identifiers */
^[ \t]*{ID}          return ident();
         /* Numbers */
0[0-7]*                     { return number(); }
0x[0-9a-fA-F][0-9a-fA-F]+   { return number(); }
[1-9][0-9]*                 { return number(); }
         /* Strings */
[a-zA-Z0-9_\.-]+            { return string(); }
         /* Quoted strings */
\"[^\\"\n]*\"         { return string(); }
\"[^\\"\n]*\\.        { BEGIN(STR);
                        line_begin();
		        line_add_unescape_last(yytext + 1, yyleng - 1); }
<STR>[^\\"\n]*\\.     { line_add_unescape_last(yytext, yyleng); }
<STR>[^\\"\n]*\"      { BEGIN(INITIAL);
                        if (yyleng > 1) 
                          line_add(yytext, yyleng - 1); 
                        line_finish();
		        return STRING; }
         /* Multiline strings */
"<<"(-" "?)?\\?{ID}[ \t]*#.*\n |
"<<"(-" "?)?\\?{ID}[ \t]*"//".*\n |
"<<"(-" "?)?\\?{ID}[ \t]*\n |
"<<"(-" "?)?\"{ID}\"[ \t]*#.*\n |
"<<"(-" "?)?\"{ID}\"[ \t]*"//".*\n |
"<<"(-" "?)?\"{ID}\"[ \t]*\n {
                        BEGIN(ML); multiline_begin(yytext+2); locus.line++; }
<ML>.*\n { char *p = multiline_strip_tabs(yytext);
           locus.line++;
	   
           if (!strncmp(p, multiline_delimiter, multiline_delimiter_len)
	       && isemptystr(p + multiline_delimiter_len))
	     {
	       free(multiline_delimiter);
	       multiline_delimiter = NULL;
	       BEGIN(INITIAL);
	       multiline_finish();
	       return MSTRING;
	     }
	    multiline_add(p); } 
{WS}     ;
         /* Other tokens */
\n       { locus.line++; } 
.        return yytext[0];
%%

int
yywrap()
{
  return pop_source();
}

static char *
get_file_name(char *p, char *endp, int *usepath)
{
    char exp, *name, *startp;
    int n;
  
    if (usepath)
	*usepath = 0;
    switch (*p) {
    case '"':
	exp = '"';
	break;

    case '<':
	exp = '>';
	if (usepath)
	    *usepath = 1;
	break;

    default:
	yyerror(_("preprocessor syntax"));
	return NULL;
    }

    for (startp = ++p; p < endp && *p != exp; p++)
	;

    if (*p != exp) {
	yyerror(_("missing closing quote in preprocessor statement"));
	return NULL;
    }
  
    n = p - startp;
    name = xmalloc(n + 1);
    memcpy(name, startp, n);
    name[n] = 0;
    return name;
}

void
config_include()
{
    char *p, *endp = yytext + yyleng, *name;
    int usepath;
    
    p = strstr(yytext, "include");
    for (p += 7; p < endp && isspace(*p); p++)
	;

    name = get_file_name(p, endp, &usepath);
    if (!name)
	return;
    if (access(name, R_OK)) 
	config_error(&locus, errno, _("cannot access `%s'"), name);
    else 
	push_source(name);
    free(name);
}

int
config_lex_begin(const char *name)
{
    obstack_init(&stk);
    return push_source(name);
}

void
config_lex_end()
{
    while (pop_source() == 0)
	;
}

int
number()
{
    yylval.number = strtoul(yytext, NULL, 0);
    return NUMBER;
}

int
string()
{
    obstack_grow(&stk, yytext + 1, yyleng - 2);
    obstack_1grow(&stk, 0);
    yylval.string = obstack_finish(&stk);
    return STRING; 
}

int
isemptystr(char *text)
{
    for (; *text && isspace(*text); text++)
	;
    return *text == 0;
}

char *
multiline_strip_tabs(char *text)
{
    if (char_to_strip)
	for (; *text && char_to_strip(*text); text++)
	    ;
    return text;
}

static char quote_transtab[] = "\\\\a\ab\bf\fn\nr\rt\t";

int
unquote_char(int c)
{
    char *p;

    for (p = quote_transtab; *p; p += 2) {
	if (*p == c)
	    return p[1];
    }
    return c;
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
    if (text[len - 1] != '\n')
	obstack_1grow(&stk, unquote_char(text[len - 1]));
}

void
multiline_add(char *s)
{
    if (multiline_unescape) {
	for (; *s; s++) {
	    if (*s == '\\') {
		if (s[1] != '\n')
		    obstack_1grow(&stk, unquote_char(s[1]));
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
    
    multiline_delimiter = xmalloc(multiline_delimiter_len + 1);
    memcpy(multiline_delimiter, p, multiline_delimiter_len);
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


