/* This file is part of Gjdict.
   Copyright (C) 1998-2000, 2008 Sergey Poznyakoff

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
#include <pwd.h>
#include <grp.h>

int foreground;     /* Run in foreground mode */
int single_process; /* Single process mode */
int log_to_stderr;  /* Log to stderr */
/* Location of the default configuration file */
char *config_file = SYSCONFIG "/dictd.conf" ; 

/* Operation mode */
int mode = MODE_DAEMON;

/* Maximum number of children in allowed in daemon mode. */
unsigned int max_children;

/* Syslog parameters: */ 
const char *log_tag; 
int log_facility;

/* Server information (for SHOW INFO command) */
const char *server_info;

/* List of sockets to listen on for the requests */
dict_list_t /* of struct sockaddr */ *listen_addr;

/* Run as this user */
struct passwd *user;
/* Retain these supplementary groups when switching to the user privileges. */
dict_list_t /* of gid_t */ *group_list;

/* List of configured dictionary handlers */
dict_list_t /* of dictd_handler_t */ *handler_list;

/* List of configured dictionaries */
dict_list_t /* of dictd_dictionary_t */ *dictionary_list;


/* Configuration */
int
set_user(enum cfg_callback_command cmd,
	 gd_locus_t *locus,
	 void *varptr,
	 config_value_t *value,
	 void *cb_data)
{
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value buf found list"));
	return 1;
    }
    
    user = getpwnam(value->v.string);
    if (!user) {
	config_error(locus, 0, _("%s: no such user"), value->v.string);
	return 1;
    }
    return 0;
}

static int set_supp_group(enum cfg_callback_command cmd,
			  gd_locus_t *locus,
			  void *varptr,
			  config_value_t *value,
			  void *cb_data);

static int
set_supp_group_iter(void *item, void *data)
{
    return set_supp_group(callback_set_value,
			  (gd_locus_t *)data,
			  NULL,
			  (config_value_t *)item,
			  NULL);
}
	
static int
set_supp_group(enum cfg_callback_command cmd,
	       gd_locus_t *locus,
	       void *varptr,
	       config_value_t *value,
	       void *cb_data)
{
    if (!group_list)
	group_list = dict_list_create();
    
    if (value->type == TYPE_LIST)
	dict_list_iterate(value->v.list, set_supp_group_iter, locus);
    else {
	struct group *group = getgrnam(value->v.string);
	if (group)
	    dict_list_append(group_list, (void*)group->gr_gid);
	else {
	    config_error(locus, 0, _("%s: unknown group"), value->v.string);
	    return 1;
	}
    }
    return 0;
}

int
set_mode(enum cfg_callback_command cmd,
	 gd_locus_t *locus,
	 void *varptr,
	 config_value_t *value,
	 void *cb_data)
{
    static struct xlat_tab tab[] = {
	{ "daemon", MODE_DAEMON },
	{ "inetd", MODE_INETD },
	{ NULL }
    };
	
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value buf found list"));
	return 1;
    }
    if (xlat_c_string(tab, value->v.string, 0, &mode)) {
	config_error(locus, 0, _("unknown mode"));
	return 1;
    }
    return 0;
}

static struct xlat_tab syslog_facility_tab[] = {
    { "USER",    LOG_USER },   
    { "DAEMON",  LOG_DAEMON },
    { "AUTH",    LOG_AUTH },
    { "AUTHPRIV",LOG_AUTHPRIV },
    { "MAIL",    LOG_MAIL },
    { "CRON",    LOG_CRON },
    { "LOCAL0",  LOG_LOCAL0 },
    { "LOCAL1",  LOG_LOCAL1 },
    { "LOCAL2",  LOG_LOCAL2 },
    { "LOCAL3",  LOG_LOCAL3 },
    { "LOCAL4",  LOG_LOCAL4 },
    { "LOCAL5",  LOG_LOCAL5 },
    { "LOCAL6",  LOG_LOCAL6 },
    { "LOCAL7",  LOG_LOCAL7 },
    { NULL }
};

int
set_log_facility(enum cfg_callback_command cmd,
		 gd_locus_t *locus,
		 void *varptr,
		 config_value_t *value,
		 void *cb_data)
{
    const char *str;
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value buf found list"));
	return 1;
    }
    str = value->v.string;
    if (strncasecmp (str, "LOG_", 4) == 0)
	str += 4;
    if (xlat_c_string(syslog_facility_tab, str, XLAT_ICASE, &log_facility)) {
	config_error(locus, 0, _("unknown syslog facility"));
	return 1;
    }
    return 0;
}

