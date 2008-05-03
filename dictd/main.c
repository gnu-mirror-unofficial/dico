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
#include <xgethostname.h>

int foreground;     /* Run in foreground mode */
int single_process; /* Single process mode */
int log_to_stderr;  /* Log to stderr */
/* Location of the default configuration file */
char *config_file = SYSCONFIG "/dictd.conf" ; 
/* Location of the pidfile */
char *pidfile_name = "/var/run/dictd.pid";

/* Operation mode */
int mode = MODE_DAEMON;

/* Maximum number of children in allowed in daemon mode. */
unsigned int max_children;
/* Wait this number of seconds for all subprocesses to terminate. */
unsigned int shutdown_timeout = 5;
/* Inactivity timeout */
unsigned int inactivity_timeout = 0;

/* Syslog parameters: */ 
const char *log_tag; 
int log_facility = LOG_FACILITY;
int log_print_severity;

/* Server information (for SHOW INFO command) */
const char *server_info;

/* This host name */
char *hostname;

/* Text of initial banner. By default == program_version */
char *initial_banner_text;

/* Alternative help text. Default - see dictd_help (commands.c) */
char *help_text;

/* List of sockets to listen on for the requests */
dict_list_t /* of struct sockaddr */ listen_addr;

/* Run as this user */
uid_t user_id;
gid_t group_id;
/* Retain these supplementary groups when switching to the user privileges. */
dict_list_t /* of gid_t */ group_list;

/* List of configured dictionary handlers */
dict_list_t /* of dictd_handler_t */ handler_list;

/* List of configured dictionaries */
dict_list_t /* of dictd_dictionary_t */ dictionary_list;


/* Configuration */
int
set_user(enum cfg_callback_command cmd,
	 gd_locus_t *locus,
	 void *varptr,
	 config_value_t *value,
	 void *cb_data)
{
    struct passwd *pw;
    
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value buf found list"));
	return 1;
    }
    
    pw = getpwnam(value->v.string);
    if (!pw) {
	config_error(locus, 0, _("%s: no such user"), value->v.string);
	return 1;
    }
    user_id = pw->pw_uid;
    group_id = pw->pw_gid;
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
	config_error(locus, 0, _("invalid use of a block statement"));
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
	config_error(locus, 0, _("invalid use of a block statement"));
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

int enable_capability(enum cfg_callback_command cmd,
		      gd_locus_t *locus,
		      void *varptr,
		      config_value_t *value,
		      void *cb_data);

int
set_capability(void *item, void *data)
{
    return enable_capability(callback_set_value,
			     (gd_locus_t *) data,
			     NULL,
			     (config_value_t *) item,
			     NULL);
}

int
enable_capability(enum cfg_callback_command cmd,
		  gd_locus_t *locus,
		  void *varptr,
		  config_value_t *value,
		  void *cb_data)
{
    if (value->type == TYPE_LIST)
	dict_list_iterate(value->v.list, set_capability, locus);
    else if (dictd_capa_add(value->v.string)) 
	config_error(locus, 0, _("unknown capability: %s"), value->v.string);
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
    { "info", cfg_string, NULL, offsetof(dictd_dictionary_t, info) },
    { "handler", cfg_string, NULL, offsetof(dictd_dictionary_t, handler),
      set_dict_handler },
    { NULL }
};


struct user_db_conf {
    char *url;
    char *get_pw;
    char *get_groups;
};

struct user_db_conf user_db_cfg;

struct config_keyword kwd_user_db[] = {
    { "get-password", cfg_string, NULL, offsetof(struct user_db_conf, get_pw) },
    { "get-groups", cfg_string, NULL, offsetof(struct user_db_conf, get_groups) },
    { NULL }
};

int
user_db_config(enum cfg_callback_command cmd,
	       gd_locus_t *locus,
	       void *varptr,
	       config_value_t *value,
	       void *cb_data)
{
    struct user_db_conf *cfg = varptr;
    void **pdata = cb_data;
    
    switch (cmd) {
    case callback_section_begin:
	if (value->type != TYPE_STRING) 
	    config_error(locus, 0, _("URL must be a string"));
	else if (!value->v.string)
	    config_error(locus, 0, _("empty URL"));
	else
	    cfg->url = strdup(value->v.string);
	*pdata = cfg;
	break;
	
    case callback_section_end:
	/* FIXME:
	   init_user_db(cfg->url, cfg->get_pw, cfg->get_groups, locus);
	*/
	break;
	
    case callback_set_value:
	if (value->type != TYPE_STRING) 
	    config_error(locus, 0, _("URL must be a string"));
	else if (!value->v.string)
	    config_error(locus, 0, _("empty URL"));
	/* FIXME:
	   init_user_db(value->v.string, NULL, NULL, locus);
	*/
    }
    return 0;
}

