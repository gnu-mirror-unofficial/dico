/* This file is part of Dico.
   Copyright (C) 1998-2000, 2008 Sergey Poznyakoff

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
#include <pwd.h>
#include <grp.h>
#include <xgethostname.h>
#include <xgetdomainname.h>

int foreground;     /* Run in foreground mode */
int single_process; /* Single process mode */
/* Location of the default configuration file */
char *config_file = SYSCONFIG "/dictd.conf" ;
int config_lint_option; /* Check configuration file syntax and exit. */
/* Location of the pidfile */
char *pidfile_name = "/var/run/dictd.pid";

/* Operation mode */
int mode = MODE_DAEMON;

/* Debug verbosity level */
int debug_level;
char *debug_level_str;

/* Maximum number of children in allowed in daemon mode. */
unsigned int max_children;
/* Wait this number of seconds for all subprocesses to terminate. */
unsigned int shutdown_timeout = 5;
/* Inactivity timeout */
unsigned int inactivity_timeout = 0;

/* Syslog parameters: */ 
int log_to_stderr;  /* Log to stderr */
const char *log_tag; 
int log_facility = LOG_FACILITY;
int log_print_severity;
int transcript;

/* Server information (for SHOW INFO command) */
const char *server_info;

enum ssi_mode show_sys_info = ssi_never;
dico_list_t /* of char * */ ssi_group_list;

/* This host name */
char *hostname;

/* Text of initial banner. By default == program_version */
char *initial_banner_text;

/* Alternative help text. Default - see dictd_help (commands.c) */
char *help_text;

/* List of sockets to listen on for the requests */
dico_list_t /* of struct sockaddr */ listen_addr;

/* User database for AUTH */
dictd_user_db_t user_db;

/* Run as this user */
uid_t user_id;
gid_t group_id;
/* Retain these supplementary groups when switching to the user privileges. */
dico_list_t /* of gid_t */ group_list;

/* List of directories to search for handler modules. */
dico_list_t /* of char * */ module_load_path;

/* List of configured database handlers */
dico_list_t /* of dictd_handler_t */ handler_list;

/* List of configured dictionaries */
dico_list_t /* of dictd_database_t */ database_list;

int require_auth; /* Require authentication */

/* From CLIENT command: */
char *client_id;

/* In authenticated state: */
char *user_name;  /* User name */
dico_list_t /* of char * */ user_groups; /* List of groups he is member of */

/* Information from AUTH check: */
int identity_check;     /* Enable identity check */
char *identity_name;    /* Name received from AUTH query. */
char *ident_keyfile;    /* Keyfile for decrypting AUTH replies. */

/* Provide timing information */
int timing_option;


/* Configuration */
int
set_user(enum cfg_callback_command cmd,
	 dictd_locus_t *locus,
	 void *varptr,
	 config_value_t *value,
	 void *cb_data)
{
    struct passwd *pw;
    
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value but found list"));
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
			  dictd_locus_t *locus,
			  void *varptr,
			  config_value_t *value,
			  void *cb_data);

static int
set_supp_group_iter(void *item, void *data)
{
    return set_supp_group(callback_set_value,
			  (dictd_locus_t *)data,
			  NULL,
			  (config_value_t *)item,
			  NULL);
}
	
static int
set_supp_group(enum cfg_callback_command cmd,
	       dictd_locus_t *locus,
	       void *varptr,
	       config_value_t *value,
	       void *cb_data)
{
    if (!group_list)
	group_list = xdico_list_create();
    
    if (value->type == TYPE_LIST)
	dico_list_iterate(value->v.list, set_supp_group_iter, locus);
    else {
	struct group *group = getgrnam(value->v.string);
	if (group)
	    xdico_list_append(group_list, (void*)group->gr_gid);
	else {
	    config_error(locus, 0, _("%s: unknown group"), value->v.string);
	    return 1;
	}
    }
    return 0;
}

