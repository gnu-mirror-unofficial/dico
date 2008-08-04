/* This file is part of GNU Dico.
   Copyright (C) 1998-2000, 2008 Sergey Poznyakoff

   GNU Dico is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Dico is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>. */

#include <dicod.h>
#include <pwd.h>
#include <grp.h>

int foreground;     /* Run in foreground mode */
int single_process; /* Single process mode */
/* Location of the default configuration file */
char *config_file = SYSCONFIG "/dicod.conf" ;
int config_lint_option; /* Check configuration file syntax and exit. */
/* Location of the pidfile */
char *pidfile_name = LOCALSTATEDIR "/run/dicod.pid";

/* Operation mode */
int mode = MODE_DAEMON;

/* Debug verbosity level */
int debug_level;
char *debug_level_str;

/* Maximum number of children in allowed in daemon mode. */
unsigned int max_children = 64;
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

dicod_acl_t show_sys_info;

/* This host name */
char *hostname;

/* Text of initial banner. By default == program_version */
char *initial_banner_text;

/* Alternative help text. Default - see dicod_help (commands.c) */
char *help_text;

/* List of sockets to listen on for the requests */
dico_list_t /* of struct sockaddr */ listen_addr;

/* User database for AUTH */
dicod_user_db_t user_db;

/* Run as this user */
uid_t user_id;
gid_t group_id;
/* Retain these supplementary groups when switching to the user privileges. */
dico_list_t /* of gid_t */ group_list;

/* List of directories to search for handler modules. */
dico_list_t /* of char * */ module_load_path;

/* List of configured database module instances */
dico_list_t /* of dicod_module_instance_t */ modinst_list;

/* List of configured dictionaries */
dico_list_t /* of dicod_database_t */ database_list;

/* Global Dictionary ACL */
dicod_acl_t global_acl;

/* ACL for incoming connections */
dicod_acl_t connect_acl;

/* From CLIENT command: */
char *client_id;

/* In authenticated state: */
char *user_name;  /* User name */
dico_list_t /* of char * */ user_groups; /* List of groups he is member of */

/* Information from AUTH check: */
int identity_check;     /* Enable identity check */
char *identity_name;    /* Name received from AUTH query. */
char *ident_keyfile;    /* Keyfile for decrypting AUTH replies. */
long ident_timeout = 3; /* Timeout for AUTH I/O */

/* Provide timing information */
int timing_option;


/* Configuration */

int
allow_cb(enum cfg_callback_command cmd,
	 dicod_locus_t *locus,
	 void *varptr,
	 config_value_t *value,
	 void *cb_data)
{
    dicod_acl_t acl = varptr;

    if (cmd != callback_set_value) {
	config_error(locus, 0, _("Unexpected block statement"));
	return 1;
    }
    parse_acl_line(locus, 1, acl, value);
    return 0;
}

int
deny_cb(enum cfg_callback_command cmd,
	dicod_locus_t *locus,
	void *varptr,
	config_value_t *value,
	void *cb_data)
{
    dicod_acl_t acl = varptr;
    if (cmd != callback_set_value) {
	config_error(locus, 0, _("Unexpected block statement"));
	return 1;
    }
    parse_acl_line(locus, 0, acl, value);
    return 0;
}

int
acl_cb(enum cfg_callback_command cmd,
       dicod_locus_t *locus,
       void *varptr,
       config_value_t *value,
       void *cb_data)
{
    void **pdata = cb_data;
    dicod_acl_t acl;
    
    switch (cmd) {
    case callback_section_begin:
	if (value->type != TYPE_STRING) 
	    config_error(locus, 0, _("ACL name must be a string"));
	else if (!value->v.string)
	    config_error(locus, 0, _("missing ACL name"));
	else {
	    dicod_locus_t defn_loc;
	    acl = dicod_acl_create(value->v.string, locus);
	    if (dicod_acl_install(acl, &defn_loc)) {
		config_error(locus, 0,
			     _("redefinition of ACL %s"),
			     value->v.string);
		config_error(&defn_loc, 0,
			     _("location of the previous definition"));
		return 1;
	    }
	    *pdata = acl;
	}
	break;

    case callback_section_end:
    case callback_set_value:
	break;
    }	
    return 0;
}

