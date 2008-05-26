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

#include <dictd.h>
#include <config-gram.h>

static struct config_keyword config_keywords;
static struct config_keyword *cursect;
static dico_list_t sections;
int config_error_count;    

void *target_ptr(struct config_keyword *kwp);
void stmt_begin(struct config_keyword *kwp, config_value_t tag);
void stmt_end(struct config_keyword *kwp);
struct config_keyword *find_keyword(const char *ident);

void process_ident(struct config_keyword *kwp, config_value_t *value);
 
%}

%union {
    char *string;
    config_value_t value;
    dico_list_t list;
    struct config_keyword *kw;
}

%token <string> IDENT STRING QSTRING MSTRING
%type <string> string slist
%type <list> slist0
%type <value> value tag
%type <list> values list
%type <kw> ident

%%

input   : stmtlist
        ;

stmtlist: stmt
        | stmtlist stmt
        ;

stmt    : simple
        | block
        ;

simple  : ident value ';'
          {
	      process_ident($1, &$2);
	  }
        | ident MSTRING
          {
	      config_value_t value;
	      value.type = TYPE_STRING;
	      value.v.string = $2;
	      process_ident($1, &value);
	  }
        ;

block   : ident tag { stmt_begin($<kw>1, $<value>2); } '{' stmtlist '}' opt_sc
          {
	      stmt_end($1);
	  }
        ;

ident   : IDENT
          {
	      $$ = find_keyword($1);
	      if (!$$) 
		  config_error(&locus, 0, _("Unknown keyword"));
	  }
        ;

tag     : /* empty */
         {
	     $$.type = TYPE_STRING;
	     $$.v.string = NULL;
	 }
        | value
        ;

value   : string
          {
	      $$.type = TYPE_STRING;
	      $$.v.string = $1;
	  }
        | list
          {
	      $$.type = TYPE_LIST;
	      $$.v.list = $1;
	  }
        ;

string  : STRING
        | MSTRING
        | QSTRING
        | slist
        ;

slist   : slist0
          {
	      dico_iterator_t itr = xdico_iterator_create($1);
	      char *p;
	      line_begin();
	      for (p = dico_iterator_first(itr); p;
		   p = dico_iterator_next(itr)) 
		  line_add(p, strlen(p));
	      $$ = line_finish0();
	      dico_iterator_destroy(&itr);
	      dico_list_destroy(&$1, NULL, NULL);
	  }
        ;

slist0  : QSTRING QSTRING
          {
	      $$ = xdico_list_create();
	      xdico_list_append($$, $1);
	      xdico_list_append($$, $2);
	  }
        | slist0 QSTRING
          {
	      xdico_list_append($1, $2);
	      $$ = $1;
	  }
        ;

list    : '(' values ')'
          {
	      $$ = $2;
	  }
        | '(' values ',' ')'
          {
	      $$ = $2;
	  }
        ;

values  : value
          {
	      $$ = xdico_list_create();
	      xdico_list_append($$, config_value_dup(&$1));
	  }
        | values ',' value
          {
	      xdico_list_append($1, config_value_dup(&$3));
	      $$ = $1;
	  }
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
    if (!locus || !locus->file)
	dico_vlog(L_ERR, errcode, fmt, ap);
    else {
	fprintf(stderr, "%s:%d: ", locus->file, locus->line);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
    }
    va_end(ap);
    config_error_count++;
}

void
config_set_keywords(struct config_keyword *kwd)
{
    config_keywords.kwd = kwd;
}

int
config_parse(const char *name)
{
    int rc;
    if (config_lex_begin(name))
	return 1;
    cursect = &config_keywords;
    dico_list_destroy(&sections, NULL, NULL);
    rc = yyparse();
    config_lex_end();
    if (config_error_count)
	rc = 1;
    return rc;
}

void
config_gram_trace(int n)
{
    yydebug = n;
}