int
set_mode(enum cfg_callback_command cmd,
	 dictd_locus_t *locus,
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
	config_error(locus, 0, _("expected scalar value but found list"));
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
		 dictd_locus_t *locus,
		 void *varptr,
		 config_value_t *value,
		 void *cb_data)
{
    const char *str;
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value but found list"));
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
		 dictd_locus_t *locus,
		 void *varptr,
		 config_value_t *value,
		 void *cb_data)
{
    static struct xlat_tab tab[] = {
	{ "loadable", handler_loadable },
	{ NULL }
    };
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value but found list"));
	return 1;
    }
    if (xlat_c_string(tab, value->v.string, XLAT_ICASE, varptr)) {
	config_error(locus, 0, _("unknown handler type"));
	return 1;
    }
    return 0;
}

int
set_handler(enum cfg_callback_command cmd,
	    dictd_locus_t *locus,
	    void *varptr,
	    config_value_t *value,
	    void *cb_data)
{
    dictd_handler_t *han;
    void **pdata = cb_data;
    
    switch (cmd) {
    case callback_section_begin:
	han = xzalloc(sizeof(*han));
	han->type = handler_loadable;
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
	    handler_list = xdico_list_create();
	han = *pdata;
	xdico_list_append(handler_list, han);
	*pdata = NULL;
	break;
	
    case callback_set_value:
	config_error(locus, 0, _("invalid use of block statement"));
    }
    return 0;
}

int
set_database(enum cfg_callback_command cmd,
	     dictd_locus_t *locus,
	     void *varptr,
	     config_value_t *value,
	     void *cb_data)
{
    dictd_database_t *dict;
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
	if (!database_list)
	    database_list = xdico_list_create();
	dict = *pdata;
	xdico_list_append(database_list, dict);
	*pdata = NULL;
	break;
	
    case callback_set_value:
	config_error(locus, 0, _("invalid use of block statement"));
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
		 dictd_locus_t *locus,
		 void *varptr,
		 config_value_t *value,
		 void *cb_data)
{
    dictd_handler_t *han;
    dictd_database_t *db = varptr;
    int rc;
    
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value but found list"));
	return 1;
    }

    db->command = (char *) value->v.string;
    if ((rc = dico_argcv_get(value->v.string, NULL, NULL,
			     &db->argc, &db->argv))) {
	config_error(locus, rc, _("cannot parse command line `%s'"),
		     value->v.string);
	dictd_database_free(db); 
	return 1;
    } 

    han = dico_list_locate(handler_list, db->argv[0], cmp_handler_ident);
    if (!han) {
	config_error(locus, 0, _("%s: handler not declared"), db->argv[0]);
	/* FIXME: Free memory */
	return 1;
    }
    db->handler = han;
    
    return 0;
}

int enable_capability(enum cfg_callback_command cmd,
		      dictd_locus_t *locus,
		      void *varptr,
		      config_value_t *value,
		      void *cb_data);

int
set_capability(void *item, void *data)
{
    return enable_capability(callback_set_value,
			     (dictd_locus_t *) data,
			     NULL,
			     (config_value_t *) item,
			     NULL);
}

int
enable_capability(enum cfg_callback_command cmd,
		  dictd_locus_t *locus,
		  void *varptr,
		  config_value_t *value,
		  void *cb_data)
{
    if (value->type == TYPE_LIST)
	dico_list_iterate(value->v.list, set_capability, locus);
    else if (dictd_capa_add(value->v.string)) 
	config_error(locus, 0, _("unknown capability: %s"), value->v.string);
    return 0;
}

int
set_defstrat(enum cfg_callback_command cmd,
	     dictd_locus_t *locus,
	     void *varptr,
	     config_value_t *value,
	     void *cb_data)
{
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value but found list"));
	return 1;
    }
    if (dico_set_default_strategy(value->v.string)) {
	config_error(locus, 0, _("unknown strategy"));
	return 1;
    }
    return 0;
}

struct config_keyword kwd_handler[] = {
    { "type", N_("type"), N_("Type of this handler"),
      cfg_string, NULL, offsetof(dictd_handler_t, type),
      set_handler_type },
    { "command", N_("arg"), N_("Command line."),
      cfg_string, NULL, offsetof(dictd_handler_t, command) },
    { NULL }
};