int
set_handler_type(enum cfg_callback_command cmd,
		 gd_locus_t *locus,
		 void *varptr,
		 config_value_t *value,
		 void *cb_data)
{
    static struct xlat_tab tab[] = {
	{ "extern", handler_extern },
	{ NULL }
    };
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value buf found list"));
	return 1;
    }
    if (xlat_c_string(tab, value->v.string, XLAT_ICASE, &log_facility)) {
	config_error(locus, 0, _("unknown handler type"));
	return 1;
    }
    return 0;
}

int
set_handler(enum cfg_callback_command cmd,
	    gd_locus_t *locus,
	    void *varptr,
	    config_value_t *value,
	    void *cb_data)
{
    dictd_handler_t *han;
    void **pdata = cb_data;
    
    switch (cmd) {
    case callback_section_begin:
	han = xzalloc(sizeof(*han));
	han->type = handler_extern;
	if (value->type != TYPE_STRING) 
	    config_error(locus, 0, _("tag must be a string"));
	else if (value->v.string == NULL) 
	    config_error(locus, 0, _("missing tag"));
	else
	    han->ident = strdup(value->v.string);
	*pdata = han;
	break;
	
    case callback_section_end:
	if (!handler_list)
	    handler_list = dict_list_create();
	han = *pdata;
	dict_list_append(handler_list, han);
	*pdata = NULL;
	break;
	
    case callback_set_value:
	/* Shouldn't happen */
	abort();
    }
    return 0;
}

int
set_dictionary(enum cfg_callback_command cmd,
	       gd_locus_t *locus,
	       void *varptr,
	       config_value_t *value,
	       void *cb_data)
{
    dictd_dictionary_t *dict;
    void **pdata = cb_data;
    
    switch (cmd) {
    case callback_section_begin:
	dict = xzalloc(sizeof(*dict));
	if (value->type != TYPE_STRING) 
	    config_error(locus, 0, _("tag must be a string"));
	else if (value->v.string) 
	    dict->name = strdup(value->v.string);
	*pdata = dict;
	break;
	
    case callback_section_end:
	if (!dictionary_list)
	    dictionary_list = dict_list_create();
	dict = *pdata;
	dict_list_append(dictionary_list, dict);
	*pdata = NULL;
	break;
	
    case callback_set_value:
	/* Shouldn't happen */
	abort();
    }
    return 0;
}

static int
cmp_handler_ident(const void *item, const void *data)
{
    const dictd_handler_t *han = item;
    if (!han->ident)
	return 1;
    return strcmp(han->ident, (const char*)data);
}

int
set_dict_handler(enum cfg_callback_command cmd,
		 gd_locus_t *locus,
		 void *varptr,
		 config_value_t *value,
		 void *cb_data)
{
    dictd_handler_t *han;
    
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value buf found list"));
	return 1;
    }

    han = dict_list_locate(handler_list, (void*) value->v.string,
			   cmp_handler_ident);
    if (!han) {
	config_error(locus, 0, _("%s: handler not declared"), value->v.string);
	return 1;
    }
    *(dictd_handler_t**)varptr = han;
    return 0;
}

struct config_keyword kwd_handler[] = {
    { "type", cfg_string, NULL, offsetof(dictd_handler_t, type),
      set_handler_type },
    { "command", cfg_string, NULL, offsetof(dictd_handler_t, command) },
    { NULL }
};

struct config_keyword kwd_dictionary[] = {
    { "name", cfg_string, NULL, offsetof(dictd_dictionary_t, name) },
    { "description", cfg_string, NULL, offsetof(dictd_dictionary_t, descr) },
    { "handler", cfg_string, NULL, offsetof(dictd_dictionary_t, handler),
      set_dict_handler },
    { NULL }
};

struct config_keyword keywords[] = {
    { "user", cfg_string, NULL, 0, set_user  },
    { "group", cfg_string, NULL, 0, set_supp_group },
    { "mode", cfg_string, NULL, 0, set_mode },
    { "server-info", cfg_string, &server_info,  },
    { "max-children", cfg_uint, &max_children, 0 },
    { "log-tag", cfg_string, &log_tag, 0 },
    { "listen", cfg_sockaddr|CFG_LIST, &listen_addr,  },
    { "log-facility", cfg_string, NULL, 0, set_log_facility },
    { "dictionary", cfg_section, NULL, 0, set_dictionary, NULL,
      kwd_dictionary },
    { "handler", cfg_section, NULL, 0, set_handler, NULL,
      kwd_handler },
    { NULL }
};
	    

int
main(int argc, char **argv)
{
    set_program_name(argv[0]);
    log_tag = program_name;
    config_lex_trace(0);
    get_options(argc, argv);
    config_set_keywords(keywords);
    config_parse(config_file);
}