struct config_keyword kwd_acl[] = {
    { "allow", N_("[all|authenticated|group <grp: list>] [from <addr: list>]"),
      N_("Allow access"),
      cfg_string, NULL, 0,
      allow_cb },
    { "deny", N_("[all|authenticated|group <grp: list>] [from <addr: list>]"),
      N_("Deny access"),
      cfg_string, NULL, 0,
      deny_cb },
    { NULL }
};

int
apply_acl_cb(enum cfg_callback_command cmd,
	     dicod_locus_t *locus,
	     void *varptr,
	     config_value_t *value,
	     void *cb_data)
{
    dicod_acl_t *pacl = varptr;

    if (cmd != callback_set_value) {
	config_error(locus, 0, _("Unexpected block statement"));
	return 1;
    }
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value"));
	return 1;
    }
    if ((*pacl =  dicod_acl_lookup(value->v.string)) == NULL) {
	config_error(locus, 0, _("no such ACL: `%s'"),
		     value->v.string);
	return 1;
    }
    return 0;
}


int
set_user(enum cfg_callback_command cmd,
	 dicod_locus_t *locus,
	 void *varptr,
	 config_value_t *value,
	 void *cb_data)
{
    struct passwd *pw;
    
    if (cmd != callback_set_value) {
	config_error(locus, 0, _("Unexpected block statement"));
	return 1;
    }

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
			  dicod_locus_t *locus,
			  void *varptr,
			  config_value_t *value,
			  void *cb_data);

static int
set_supp_group_iter(void *item, void *data)
{
    return set_supp_group(callback_set_value,
			  (dicod_locus_t *)data,
			  NULL,
			  (config_value_t *)item,
			  NULL);
}
	
