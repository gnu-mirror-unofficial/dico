%{
#if defined(HAVE_CONFIG_H)
# include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <gjdictd.h>
#include "server.h"
	
extern struct sockaddr_in data_addr;
    
FILE *in;

struct keyword {
	int matchlen;
	char *name;
	int code;
};
static struct keyword search_key[];

static int decode_keyword(struct keyword *, char *);

int key_cnt;
int key_list[TreeLast] ;

static char *mod_str[] = {
     "SEQ", "EXACT", "CLOSEST"
};

%}

%token KEY RECORD
%token GET ENTRY NEXT PREV STR DONE STAT QUIT
%token SEQ CLOSEST EXACT
%token XREF
%token BIN ASC
%token PORT
%token PASV
%token NOOP
%token <number> NUMBER
%token <string> STRING

%union {
    char string[128];
    int number;
    Matchdir dir;
    struct {
	int mode;
	Matchdir dir;
	int cnt;
    } query_mod;
}

%type <number> key 
%type <number> maybe_number
%type <query_mod> dir_mod query_mod

%%

input       : list
            | stmt
            ;

list        : line
            | list line
            ;

line        : stmt '\n'
            | error '\n'
              {
		  yyerrok;
		  yyclearin;
	      }
            ;

stmt        : fmt_stmt
            | query_seq
            | stat_stmt
            | quit_stmt
            | port_stmt
            | key_stmt
            | noop_stmt
            ;

fmt_stmt    : BIN
              {
		  binary = 1;
		  debug(DEBUG_GRAM, 1)("BIN");
		  msg(RC_OK, "accepting unencoded input");
	      }
            | ASC
              {
		  binary = 0;
		  debug(DEBUG_GRAM, 1)("ASC");
		  msg(RC_OK, "accepting encoded input");
	      }
            ;

key_stmt    : KEY key
              {
		  if ($2 >= 0) {
		      if (have_tree($2)) {
			  search_ctl.tree = $2;
			  msg(RC_OK, "search tree %s", tree_name($2));
			  debug(DEBUG_GRAM, 1)("KEY %s", tree_name($2));
		      } else {
			  msg(RC_ERR, "search tree %s not available",
			      tree_name($2));
		      }
		  }
	      }
            ;

query_seq   : get query_stmt_list done
            | get done
            ;

get         : GET 
              {
		  debug(DEBUG_GRAM, 1)("GET");
		  if (opendata())
		      YYERROR;
	      } '\n'
            ;


done        : DONE 
              {
		  closedata();
	      }
            ;

query_stmt_list: query_stmt '\n'
            | query_stmt_list query_stmt '\n'
            | error '\n'
              {
		  yyerrok;
		  yyclearin;
	      }
            ;

query_stmt  : /* empty */
            | ENTRY STRING query_mod
              {
		  debug(DEBUG_GRAM, 1)("ENTRY \"%s\"", $2);
		  set_mode($3.mode, $3.dir, $3.cnt);
		  find($2);
	      }
            | RECORD NUMBER
              {
		  debug(DEBUG_GRAM, 1)("RECORD %d", $2);
		  getrecord($2);
	      }
            | STR NUMBER
              {
		  debug(DEBUG_GRAM, 1)("STR %d", $2);
		  getstr($2);
	      }
            | XREF NUMBER
              {
		  debug(DEBUG_GRAM, 1)("XREF %d", $2);
		  xref($2);
	      }
            ;

query_mod   : dir_mod
              {
		  $$ = $1;
		  $$.mode = QUERY_SEQ;
		  debug(DEBUG_GRAM, 1)
		      ("mode %s, dir NEXT, count 1",
		       mod_str[$$.mode]);
	      }
            | SEQ dir_mod
              {
		  $$ = $2;
		  $$.mode = QUERY_SEQ;
		  debug(DEBUG_GRAM, 1)
		      ("mode %s, dir NEXT, count 1",
		       mod_str[$$.mode]);
	      }
            | CLOSEST
              {
		  $$.mode = QUERY_CLOSEST;
		  debug(DEBUG_GRAM, 1)
		      ("mode %s", mod_str[$$.mode]);
	      }
            | EXACT
              {
		  $$.mode = QUERY_EXACT;
		  debug(DEBUG_GRAM, 1)("mode %s", mod_str[$$.mode]);
	      }
            ;

dir_mod     : /* empty */
              {
		  $$.dir = MatchNext;
		  $$.cnt = 1;
	      }
            | NEXT maybe_number
              {
		  $$.dir = MatchNext;
		  $$.cnt = $2;
	      }
            | PREV maybe_number
              {
		  $$.dir = MatchPrev;
		  $$.cnt = $2;
	      }
            ;

maybe_number: /* empty */
              {
		  $$ = 1;
	      }
            | NUMBER
            ;

stat_stmt   : STAT
              {
		  server_stat();
	      }
            ;

quit_stmt   : QUIT
              {
		  debug(DEBUG_GRAM, 1)("QUIT");
		  YYACCEPT;
	      }
            ;

key         : STRING
              {
		  int k = decode_keyword(search_key, $1);
		  if (k == -1)
		      YYERROR;
		  $$ = k;
	      }
            ;