void *
target_ptr(struct config_keyword *kwp)
{
    char *base;
    
    if (kwp->varptr)
	base = (char*) kwp->varptr + kwp->offset;
    else if (cursect && cursect->callback_data)
	base = (char*) cursect->callback_data + kwp->offset;
    else
	base = NULL;
    return base;
}
    
void
stmt_begin(struct config_keyword *kwp, config_value_t tag)
{
    void *target;

    if (!sections)
	sections = xdico_list_create();
    dico_list_push(sections, cursect);
    if (kwp) {
	target = target_ptr(kwp);
	cursect = kwp;
	if (kwp->callback)
	    kwp->callback(callback_section_begin,
			  &locus, /* FIXME */
			  target,
			  &tag,
			  &kwp->callback_data);
    } else
	/* install "ignore-all" section */
	cursect = kwp;
}

void
stmt_end(struct config_keyword *kwp)
{
    if (cursect && cursect->callback)
	cursect->callback(callback_section_end,
			  &locus, /* FIXME */
			  kwp ? target_ptr(kwp) : NULL,
			  NULL,
			  &cursect->callback_data);
    cursect = dico_list_pop(sections);
}

int
fake_callback(enum cfg_callback_command cmd,
	      gd_locus_t *locus,
	      void *varptr,
	      config_value_t *value,
	      void *cb_data)
{
    return 0;
}

static struct config_keyword fake = {
    "*",
    NULL,
    NULL,
    cfg_void,
    NULL,
    0,
    fake_callback,
    NULL,
    &fake
};

struct config_keyword *
find_keyword(const char *ident)
{
    struct config_keyword *kwp;

    if (cursect && cursect != &fake) {
	for (kwp = cursect->kwd; kwp->ident; kwp++)
	    if (strcmp(kwp->ident, ident) == 0)
		return kwp;
    } else {
	return &fake;
    }
    return NULL;
}

int
string_to_signed(intmax_t *sval, const char *string,
		 intmax_t minval, intmax_t maxval)
{
    intmax_t t;
    char *p;
    
    t = strtoimax(string, &p, 0);
    if (*p) {
	config_error(&locus, 0, _("cannot convert `%s' to number"),
		     string);
	return 1;
    } else if (t < minval || t > maxval) {
	config_error(&locus, 0,
		     _("%s: value out of allowed range %"PRIiMAX"..%"PRIiMAX),
		     string, minval, maxval);
	return 1;
    }
    *sval = t;
    return 0;
}
    
int
string_to_unsigned(uintmax_t *sval, const char *string, uintmax_t maxval)
{
    uintmax_t t;
    char *p;
    
    t = strtoumax(string, &p, 0);
    if (*p) {
	config_error(&locus, 0, _("cannot convert `%s' to number"),
		     string);
	return 1;
    } else if (t > maxval) {
	config_error(&locus, 0,
		     _("%s: value out of allowed range 0..%"PRIuMAX),
		     string, maxval);
	return 1;
    }
    *sval = t;
    return 0;
}

static int
string_to_bool(const char *string, int *pval)
{
    if (strcmp(string, "yes") == 0
	|| strcmp(string, "true") == 0
	|| strcmp(string, "t") == 0
	|| strcmp(string, "1") == 0)
	*pval = 1;
    else if (strcmp(string, "no") == 0
	     || strcmp(string, "false") == 0
	     || strcmp(string, "nil") == 0
	     || strcmp(string, "0") == 0)
	*pval = 0;
    else {
	config_error(&locus, 0,
		     _("%s: not a valid boolean value"),
		     string);
	return 1;
    }
    return 0;
}

static int
string_to_host(struct in_addr *in, const char *string)
{
    if (inet_aton(string, in) == 0) {
	struct hostent *hp;

	hp = gethostbyname(string);
	if (hp == NULL)
	    return 1;
	memcpy(in, hp->h_addr, sizeof(struct in_addr));
    }
    return 0;
}

