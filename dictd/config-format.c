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
#include <gettext.h>

static const char *
config_data_type_string(enum config_data_type type)
{
    switch (type) {
    case cfg_void:
	return "void";
	
    case cfg_string:
	return "string";
	
    case cfg_short:
    case cfg_ushort:
    case cfg_int:
    case cfg_uint:
    case cfg_long:
    case cfg_ulong:
    case cfg_size:
/*  case  cfg_off:*/
    case cfg_uintmax:
    case cfg_intmax:
	return "number";
	
    case cfg_time:
	return "time";
	
    case cfg_bool:
	return "boolean";
	
    case cfg_ipv4:
	return "IPv4";
	
    case cfg_cidr:
	return "CIDR";
	
    case cfg_host:
	return "hostname";
	
    case cfg_sockaddr:
	return "sock-addr";
	
    case cfg_section:
	return "section";
    }
    return "UNKNOWN?";
}

static void
format_level(FILE *stream, int level)
{
    while (level--)
	fprintf(stream, "  ");
}

void
format_docstring(FILE *stream, const char *docstring, int level)
{
    size_t len = strlen(docstring);
    int width = 78 - level * 2;

    if (width < 0) {
	width = 78;
	level = 0;
    }
  
    while (len) {
	size_t seglen;
	const char *p;
      
	for (seglen = 0, p = docstring; p < docstring + width && *p; p++) {
	    if (*p == '\n') {
		seglen = p - docstring;
		break;
	    }
	    if (isspace(*p))
		seglen = p - docstring;
	}
	if (seglen == 0 || *p == 0)
	    seglen = p - docstring;

	format_level(stream, level);
	fprintf(stream, "# ");
	fwrite(docstring, seglen, 1, stream);
	fputc('\n', stream);
	len -= seglen;
	docstring += seglen;
	if (*docstring == '\n')	{
	    docstring++;
	    len--;
	} else
	    while (*docstring && isspace(*docstring)) {
		docstring++;
		len--;
	    }
    }
}

static void
format_simple_statement(FILE *stream, struct config_keyword *kwp, int level)
{
    char *argstr;
    
    if (kwp->docstring)
	format_docstring(stream, kwp->docstring, level);
    format_level(stream, level);

    if (kwp->argname) 
	argstr = kwp->argname;
    else
	argstr = N_("arg");

    if (strchr("<[", argstr[0]))
	fprintf(stream, "%s %s;\n", kwp->ident, gettext(argstr));
    else if (strchr(argstr, ':'))
	fprintf(stream, "%s <%s>;\n", kwp->ident, gettext(argstr));
    else {
	fprintf(stream, "%s <%s: ", kwp->ident, gettext(argstr));
	if (kwp->type & CFG_LIST)
	    fprintf(stream, "list of %s",
		    gettext(config_data_type_string(CFG_TYPE(kwp->type))));
	else
	    fprintf(stream, "%s",
		    gettext(config_data_type_string(kwp->type)));
	fprintf(stream, ">;\n");
    }
}

static void
format_block_statement(FILE *stream, struct config_keyword *kwp, int level)
{
    if (kwp->docstring)
	format_docstring(stream, kwp->docstring, level);
    format_level (stream, level);
    fprintf(stream, "%s", kwp->ident);
    if (kwp->argname) 
	fprintf(stream, " <%s>", gettext(kwp->argname));
    fprintf(stream, " {\n");
    format_statement_array(stream, kwp->kwd, 0, level + 1);
    format_level (stream, level);
    fprintf(stream, "}\n");
}

void
format_statement_array(FILE *stream, struct config_keyword *kwp, int n,
		       int level)
{
    for (; kwp->ident; kwp++, n++) {
	if (n)
	    fputc('\n', stream);
	if (kwp->type == cfg_section)
	    format_block_statement(stream, kwp, level);
	else
	    format_simple_statement(stream, kwp, level);
    }
}

   