struct config_keyword kwd_database[] = {
    { "name", N_("word"), N_("Dictionary name (a single word)."),
      cfg_string, NULL, offsetof(dictd_database_t, name) },
    { "description", N_("arg"),
      N_("Short description, to be shown in reply to SHOW DB command."),
      cfg_string, NULL, offsetof(dictd_database_t, descr) },
    { "info", N_("arg"),
      N_("Full description of the database, to be shown in reply to "
	 "SHOW INFO command."),
      cfg_string, NULL, offsetof(dictd_database_t, info) },
    { "handler", N_("name"), N_("Name of the handler for this database."),
      cfg_string, NULL, 0, set_dict_handler },
    { "require-auth", N_("arg"),
      N_("Require authentication for access to this database."),
      cfg_bool, NULL, offsetof(dictd_database_t, require_auth) },
    { "groups", N_("arg"),
      N_("The database is visible only for users from these groups"),
      cfg_string|CFG_LIST, NULL, offsetof(dictd_database_t, groups) },
    { "content-type", N_("arg"), N_("Content type for MIME replies."),
      cfg_string, NULL, offsetof(dictd_database_t, content_type) },
    /* FIXME: Install a callback to verify if arg is acceptable. */
    { "content-transfer-encoding", N_("arg"),
      N_("Content transfer encoding for MIME replies."),
      cfg_string, NULL, offsetof(dictd_database_t, content_transfer_encoding) },
    { NULL }
};


struct user_db_conf {
    char *url;
    char *get_pw;
    char *get_groups;
};

struct user_db_conf user_db_cfg;

struct config_keyword kwd_user_db[] = {
    { "get-password", N_("arg"), N_("Password file or query."),
      cfg_string, NULL, offsetof(struct user_db_conf, get_pw) },
    { "get-groups", N_("arg"),
      N_("File containing user group information or a query to retrieve it."),
      cfg_string, NULL, offsetof(struct user_db_conf, get_groups) },
    { NULL }
};

int
user_db_config(enum cfg_callback_command cmd,
	       dictd_locus_t *locus,
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
	udb_create(&user_db, cfg->url, cfg->get_pw, cfg->get_groups, locus);
	break;
	
    case callback_set_value:
	if (value->type != TYPE_STRING) 
	    config_error(locus, 0, _("URL must be a string"));
	else if (!value->v.string)
	    config_error(locus, 0, _("empty URL"));
	udb_create(&user_db, value->v.string, NULL, NULL, locus);
    }
    return 0;
}

int
set_show_sys_info(enum cfg_callback_command cmd,
		  dictd_locus_t *locus,
		  void *varptr,
		  config_value_t *value,
		  void *cb_data)
{
    static struct xlat_tab tab[] = {
	{ "never", ssi_never },
	{ "always", ssi_always },
	{ "auth", ssi_auth },
	{ NULL }
    };
    
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value but found list"));
	return 1;
    }
    if (xlat_c_string(tab, value->v.string, 0, varptr)) {
	config_error(locus, 0, _("unknown value"));
	return 1;
    }
    return 0;
}