static int
set_supp_group(enum cfg_callback_command cmd,
	       dicod_locus_t *locus,
	       void *varptr,
	       config_value_t *value,
	       void *cb_data)
{
    if (cmd != callback_set_value) {
	config_error(locus, 0, _("Unexpected block statement"));
	return 1;
    }

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
	 dicod_locus_t *locus,
	 void *varptr,
	 config_value_t *value,
	 void *cb_data)
{
    static struct xlat_tab tab[] = {
	{ "daemon", MODE_DAEMON },
	{ "inetd", MODE_INETD },
	{ NULL }
    };
	
    if (cmd != callback_set_value) {
	config_error(locus, 0, _("Unexpected block statement"));
	return 1;
    }

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
		 dicod_locus_t *locus,
		 void *varptr,
		 config_value_t *value,
		 void *cb_data)
{
    const char *str;

    if (cmd != callback_set_value) {
	config_error(locus, 0, _("Unexpected block statement"));
	return 1;
    }
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
load_module_cb(enum cfg_callback_command cmd,
	       dicod_locus_t *locus,
	       void *varptr,
	       config_value_t *value,
	       void *cb_data)
{
    dicod_module_instance_t *inst;
    void **pdata = cb_data;
    
    switch (cmd) {
    case callback_section_begin:
	inst = xzalloc(sizeof(*inst));
	if (value->type != TYPE_STRING) 
	    config_error(locus, 0, _("tag must be a string"));
	else if (value->v.string == NULL) 
	    config_error(locus, 0, _("missing tag"));
	else
	    inst->ident = strdup(value->v.string);
	*pdata = inst;
	break;
	
    case callback_section_end:
	if (!modinst_list)
	    modinst_list = xdico_list_create();
	inst = *pdata;
	xdico_list_append(modinst_list, inst);
	*pdata = NULL;
	break;
	
    case callback_set_value:
	config_error(locus, 0, _("invalid use of block statement"));
    }
    return 0;
}

int
set_database(enum cfg_callback_command cmd,
	     dicod_locus_t *locus,
	     void *varptr,
	     config_value_t *value,
	     void *cb_data)
{
    dicod_database_t *dict;
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
cmp_modinst_ident(const void *item, const void *data)
{
    const dicod_module_instance_t *inst = item;
    if (!inst->ident)
	return 1;
    return strcmp(inst->ident, (const char*)data);
}

int
set_dict_handler(enum cfg_callback_command cmd,
		 dicod_locus_t *locus,
		 void *varptr,
		 config_value_t *value,
		 void *cb_data)
{
    dicod_module_instance_t *inst;
    dicod_database_t *db = varptr;
    int rc;
    
    if (cmd != callback_set_value) {
	config_error(locus, 0, _("Unexpected block statement"));
	return 1;
    }

    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value but found list"));
	return 1;
    }

    db->command = (char *) value->v.string;
    if ((rc = dico_argcv_get(value->v.string, NULL, NULL,
			     &db->argc, &db->argv))) {
	config_error(locus, rc, _("cannot parse command line `%s'"),
		     value->v.string);
	dicod_database_free(db); 
	return 1;
    } 

    inst = dico_list_locate(modinst_list, db->argv[0], cmp_modinst_ident);
    if (!inst) {
	config_error(locus, 0, _("%s: handler not declared"), db->argv[0]);
	/* FIXME: Free memory */
	return 1;
    }
    db->instance = inst;
    
    return 0;
}

int
set_dict_encoding(enum cfg_callback_command cmd,
		  dicod_locus_t *locus,
		  void *varptr,
		  config_value_t *value,
		  void *cb_data)
{
    if (cmd != callback_set_value) {
	config_error(locus, 0, _("Unexpected block statement"));
	return 1;
    }
    if (value->type != TYPE_STRING) {
	config_error(locus, 0, _("expected scalar value but found list"));
	return 1;
    }
    if (strcmp (value->v.string, "quoted-printable") == 0
	|| strcmp (value->v.string, "base64") == 0) {
	*(const char**)varptr = value->v.string;
	return 0;
    }
    config_error(locus, 0, _("unknown encoding type: %s"), value->v.string);
    return 1;
}

int enable_capability(enum cfg_callback_command cmd,
		      dicod_locus_t *locus,
		      void *varptr,
		      config_value_t *value,
		      void *cb_data);

int
set_capability(void *item, void *data)
{
    return enable_capability(callback_set_value,
			     (dicod_locus_t *) data,
			     NULL,
			     (config_value_t *) item,
			     NULL);
}

int
enable_capability(enum cfg_callback_command cmd,
		  dicod_locus_t *locus,
		  void *varptr,
		  config_value_t *value,
		  void *cb_data)
{
    if (cmd != callback_set_value) {
	config_error(locus, 0, _("Unexpected block statement"));
	return 1;
    }
    if (value->type == TYPE_LIST)
	dico_list_iterate(value->v.list, set_capability, locus);
    else if (dicod_capa_add(value->v.string)) 
	config_error(locus, 0, _("unknown capability: %s"), value->v.string);
    return 0;
}

int
set_defstrat(enum cfg_callback_command cmd,
	     dicod_locus_t *locus,
	     void *varptr,
	     config_value_t *value,
	     void *cb_data)
{
    if (cmd != callback_set_value) {
	config_error(locus, 0, _("Unexpected block statement"));
	return 1;
    }
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

struct config_keyword kwd_load_module[] = {
    { "command", N_("arg"), N_("Command line."),
      cfg_string, NULL, offsetof(dicod_module_instance_t, command) },
    { NULL }
};

struct config_keyword kwd_database[] = {
    { "name", N_("word"), N_("Dictionary name (a single word)."),
      cfg_string, NULL, offsetof(dicod_database_t, name) },
    { "description", N_("arg"),
      N_("Short description, to be shown in reply to SHOW DB command."),
      cfg_string, NULL, offsetof(dicod_database_t, descr) },
    { "info", N_("arg"),
      N_("Full description of the database, to be shown in reply to "
	 "SHOW INFO command."),
      cfg_string, NULL, offsetof(dicod_database_t, info) },
    { "handler", N_("name"), N_("Name of the handler for this database."),
      cfg_string, NULL, 0, set_dict_handler },
    { "visibility-acl", N_("arg: acl"),
      N_("ACL controlling visibility of this database"),
      cfg_string, NULL, offsetof(dicod_database_t, acl), apply_acl_cb },
    { "content-type", N_("arg"), N_("Content type for MIME replies."),
      cfg_string, NULL, offsetof(dicod_database_t, content_type) },
    /* FIXME: Install a callback to verify if arg is acceptable. */
    { "content-transfer-encoding", N_("arg"),
      N_("Content transfer encoding for MIME replies."),
      cfg_string, NULL, offsetof(dicod_database_t, content_transfer_encoding),
      set_dict_encoding },
    { NULL }
};


struct user_db_conf {
    char *url;
    char *get_pw;
    char *get_groups;
};

struct user_db_conf user_db_cfg;

struct config_keyword kwd_user_db[] = {
    { "password-resource", N_("arg"), N_("Password file or query."),
      cfg_string, NULL, offsetof(struct user_db_conf, get_pw) },
    { "group-resource", N_("arg"),
      N_("File containing user group information or a query to retrieve it."),
      cfg_string, NULL, offsetof(struct user_db_conf, get_groups) },
    { NULL }
};

int
user_db_config(enum cfg_callback_command cmd,
	       dicod_locus_t *locus,
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
alias_cb(enum cfg_callback_command cmd,
	 dicod_locus_t *locus,
	 void *varptr,
	 config_value_t *value,
	 void *cb_data)
{
    char **argv;
    int argc;
    int i;

    if (cmd != callback_set_value) {
	config_error(locus, 0, _("Unexpected block statement"));
	return 1;
    }
    if (value->type != TYPE_ARRAY) {
	config_error(locus, 0, _("Not enough arguments for alias"));
	return 1;
    }
    argc = value->v.arg.c - 1;
    argv = xcalloc(argc + 1, sizeof(argv[0]));
    for (i = 0; i < argc; i++) {
	if (value->v.arg.v[i+1].type != TYPE_STRING) {
	    config_error(locus, 0, _("argument %d has wrong type"), i+1);
	    return 1;
	}
	argv[i] = value->v.arg.v[i+1].v.string;
    }
    argv[i] = NULL;
    return alias_install(value->v.arg.v[0].v.string, argc, argv, locus);
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
    { "show-sys-info", N_("arg: acl"),
      N_("Show system information if arg matches."),
      cfg_string, &show_sys_info, 0, apply_acl_cb },
    { "identity-check", N_("arg"),
      N_("Enable identification check using AUTH protocol (RFC 1413)"),
      cfg_bool, &identity_check },
    { "ident-timeout", N_("val"),
      N_("Set timeout for AUTH I/O operations."),
      cfg_long, &ident_timeout },
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
      N_("Set inactivity timeout."),
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
      N_("List of directories searched for database modules."),
      cfg_string|CFG_LIST, &module_load_path },
    { "default-strategy", N_("name"),
      N_("Set the name of the default matching strategy."),
      cfg_string, NULL, 0, set_defstrat },
    { "timing", N_("arg"),
      N_("Provide timing information after successful completion of an "
	 "operation."),
      cfg_bool, &timing_option },
    { "visibility-acl", N_("arg: acl"),
      N_("Set ACL to control visibility of all databases."),
      cfg_string, &global_acl, 0, apply_acl_cb },
    { "connection-acl", N_("arg: acl"),
      N_("Apply this ACL to incoming connections."),
      cfg_string, &connect_acl, 0, apply_acl_cb },
    { "database", NULL, N_("Define a dictionary database."),
      cfg_section, NULL, 0, set_database, NULL,
      kwd_database },
    { "load-module", N_("name: string"), N_("Load a module instance."),
      cfg_section, NULL, 0, load_module_cb, NULL,
      kwd_load_module },
    { "acl", N_("name: string"), N_("Define an ACL."),
      cfg_section, NULL, 0, acl_cb, NULL, kwd_acl },
    { "user-db", N_("url: string"),
      N_("Define user database for authentication."),
      cfg_section, &user_db_cfg, 0, user_db_config, NULL,
      kwd_user_db },
    { "sasl-disable-mechanism", N_("mech: list"),
      N_("Disable SASL mechanisms listed in <mech>."),
      cfg_string|CFG_LIST, &sasl_disabled_mech, },
    { "alias", N_("name: string"), N_("Define a command alias."),
      cfg_string, NULL, 0, alias_cb, },
    { NULL }
};

void
config_help()
{
    static char docstring[] =
	N_("Configuration file structure for dicod.\n"
	   "For more information, use `info dico configuration'.");
    format_docstring(stdout, docstring, 0);
    format_statement_array(stdout, keywords, 1, 0);
}


int
show_sys_info_p()
{
    if (!show_sys_info)
	return 1;
    return dicod_acl_check(show_sys_info, 1);
}

void
reset_db_visibility()
{
    dicod_database_t *db;
    dico_iterator_t itr;

    itr = xdico_iterator_create(database_list);
    for (db = dico_iterator_first(itr); db; db = dico_iterator_next(itr)) 
	db->visible = 1;
    dico_iterator_destroy(&itr);
}

void
check_db_visibility()
{
    dicod_database_t *db;
    dico_iterator_t itr;
    int global = dicod_acl_check(global_acl, 1);
    
    itr = xdico_iterator_create(database_list);
    for (db = dico_iterator_first(itr); db; db = dico_iterator_next(itr)) {
	db->visible = dicod_acl_check(db->acl, global);
    }
    dico_iterator_destroy(&itr);
}


static int
cmp_database_name(const void *item, const void *data)
{
    const dicod_database_t *db = item;
    int rc;
    
    if (!db->name)
	return 1;
    rc = strcmp(db->name, (const char*)data);
    if (rc == 0 && !database_visible_p(db))
	rc = 1;
    return rc;
}    

dicod_database_t *
find_database(const char *name)
{
    return dico_list_locate(database_list, (void*) name,
			    cmp_database_name);
}

static int
_count_databases(void *item, void *data)
{
    const dicod_database_t *db = item;
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
    dicod_database_t *db;
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
database_remove_dependent(dicod_module_instance_t *inst)
{
    dico_iterator_t itr = xdico_iterator_create(database_list);
    dicod_database_t *dp;

    for (dp = dico_iterator_first(itr); dp; dp = dico_iterator_next(itr)) {
	if (dp->instance == inst) {
	    dico_log(L_NOTICE, 0, _("removing database %s"), dp->name);
	    dico_iterator_remove_current(itr);
	    dicod_database_free(dp); 
	}
    }
    dico_iterator_destroy(&itr);
}

void
dicod_database_free(dicod_database_t *dp)
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


void
dicod_log_setup()
{
    if (!log_to_stderr) {
	openlog(log_tag, LOG_PID, log_facility);
	dico_set_log_printer(syslog_log_printer);
    }
}    

/* When requested restart by a SIGHUP, the daemon first starts
   a copy of itself with the `--lint' option to verify
   configuration settings.  This subsidiary process should use
   the same logging parameters as its ancestor.  In order to ensure
   that, the logging settings are passed to the lint child using
   __DICTD_LOGGING__ environment variable. */

void
dicod_log_encode_envar()
{
    char *p;
    asprintf(&p, "%d:%d:%s", log_facility, log_print_severity, log_tag);
    setenv(DICTD_LOGGING_ENVAR, p, 1);
}

void
dicod_log_pre_setup()
{
    char *str = getenv(DICTD_LOGGING_ENVAR);
    if (str) {
	char *p;
	log_facility = strtoul(str, &p, 10);
	if (*p == ':') {
	    log_print_severity = strtoul(p + 1, &p, 10);
	    if (*p == ':') 
		log_tag = p + 1;
	}
	log_to_stderr = 0;
	dicod_log_setup();
    }
}


int
main(int argc, char **argv)
{
    int rc = 0;
    
    appi18n_init();
    dico_set_program_name(argv[0]);
    set_quoting_style(NULL, escape_quoting_style);
    log_tag = dico_program_name;
    dicod_log_pre_setup();
    hostname = xdico_local_hostname();
    dicod_init_command_tab();
    dicod_init_strategies();
    udb_init();
    register_auth();
    register_mime();
    register_xidle();
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
    register_sasl();
    if (dicod_capa_flush())
	exit(1);
    compile_access_log();
    if (config_lint_option)
	exit(0);
    
    dicod_log_setup();
    
    dicod_loader_init();

    begin_timing("server");
    dicod_server_init();
    switch (mode) {
    case MODE_DAEMON:
	dicod_server(argc, argv);
	break;
	
    case MODE_INETD:
	rc = dicod_inetd();
	dicod_server_cleanup();
	break;
    }
    return rc;
}
