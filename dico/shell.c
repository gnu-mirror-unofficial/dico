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

#include "dico-priv.h"

struct funtab {
    char *name;        /* Keyword */
    int argmin;        /* Minimal number of arguments */
    int argmax;        /* Maximal number of arguments */
    void (*fun) (int argc, char **argv); /* Handler */
    char *docstring;   /* Documentation string */
};

static void ds_prefix(int argc, char **argv);
static void ds_help(int argc, char **argv);

struct funtab funtab[] = {
    { "open",      1, 3, ds_open,
      N_("Connect to a DICT server.") },
    { "close",     1, 1, ds_close,
      N_("Close the connection.") },
    { "autologin", 1, 2, ds_autologin,
      N_("Set or display autologin file name.")},
    { "database",  1, 2, ds_database,
      N_("Set or display current database name.") },
    { "strategy",  1, 2, ds_strategy,
      N_("Set or display current strategy.") },
    { "prefix", 1, 2, ds_prefix ,
      N_("Set or display command prefix.") },
    { "transcript", 1, 2, ds_transcript,
      N_("Set or display session transcript mode.") },
    { "help", 1, 1, ds_help,
      N_("Display this help text.") },
    { "quit", 1, 1, NULL,
      N_("Quit the shell.") },
    { NULL }
};

struct funtab *
find_funtab(const char *name)
{
    struct funtab *p;
    for (p = funtab; p->name; p++)
	if (strcmp(p->name, name) == 0)
	    return p;
    return NULL;
}

int cmdprefix = '.';
char special_prefixes[2];

static void
ds_prefix(int argc, char **argv)
{
    if (argc == 1)
	printf(_("Command prefix is %c\n"), cmdprefix);
    else if (argv[1][1] || !ispunct(argv[1][0]))
	script_error(0,
		     _("Expected a single punctuation character"));
    else {
	cmdprefix = argv[1][0];
	strcpy(special_prefixes, argv[1]);
	rl_special_prefixes = special_prefixes;
    }
}

static void
ds_help(int argc, char **argv)
{
    dico_stream_t str = create_pager_stream(sizeof(funtab)/sizeof(funtab[0]));
    struct funtab *ft;
    for (ft = funtab; ft->name; ft++) {
	stream_printf(str, "%c%-20s%s\n", cmdprefix, ft->name,
		      gettext(ft->docstring));
    }
    dico_stream_close(str);
    dico_stream_destroy(&str);
}

typedef int (*script_getln_fn)(void *data, char **buf);


int line = 0;
char *filename;

void
script_diag(int category, int errcode, const char *fmt, va_list ap)
{
    char *pfx;
    char *newfmt;
    
    if (category == L_WARN) 
	pfx = _("warning: ");
    else 
	pfx = NULL;
    
    if (!filename) 
	asprintf(&newfmt, "%s%s", pfx ? pfx : "", fmt);
    else {
	asprintf(&newfmt, "%s:%d: %s%s",
		 filename, line,
		 pfx ? pfx : "", fmt);
    }
    dico_vlog(category, errcode, newfmt, ap);
    free (newfmt);
}

void
script_warning(int errcode, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    script_diag(L_WARN, errcode, fmt, ap);
    va_end(ap);
}    

void
script_error(int errcode, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    script_diag(L_ERR, errcode, fmt, ap);
    va_end(ap);
}


void
parse_script_file(char *fname, script_getln_fn getln, void *data)
{
    char *buf = NULL;
    xdico_input_t input;
    int argc;
    char **argv;

    filename = fname;
    line = 0;
    input = xdico_tokenize_begin();
    while (getln(data, &buf)) {
	char *p;
	struct funtab *ft;
	char *xargv[3];
	
	line++;

	p = skipws(buf);
	dico_trim_nl(p);
	if (*p == 0 || *p == '#')
	    continue;

	xdico_tokenize_input(input, p, &argc, &argv);
	if (argc == 0)
	    continue;

	p = argv[0];
	switch (p[0]) {
	case '/':
	    /* FIXME: match */
	    continue;

	case '?':
	    ds_help(0, NULL);
	    continue;
	}
	    
	if (cmdprefix && p[0] == cmdprefix)
	    p++;
	
	ft = find_funtab(p);
	if (!ft) {
	    script_error(0, _("unknown command"));
	    continue;
	}
	if (ft->argmin == 0) {
	    argc = 2;
	    xargv[0] = argv[0];
	    xargv[1] = skipws(buf + strlen(xargv[0]));
	    xargv[2] = NULL;
	    argv = xargv;
	} else if (argc < ft->argmin) {
	    script_error(0, _("not enough arguments"));
	    continue;
	} else if (argc > ft->argmax) {
	    script_error(0, _("too many arguments"));
	    continue;
	}

	if (ft->fun)
	    ft->fun(argc, argv);
	else {
	    ds_silent_close();
	    break;
	}
    }
    xdico_tokenize_end(&input);
}
	