struct config_keyword keywords[] = {
    { "user", N_("name"), N_("Run with these user privileges."),
      cfg_string, NULL, 0, set_user  },
    { "group", N_("name"),
      N_("Supplementary group to retain with the user privileges."),
      cfg_string, NULL, 0, set_supp_group },
    { "mode", N_("arg: [daemon|inetd]"), N_("Operation mode."),
      cfg_string, NULL, 0, set_mode },
    { "server-info", N_("text"),
      N_("Server description to be shown in reply to SHOW SERVER command."),
      cfg_string, &server_info,  },
    { "show-sys-info", N_("arg: {always|never|auth}"),
      N_("Show system information in reply to SHOW SERVER command:\n"
	 "  always      - alwaysshow;\n"
	 "  never       - never show;\n"
	 "  auth        - show for authorized users"),
      cfg_uint, &show_sys_info, 0, set_show_sys_info },
    { "sys-info-groups", N_("arg"),
      N_("With `show-sys-info auth', show system information only if "
	 "the user is member of one of these groups"),
      cfg_string|CFG_LIST, &ssi_group_list },
    { "identity-check", N_("arg"),
      N_("Enable identification check using AUTH protocol (RFC 1413)"),
      cfg_bool, &identity_check },
    { "ident-keyfile", N_("name"),
      N_("Name of the file containing the keys for decrypting AUTH replies."),
      cfg_string, &ident_keyfile },
    { "max-children", N_("arg"),
      N_("Maximum number of children running simultaneously."),
      cfg_uint, &max_children, 0 },
    { "log-tag", N_("arg"),  N_("Tag syslog diagnostics with this tag."),
      cfg_string, &log_tag, 0 },
    { "log-facility", N_("arg"),
      N_("Set syslog facility. Arg is one of the following: user, daemon, "
	 "auth, authpriv, mail, cron, local0 through local7 "
	 "(case-insensitive), or a facility number."),
      cfg_string, NULL, 0, set_log_facility },
    { "log-print-severity", N_("arg"),
      N_("Prefix diagnostics messages with their severity."),
      cfg_bool, &log_print_severity, 0 },
    { "access-log-format", N_("fmt"),
      N_("Set format string for access log file."),
      cfg_string, &access_log_format, },
    { "access-log-file", N_("name"),
      N_("Set access log file name."),
      cfg_string, &access_log_file },
    { "transcript", N_("arg"), N_("Log session transcript."),
      cfg_bool, &transcript },
    { "pidfile", N_("name"),
      N_("Store PID of the master process in this file."),
      cfg_string, &pidfile_name, },
    { "shutdown-timeout", N_("seconds"),
      N_("Wait this number of seconds for all children to terminate."),
      cfg_uint, &shutdown_timeout },
    { "inactivity-timeout", N_("seconds"),
      N_("Set inactivity timeouit."),
      cfg_uint, &inactivity_timeout },
    { "listen", N_("addr"), N_("Listen on these addresses."),
      cfg_sockaddr|CFG_LIST, &listen_addr,  },
    { "initial-banner-text", N_("text"),
      N_("Display this text in the initial 220 banner"),
      cfg_string, &initial_banner_text },
    { "help-text", N_("text"),
      N_("Display this text in reply to the HELP command. If text "
	 "begins with a +, usual command summary is displayed before it."),
      cfg_string, &help_text },
    { "hostname", N_("name"), N_("Override the host name."),
      cfg_string, &hostname },
    { "capability", N_("arg"), N_("Request additional capabilities."),
      cfg_string|CFG_LIST, NULL, 0, enable_capability },
    { "module-load-path", N_("path"),
      N_("List of directories searched for handler modules."),
      cfg_string|CFG_LIST, &module_load_path },
    { "default-strategy", N_("name"),
      N_("Set the name of the default matching strategy."),
      cfg_string, NULL, 0, set_defstrat },
    { "timing", N_("arg"),
      N_("Provide timing information after successful completion of an "
	 "operation."),
      cfg_bool, &timing_option },
    { "require-auth", N_("arg"),
      N_("Require authentication for access to databases."),
      cfg_bool, &require_auth },
    { "database", NULL, N_("Define a dictionary database."),
      cfg_section, NULL, 0, set_database, NULL,
      kwd_database },
    { "handler", N_("name: string"), N_("Define a database handler."),
      cfg_section, NULL, 0, set_handler, NULL,
      kwd_handler },
    { "user-db", N_("url: string"),
      N_("Define user database for authentication."),
      cfg_section, &user_db_cfg, 0, user_db_config, NULL,
      kwd_user_db },
    { NULL }
};

void
config_help()
{
    static char docstring[] =
	N_("Configuration file structure for dictd.\n"
	   "For more information, use `info dico'.");
    format_docstring(stdout, docstring, 0);
    format_statement_array(stdout, keywords, 1, 0);
}


static int
cmp_group_name(const void *item, const void *data)
{
    return strcmp((char*)item, (char*)data);
}

int
show_sys_info_p()
{
    switch (show_sys_info) {
    case ssi_always:
	return 1;
    case ssi_never:
	return 0;
    case ssi_auth:
	return ssi_group_list ?
	        dico_list_intersect_p(ssi_group_list, user_groups,
				      cmp_group_name) :
	        (user_name != NULL);
    }
}

int
database_visible_p(const dictd_database_t *db)
{
    return ((!db->groups || dico_list_intersect_p(db->groups, user_groups,
						  cmp_group_name))
	    && (user_name || !(db->require_auth || require_auth)));
}

static int
cmp_database_name(const void *item, const void *data)
{
    const dictd_database_t *db = item;
    int rc;
    
    if (!db->name)
	return 1;
    rc = strcmp(db->name, (const char*)data);
    if (rc == 0 && !database_visible_p(db))
	rc = 1;
    return rc;
}    