static int
string_to_sockaddr(sockaddr_union_t *s, const char *string)
{
    if (string[0] == '/') {
	if (strlen(string) >= sizeof(s->s_un.sun_path)) {
	    config_error(&locus, 0,
			 _("%s: UNIX socket name too long"),
			 string);
	    return 1;
	}
	s->s_un.sun_family = AF_UNIX;
	strcpy(s->s_un.sun_path, string);
    } else {
	char *p = strchr(string, ':');
	char *host;
	size_t len;
	struct sockaddr_in sa;
	struct servent *serv;

	sa.sin_family = AF_INET;
	if (!p) {
	    config_error(&locus, 0,
			 _("%s: not a valid socket address"),
			 string);
	    return 1;
	}
	len = p - string;
	host = xmalloc(len + 1);
	memcpy(host, string, len);
	host[len] = 0;
	
	if (string_to_host(&sa.sin_addr, host)) {
	    config_error(&locus, 0,
			 _("%s: not a valid IP address or hostname"), host);
	    free(host);
	    return 1;
	}
	free(host);

	p++;
	serv = getservbyname(p, "tcp");
	if (serv != NULL)
	    sa.sin_port = serv->s_port;
	else {
	    unsigned long l;
	    char *q;
	    
	    /* Not in services, maybe a number? */
	    l = strtoul(p, &q, 0);

	    if (*p || l > USHRT_MAX) {
		config_error(&locus, 0,
			     _("%s: not a valid port number"), p);
		return 1;
	    }
	    sa.sin_port = htons(l);
	}
	s->s_in = sa;
    }
    return 0;
}

static int
string_convert(void *target, enum config_data_type type, const char *string)
{
    uintmax_t uval;
    intmax_t sval;
	
    switch (type) {
    case cfg_void:
	abort();
	    
    case cfg_string:
	*(const char**)target = string;
	break;
	    
    case cfg_short:
	if (string_to_signed(&sval, string, SHRT_MIN, SHRT_MAX) == 0)
	    *(short*)target = sval;
	else
	    return 1;
	break;
	    
    case cfg_ushort:
	if (string_to_unsigned(&uval, string, USHRT_MAX) == 0)
	    *(unsigned short*)target = uval;
	else
	    return 1;
	break;
	    
    case cfg_bool:
	return string_to_bool(string, (int*)target);
	    
    case cfg_int:
	if (string_to_signed(&sval, string, INT_MIN, INT_MAX) == 0)
	    *(int*)target = sval;
	else
	    return 1;
	break;
	    
    case cfg_uint:
	if (string_to_unsigned(&uval, string, UINT_MAX) == 0)
	    *(unsigned int*)target = uval;
	else
	    return 1;
	break;
	    
    case cfg_long:
	if (string_to_signed(&sval, string, LONG_MIN, LONG_MAX) == 0)
	    *(long*)target = sval;
	else
	    return 1;
	break;
	    
    case cfg_ulong:
	if (string_to_unsigned(&uval, string, ULONG_MAX) == 0)
	    *(unsigned long*)target = uval;
	else
	    return 1;
	break;
	    
    case cfg_size:
	if (string_to_unsigned(&uval, string, SIZE_MAX) == 0)
	    *(size_t*)target = uval;
	else
	    return 1;
	break;
	    
    case cfg_intmax:
	return string_to_signed((intmax_t*)target, string,
				INTMAX_MIN, INTMAX_MAX);
	    
    case cfg_uintmax:
	return string_to_unsigned((uintmax_t*)target, string, UINTMAX_MAX);
	    
    case cfg_time:
	/*FIXME: Use getdate */
	if (string_to_unsigned(&uval, string, (time_t)-1) == 0)
	    *(time_t*)target = uval;
	else
	    return 1;
	break;

    case cfg_ipv4:
	if (inet_aton(string, (struct in_addr *)target)) {
	    config_error(&locus, 0, _("%s: not a valid IP address"), string);
	    return 1;
	}
	break;
	
    case cfg_host:
	if (string_to_host((struct in_addr *)target, string)) {
	    config_error(&locus, 0,
			 _("%s: not a valid IP address or hostname"), string);
	    return 1;
	}
	break;    
	    
    case cfg_sockaddr:
	return string_to_sockaddr((sockaddr_union_t*)target, string);
	
	/* FIXME: */
    case cfg_cidr:
	    
    case cfg_section:
	config_error(&locus, 0, _("INTERNAL ERROR at %s:%d"), __FILE__,
		     __LINE__);
	abort();
    }
    return 0;
}