struct config_keyword keywords[] = {
    { "user", cfg_string, NULL, 0, set_user  },
    { "group", cfg_string, NULL, 0, set_supp_group },
    { "mode", cfg_string, NULL, 0, set_mode },
    { "server-info", cfg_string, &server_info,  },
    { "max-children", cfg_uint, &max_children, 0 },
    { "log-tag", cfg_string, &log_tag, 0 },
    { "log-facility", cfg_string, NULL, 0, set_log_facility },
    { "log-print-severity", cfg_bool, &log_print_severity, 0 },
    { "pidfile", cfg_string, &pidfile_name, },
    { "shutdown-timeout", cfg_uint, &shutdown_timeout },
    { "inactivity-timeout", cfg_uint, &inactivity_timeout },
    { "listen", cfg_sockaddr|CFG_LIST, &listen_addr,  },
    { "initial-banner-text", cfg_string, &initial_banner_text },
    { "help-text", cfg_string, &help_text },
    { "hostname", cfg_string, &hostname },
    { "capability", cfg_string|CFG_LIST, NULL, 0, enable_capability },
    { "dictionary", cfg_section, NULL, 0, set_dictionary, NULL,
      kwd_dictionary },
    { "handler", cfg_section, NULL, 0, set_handler, NULL,
      kwd_handler },
    { "user-db", cfg_section, &user_db_cfg, 0, user_db_config, NULL,
      kwd_user_db },
    { NULL }
};
	    

static int
cmp_dict_name(const void *item, const void *data)
{
    const dictd_dictionary_t *db = item;
    if (!db->name)
	return 1;
    return strcmp(db->name, (const char*)data);
}    

dictd_dictionary_t *
find_dictionary(const char *name)
{
    return dict_list_locate(dictionary_list, (void*) name,
			    cmp_dict_name);
}


void
syslog_log_printer(int lvl, int exitcode, int errcode,
                   const char *fmt, va_list ap)
{
    char *s;
    int prio = LOG_INFO;
    static struct {
        char *prefix;
        int priority;
    } loglevels[] = {
        { "Debug",  LOG_DEBUG },
        { "Info",   LOG_INFO },
        { "Notice", LOG_NOTICE },
        { "Warning",LOG_WARNING },
        { "Error",  LOG_ERR },
        { "CRIT",   LOG_CRIT },
        { "ALERT",  LOG_ALERT },
        { "EMERG",  LOG_EMERG },
    };
    char buf[512];
    int level = 0;

    if (lvl & L_CONS)
        _stderr_log_printer(lvl, exitcode, errcode, fmt, ap);

    s    = loglevels[lvl & L_MASK].prefix;
    prio = loglevels[lvl & L_MASK].priority;

    if (log_print_severity)
	level += snprintf(buf + level, sizeof(buf) - level, "%s: ", s);
    level += vsnprintf(buf + level, sizeof(buf) - level, fmt, ap);
    if (errcode)
        level += snprintf(buf + level, sizeof(buf) - level, ": %s",
                          strerror(errcode));
    syslog(prio, "%s", buf);
}


static void
dictd_xversion(stream_t str, int argc, char **argv)
{
    stream_writez(str, "110 ");
    stream_writez(str, (char*)program_version);
    stream_write(str, "\r\n", 2);
}
    
static void
register_xversion()
{
    static struct dictd_command cmd = 
	{ "XVERSION", 1, NULL, "show implementation and version info",
	  dictd_xversion };
    dictd_capa_register("xversion", &cmd, NULL, NULL);
}


int
main(int argc, char **argv)
{
    set_program_name(argv[0]);
    log_tag = program_name;
    hostname = xgethostname();
    dictd_init_command_tab();
    register_xversion();
    config_lex_trace(0);
    get_options(argc, argv);
    config_set_keywords(keywords);
    if (config_parse(config_file))
	exit(1);
    
    if (!log_to_stderr) {
	openlog(log_tag, LOG_PID, LOG_FACILITY);
	set_log_printer(syslog_log_printer);
    }

    switch (mode) {
    case MODE_DAEMON:
	return dictd_server(argc, argv);

    case MODE_INETD:
	return dictd_inetd();
    }

	
    return 0;
}