dictd_database_t *
find_database(const char *name)
{
    return dico_list_locate(database_list, (void*) name,
			    cmp_database_name);
}

static int
_count_databases(void *item, void *data)
{
    const dictd_database_t *db = item;
    size_t *pcount = data;
    if (database_visible_p(db))
	++*pcount;
    return 0;
}

size_t
database_count()
{
    size_t count = 0;
    dico_list_iterate(database_list, _count_databases, &count);
    return count;
}

int
database_iterate(dico_list_iterator_t fun, void *data)
{
    dico_iterator_t itr = xdico_iterator_create(database_list);
    dictd_database_t *db;
    int rc = 0;
    
    for (db = dico_iterator_first(itr); rc == 0 && db;
	 db = dico_iterator_next(itr)) {
	if (database_visible_p(db)) 
	    rc = fun(db, data);
    }
    dico_iterator_destroy(&itr);
    return rc;
}

/* Remove all dictionaries that depend on the given handler */
void
database_remove_dependent(dictd_handler_t *handler)
{
    dico_iterator_t itr = xdico_iterator_create(database_list);
    dictd_database_t *dp;

    for (dp = dico_iterator_first(itr); dp; dp = dico_iterator_next(itr)) {
	if (dp->handler == handler) {
	    dico_log(L_NOTICE, 0, _("removing database %s"), dp->name);
	    dico_iterator_remove_current(itr);
	    dictd_database_free(dp); 
	}
    }
    dico_iterator_destroy(&itr);
}

void
dictd_database_free(dictd_database_t *dp)
{
    dico_argcv_free(dp->argc, dp->argv);
    free(dp);
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
        _dico_stderr_log_printer(lvl, exitcode, errcode, fmt, ap);

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
dictd_xversion(dico_stream_t str, int argc, char **argv)
{
    stream_writez(str, "110 ");
    stream_writez(str, (char*)program_version);
    dico_stream_write(str, "\r\n", 2);
}
    
static void
register_xversion()
{
    static struct dictd_command cmd = 
	{ "XVERSION", 1, NULL, "show implementation and version info",
	  dictd_xversion };
    dictd_capa_register("xversion", &cmd, NULL, NULL);
}


char *
get_full_hostname()
{
    struct hostent *hp;
    char *hostpart = xgethostname();
    char *ret;
	
    hp = gethostbyname(hostpart);
    if (hp) 
	ret = xstrdup(hp->h_name);
    else {
	char *domainpart = xgetdomainname();

	if (domainpart && domainpart[0] && strcmp(domainpart, "(none)")) {
	    ret = xmalloc(strlen(hostpart) + 1
			       + strlen(domainpart) + 1);
	    strcpy(ret, hostpart);
	    strcat(ret, ".");
	    strcat(ret, domainpart);
	    free(hostpart);
	} else
	    ret = hostpart;
	free(domainpart);
    }
    return ret;
}

void
dictd_log_setup()
{
    if (!log_to_stderr) {
	openlog(log_tag, LOG_PID, log_facility);
	dico_set_log_printer(syslog_log_printer);
    }
}    


int
main(int argc, char **argv)
{
    dico_set_program_name(argv[0]);
    log_tag = dico_program_name;
    hostname = get_full_hostname();
    dictd_init_command_tab();
    dictd_init_strategies();
    udb_init();
    register_auth();
    register_mime();
    register_xversion();
    register_lev();
    register_regex();
    include_path_setup();
    config_lex_trace(0);
    dico_argcv_quoting_style = dico_argcv_quoting_hex;
    get_options(argc, argv);

    if (mode == MODE_PREPROC)
	return preprocess_config(preprocessor);

    config_set_keywords(keywords);
    if (config_parse(config_file))
	exit(1);
    if (dictd_capa_flush())
	exit(1);
    compile_access_log();
    if (config_lint_option)
	exit(0);
    
    dictd_log_setup();
    
    dictd_loader_init();

    begin_timing("server");
    dictd_server_init();
    switch (mode) {
    case MODE_DAEMON:
	dictd_server(argc, argv);

    case MODE_INETD:
	return dictd_inetd();

    }
	
    return 0;
}