struct init_script {
    FILE *fp;
    char *buf;
    size_t size;
};

int
script_getline(void *data, char **buf)
{
    struct init_script *p = data;
    int rc = getline(&p->buf, &p->size, p->fp);
    *buf = p->buf;
    return rc > 0;
}

void
parse_init_script(const char *name)
{
    struct init_script scr;
    scr.fp = fopen(name, "r");
    if (!scr.fp) {
	if (errno != ENOENT) 
	    dico_log(L_ERR, errno, _("Cannot open init file %s"), name);
	return;
    }
    scr.buf = NULL;
    scr.size = 0;
    parse_script_file(name, script_getline, &scr);
    fclose(scr.fp);
    free(scr.buf);
}

void
parse_init_scripts()
{
    char *name = dico_full_file_name(get_homedir(), ".dico");
    parse_init_script(name);
    free(name);
    parse_init_script(".dico");
}


int interactive;
char *prompt = "dico> ";

#ifdef WITH_READLINE

static char *
_command_generator(const char *text, int state)
{
    static int i, len;
    const char *name;
  
    if (!state) {
	i = 0;
	len = strlen (text);
	if (cmdprefix)
	    len--;
    }
    if (cmdprefix)
	text++;
    while ((name = funtab[i].name)) {
	i++;
	if (strncmp(name, text, len) == 0) {
	    if (!cmdprefix)
		return strdup (name);
	    else {
		char *p = xmalloc(strlen(name) + 2);
		*p = cmdprefix;
		strcpy(p+1, name);
		return p;
	    }
	}
    }

    return NULL;
}

static char **
_command_completion(char *cmd, int start, int end)
{
    if (start == 0)
	return rl_completion_matches(cmd, _command_generator);
    return NULL;
}

#define HISTFILE_SUFFIX "_history"

static char *
get_history_file_name()
{
  static char *filename = NULL;

  if (!filename) {
	char *hname;
	
	hname = xmalloc(1 +
			strlen (rl_readline_name) + sizeof HISTFILE_SUFFIX);
	strcpy(hname, ".");
	strcat(hname, rl_readline_name);
	strcat(hname, HISTFILE_SUFFIX);
	filename = dico_full_file_name(get_homedir(), hname);
	free(hname);
  }
  return filename;
}
#endif

void
shell_init(struct init_script *p)
{
    interactive = isatty(fileno(stdin));
#ifdef WITH_READLINE
    if (interactive) {
	rl_readline_name = dico_program_name;
	rl_attempted_completion_function = (CPPFunction*) _command_completion;
	read_history (get_history_file_name());
    } 
#endif
    p->fp = stdin;
    p->buf = NULL;
    p->size = 0;
}

void
shell_finish(struct init_script *p)
{
#ifdef WITH_READLINE
    if (interactive)
	write_history (get_history_file_name());
#endif
    free(p->buf);
}

int
shell_getline(void *data, char **buf)
{
    if (interactive) {
#ifdef WITH_READLINE
	char *p = readline(prompt);
	if (!p)
	    return 0;
	*buf = p;
	add_history(p);
	return 1;
#else
	fprintf(stdout, "%s", prompt);
	fflush(stdout);
#endif
    }
    return script_getline(data, buf);
}

void
shell_banner()
{
    print_version(PACKAGE_STRING, stdout);
    printf("%s\n\n", _("Type ? for help summary"));
}

void
dico_shell()
{
    struct init_script dat;
    shell_init(&dat);
    if (interactive)
	shell_banner();
    parse_script_file(NULL, shell_getline, &dat);
    shell_finish(&dat);
}