port_stmt   : PORT NUMBER ',' NUMBER ',' NUMBER ',' NUMBER ','
                   NUMBER ',' NUMBER
              {
		  debug(DEBUG_GRAM, 1)
		      ("PORT %d,%d,%d,%d,%d,%d",
		       $2,$4,$6,$8,$10,$12);
		  if (!binary)
		      msg(RC_ERR, "PORT not supported in ASCII mode");
		  else {
		      if ($2 > 255 || $4 > 255 || $6 > 255 || $8 > 255) 
			  msg(RC_ERR, "invalid IP address");
		      else 
			  active($2, $4, $6, $8, $10, $12);
		  }
	      }
            | PASV
              {
		  debug(DEBUG_GRAM, 1)("PASV");
		  if (!binary) 
		      msg(RC_ERR, "PASV not supported in ASCII mode");
		  else 
		      passive();
	      }
            ;

noop_stmt   : NOOP
              {
		  debug(DEBUG_GRAM, 1)("NOOP");
		  msg(RC_OK, "Ok");
	      }
            ;
%%

struct keyword keywords[] = {
    { 1, "ASC", ASC },
    { 2, "BIN", BIN },
    { 1, "CLOSEST", CLOSEST },
    { 1, "DONE", DONE },
    { 2, "ENTRY", ENTRY },
    { 1, "EXACT", EXACT },
    { 1, "GET", GET },
    { 1, "KEY", KEY },
    { 2, "NEXT", NEXT },
    { 2, "NOOP", NOOP },
    { 2, "PASV", PASV },
    { 2, "PORT", PORT },
    { 2, "PREV", PREV },
    { 1, "QUIT", QUIT },
    { 1, "RECORD", RECORD },
    { 2, "SEQ", SEQ },
    { 3, "STAT", STAT },
    { 3, "STR", STR },
    { 1, "XREF", XREF },
    { 0 }
};

struct keyword search_key[] = {
    { 1, "BUSHU", TreeBushu },
    { 1, "CORNER", TreeCorner },
    { 1, "ENGLISH", TreeWords },
    { 1, "FREQUENCY", TreeFreq },
    { 1, "GRADE", TreeGrade },
    { 1, "HALPERN", TreeHalpern },
    { 1, "JIS", TreeJis },
    { 1, "KANJI", TreeKanji },
    { 1, "NELSON", TreeNelson },
    { 1, "PINYIN", TreePinyin },
    { 1, "ROMAJI", TreeRomaji },
    { 1, "SKIP", TreeSkip },
    { 1, "UNICODE", TreeUnicode },
    { 1, "XREF", TreeXref },
    { 1, "YOMI", TreeYomi },
    { 0 }
};


int
decode_keyword(struct keyword *kw, char *string)
{
    int len = strlen(string);

    for ( ; kw->matchlen; kw++)
	if (len >= kw->matchlen && len <= strlen(kw->name)
	    && strncasecmp(kw->name, string, len) == 0)
	    return kw->code;
    return -1;
}

int
yylex()
{
    int c, k;
    int too_long = 0;
    char *p;
    
    alarm(inactivity_timeout);
    
    while ((c = fgetc(in)) != EOF && (c == ' ' || c == '\t'))
	;
    if (c == EOF) 
	return 0;

    if (isalpha(c)) {
	p = yylval.string;
	
	do { 
	    if (p >= yylval.string + sizeof(yylval.string) - 1) {
		if (!too_long) {
		    logmsg(L_ERR, 0, "token buffer overflow");
		    too_long = 1;
		} 
	    } else 
		*p++ = c;
	} while ((c = fgetc(in)) != EOF && (isalnum(c) || c == '_'));
	*p = 0;

	if (c != EOF)
	    ungetc(c, in);

	debug(DEBUG_GRAM, 20)
	    ("token (STR/KWD) `%s'", yylval.string);
	
	if ((k = decode_keyword(keywords, yylval.string)) >= 0)
	    return k;
	
	return STRING;
    } 

    if (isdigit(c)) {
	p = yylval.string;
	
	do { 
	    if (p >= yylval.string + sizeof(yylval.string) - 1) {
		if (!too_long) {
		    logmsg(L_ERR, 0, "token buffer overflow");
		    too_long = 1;
		}
	    } else 
		*p++ = c;
	} while ((c = fgetc(in)) != EOF && (isdigit(c)));
	*p = 0;

	if (c != EOF)
	    ungetc(c, in);

	debug(DEBUG_GRAM, 20)
	    ("token (NUMBER) %s", yylval.string);

	yylval.number = atoi(yylval.string);
	
	return NUMBER;
    } 

    
    if (c == '"') {
	p = yylval.string;
	
	while ((c = fgetc(in)) != EOF && c != '"') { 
	    if (p >= yylval.string + sizeof(yylval.string) - 1) {
		logmsg(L_ERR, 0, "token buffer overflow");
		break;
	    }
	    if (c == '\\')  
		if ((c = fgetc(in)) == EOF)
		    break;
	    *p++ = c;
	} 
	*p = 0;

	debug(DEBUG_GRAM, 20)
	    ("token (STRING) %s", yylval.string);

	return STRING;
    }
    
    if (c == '\r') 
	c = fgetc(in);

    debug(DEBUG_GRAM, 20)
	("token (CHAR) `%c'", c);
	
    return c;
}

int
yyerror(char *s)
{
    msg(RC_ERR, "%s", s);
    return 0;
}

void
enable_yydebug()
{
#if YYDEBUG
# define ENABLE_DEBUG
#elif defined(YACC_DEBUG)
# define ENABLE_DEBUG
#endif
#ifdef ENABLE_DEBUG
    extern int yydebug;
    yydebug = 1;
#else
    logmsg(L_ERR|L_CONS, 0,
	   "gjdictd compiled without parser debugging support");
#endif
}

