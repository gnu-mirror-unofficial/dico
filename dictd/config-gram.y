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

%}

%union {
    char *string;
    unsigned long number;
}

%token <string> IDENT STRING QSTRING MSTRING
%token <number> NUMBER
%type <string> string slist

%%

input   : stmtlist
        ;

stmtlist: stmt
        | stmtlist stmt
        ;

stmt    : simple
        | block
        ;

simple  : IDENT value ';'
        | IDENT MSTRING
        ;

block   : IDENT tag '{' stmtlist '}' opt_sc
        ;

tag     : /* empty */
        | value
        ;

value   : NUMBER
        | string
        | list
        | addr
        ;

string  : STRING
        | MSTRING
        | QSTRING
        | slist
        ;

slist   : slist0
          {
	      $$ = line_finish0();
	  }
        ;

slist0  : QSTRING QSTRING
          {
	      line_begin();
	      line_add($1, strlen($1));
	      line_add($2, strlen($2));
	  }
        | slist QSTRING
          {
	      line_add($2, strlen($2));
	  }
        ;

list    : '(' values ')'
        | '(' values ',' ')'
        ;

values  : value
        | values ',' value
        ;

addr    : STRING ':' NUMBER
        | STRING ':' STRING
        ; 

opt_sc  : /* empty */
        | ';'
        ;

%%

int
yyerror(char *s)
{
    config_error(&locus, 0, "%s", s);
    return 0;
}

void
config_error(gd_locus_t *locus, int errcode, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "%s:%d: ", locus->file, locus->line);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

int
config_parse(const char *name)
{
    int rc;
    if (config_lex_begin(name))
	return 1;
    rc = yyparse();
    config_lex_end();
    return rc;
}

void
config_gram_trace(int n)
{
    yydebug = n;
}