size_t config_type_size[] = {
    0                        /* cfg_void */,
    sizeof(char*)            /* cfg_string */,
    sizeof(short)            /* cfg_short */,
    sizeof(unsigned short)   /* cfg_ushort */,
    sizeof(int)              /* cfg_int */,
    sizeof(unsigned int)     /* cfg_uint */,
    sizeof(long)             /* cfg_long */,
    sizeof(unsigned long)    /* cfg_ulong */,
    sizeof(size_t)           /* cfg_size */,
/*    cfg_off,*/
    sizeof(uintmax_t)        /* cfg_uintmax */,
    sizeof(intmax_t)         /* cfg_intmax */,
    sizeof(time_t)           /* cfg_time */,
    sizeof(int)              /* cfg_bool */,
    sizeof(struct in_addr)   /* cfg_ipv4 */,
    0                        /* FIXME: cfg_cidr */,
    sizeof(struct in_addr)   /* cfg_host */, 
    sizeof(sockaddr_union_t) /* cfg_sockaddr */,
    0                        /* cfg_section */
};

void
process_ident(struct config_keyword *kwp, config_value_t *value)
{
    void *target;

    if (!kwp)
	return;

    target = target_ptr(kwp);

    if (kwp->callback)
	kwp->callback(callback_set_value,
		      &locus, /* FIXME */
		      target,
		      value,
		      &kwp->callback_data);
    else if (value->type == TYPE_LIST) {
	if (CFG_IS_LIST(kwp->type)) {
	    dico_iterator_t itr = xdico_iterator_create(value->v.list);
	    enum config_data_type type = CFG_TYPE(kwp->type);
	    int num = 1;
	    void *p;
	    dico_list_t list = xdico_list_create();
	    
	    for (p = dico_iterator_first(itr); p;
		 p = dico_iterator_next(itr), num++) {
		config_value_t *vp = p;
		size_t size;

		if (type >= DICO_ARRAY_SIZE(config_type_size)
		    || (size = config_type_size[type]) == 0) {
		    config_error(&locus, 0,
				 _("INTERNAL ERROR at %s:%d: "
				   "unhandled data type %d"),
				 __FILE__, __LINE__, type);
		    abort();
		}
		
		if (vp->type != TYPE_STRING)
		    config_error(&locus, 0,
				 _("%s: incompatible data type in list item #%d"),
				 kwp->ident, num);
		else {
		    void *ptr = xmalloc(size);
		    if (string_convert(ptr, type, vp->v.string) == 0) 
			xdico_list_append(list, ptr);
		    else
			free(ptr);
		}
	    }
	    dico_iterator_destroy(&itr);
	    *(dico_list_t*)target = list;
	} else {
	    config_error(&locus, 0, _("incompatible data type for `%s'"),
			 kwp->ident);
	    return;
	}
    } else if (CFG_IS_LIST(kwp->type)) {
	dico_list_t list = xdico_list_create();
	enum config_data_type type = CFG_TYPE(kwp->type);
	size_t size;
	void *ptr;
	
	if (type >= DICO_ARRAY_SIZE(config_type_size)
	    || (size = config_type_size[type]) == 0) {
	    config_error(&locus, 0,
			 _("INTERNAL ERROR at %s:%d: unhandled data type %d"),
			 __FILE__, __LINE__, type);
	    abort();
	}
	ptr = xmalloc(size);
	if (string_convert(ptr, type, value->v.string)) {
	    free(ptr);
	    dico_list_destroy(&list, NULL, NULL);
	    return;
	}
	xdico_list_append(list, ptr);
	*(dico_list_t*)target = list;
    } else
	string_convert(target, CFG_TYPE(kwp->type), value->v.string);
}

