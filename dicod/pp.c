/* This file is part of GNU Dico.
   Copyright (C) 1998, 1999, 2000, 2008, 2010 Sergey Poznyakoff

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
#include <sys/types.h>	
#include <sys/stat.h>
#include <sys/wait.h>
#include <hash.h>

struct input_file_ident {
    ino_t i_node;
    dev_t device;
};

struct buffer_ctx {
    struct buffer_ctx *prev;  /* Pointer to previous context */
    dicod_locus_t locus;      /* Current input location */
    size_t namelen;           /* Length of the file name */
    size_t xlines;            /* Number of #line directives output so far */
    struct input_file_ident id;
    FILE *infile;
};

extern int yy_flex_debug;
static struct buffer_ctx *context_stack;

#define INFILE context_stack->infile
#define LOCUS context_stack->locus

static char *linebuf;
static size_t bufsize;
static char *putback_buffer;
static size_t putback_size;
static size_t putback_max;

static int push_source(const char *name, int once);
static int pop_source(void);
static int parse_include(const char *text, int once);

static void
putback(const char *str)
{
    size_t len;

    if (!*str)
	return;
    len = strlen(str) + 1;
    if (len > putback_max) {
	 putback_max = len;
	 putback_buffer = xrealloc(putback_buffer, putback_max);
    }
    strcpy(putback_buffer, str);
    putback_size = len - 1;
}

/* Compute the size of the line

   #line NNN "FILENAME"
*/
static size_t
pp_line_stmt_size()
{
    char nbuf[128];
    size_t n = snprintf(nbuf, sizeof nbuf, "%lu",
			(unsigned long) LOCUS.line);
    size_t n1 = snprintf(nbuf, sizeof nbuf, "%lu",
			 (unsigned long) context_stack->xlines + 1);
    if (context_stack->namelen == 0)
	context_stack->namelen = strlen(LOCUS.file);
    /* "#line " is 6 chars, two more spaces, two quotes and a linefeed
       make another 5, summa facit 11 */
    return 11 + n + n1 + context_stack->namelen;
}

static void
pp_line_stmt()
{
    char *p;
    size_t ls_size = pp_line_stmt_size();
    size_t pb_size = putback_size + ls_size + 1;
    
    if (pb_size > putback_max) {
	putback_max = pb_size;
	putback_buffer = xrealloc(putback_buffer, putback_max);
    }
    
    p = putback_buffer + putback_size;
    context_stack->xlines++;
    snprintf(p, putback_max - putback_size,
	     "#line %lu \"%s\" %lu\n",
	     (unsigned long) LOCUS.line,
	     LOCUS.file,
	     (unsigned long) context_stack->xlines);
    putback_size += ls_size;
}

#define STRMATCH(p, len, s) (len >= sizeof(s)			      \
			     && memcmp (p, s, sizeof(s) - 1) == 0     \
			     && isspace(p[sizeof(s) - 1]))

static int
next_line()
{
    ssize_t rc;
    
    do {
	if (putback_size) {
	    if (putback_size + 1 > bufsize) {
		bufsize = putback_size + 1;
		linebuf = xrealloc(linebuf, bufsize);
	    }
	    strcpy(linebuf, putback_buffer);
	    rc = putback_size;
	    putback_size = 0;
	} else if (!context_stack)
	    return 0;
	else 
	    rc = getline(&linebuf, &bufsize, INFILE);
    } while (rc == -1 && pop_source() == 0);
    return rc;
}

size_t
pp_fill_buffer(char *buf, size_t size)
{
    size_t bufsize = size;
    
    while (next_line() > 0) {
	char *p;
	size_t len;
	int is_line = 0;
	
	for (p = linebuf; *p && isspace(*p); p++)
	    ;
	if (*p == '#') {
	    size_t l;
	    for (p++; *p && isspace(*p); p++)
		;
	    l = strlen(p);
	    if (STRMATCH(p, l, "include_once")) {
		if (parse_include(linebuf, 1))
		    putback("/*include_once*/\n");
		continue;
	    } else if (STRMATCH(p, l, "include")) {
		if (parse_include(linebuf, 0))
		    putback("/*include*/\n");
		continue;
	    } else if (STRMATCH(p, l, "line")) 
		is_line = 1;
	}
	
	len = strlen (linebuf);
	
	if (len > size)
	    len = size;
	
	memcpy(buf, linebuf, len);
	buf += len;
	size -= len;
	
	if (size == 0) {
	    putback(linebuf + len);
	    break;
	}
	
	if (!is_line && len > 0 && linebuf[len-1] == '\n')
	    LOCUS.line++;
    }
    return bufsize - size;
}

#define STAT_ID_EQ(st,id) ((id).i_node == (st).st_ino       \
			    && (id).device == (st).st_dev)

static struct buffer_ctx *
ctx_lookup(struct stat *st)
{
    struct buffer_ctx *ctx;
    
    if (!context_stack)
	return NULL;
    
    for (ctx = context_stack->prev; ctx; ctx = ctx->prev)
	if (STAT_ID_EQ(*st, ctx->id))
	    break;
    return ctx;
}

const char *preprocessor = DEFAULT_PREPROCESSOR;
static dico_list_t include_path;
static dico_list_t std_include_path;

struct file_data {
    const char *name;
    size_t namelen;
    char *buf;
    size_t buflen;
    int found;
};

static int
find_include_file(void *item, void *data)
{
    char *dir = item;
    struct file_data *dptr = data;
    size_t size = strlen(dir) + 1 + dptr->namelen + 1;
    if (size > dptr->buflen) {
	dptr->buflen = size;
	dptr->buf = xrealloc(dptr->buf, dptr->buflen);
    }
    strcpy(dptr->buf, dir);
    strcat(dptr->buf, "/");
    strcat(dptr->buf, dptr->name);
    return dptr->found = access(dptr->buf, F_OK) == 0;
}

void
include_path_setup()
{
    if (!include_path) 
	include_path = xdico_list_create();
    std_include_path = xdico_list_create();
    dico_list_append(std_include_path, DEFAULT_VERSION_INCLUDE_DIR);
    dico_list_append(std_include_path, DEFAULT_INCLUDE_DIR);
}

void
add_include_dir(char *dir)
{
    if (!include_path) 
	include_path = xdico_list_create();
    xdico_list_append(include_path, dir);
}

static Hash_table *incl_sources;

/* Calculate the hash of a struct input_file_ident.  */
static size_t
incl_hasher(void const *data, unsigned n_buckets)
{
    const struct input_file_ident *id = data;
    return (id->i_node + id->device) % n_buckets;
}

/* Compare two input_file_idents for equality.  */
static bool
incl_compare(void const *data1, void const *data2)
{
    const struct input_file_ident *id1 = data1;
    const struct input_file_ident *id2 = data2;
    return id1->device == id2->device && id1->i_node == id2->i_node;
}

static void
incl_free(void *data)
{
    free(data);
}

static int
source_lookup(struct stat *st)
{
    struct input_file_ident *sample = xmalloc(sizeof(*sample)), *id;
    
    sample->i_node = st->st_ino;
    sample->device = st->st_dev;
    
    if (! ((incl_sources
	    || (incl_sources = hash_initialize(0, 0, 
					       incl_hasher,
					       incl_compare,
					       incl_free)))
	   && (id = hash_insert(incl_sources, sample))))
	xalloc_die ();
    
    if (id != sample) {
	free(sample);
	return 1; /* Found */
    }
    return 0;
}


static Hash_table *text_table;

/* Calculate the hash of a string.  */
static size_t
text_hasher(void const *data, unsigned n_buckets)
{
    return hash_string(data, n_buckets);
}

/* Compare two strings for equality.  */
static bool
text_compare(void const *data1, void const *data2)
{
    return strcmp(data1, data2) == 0;
}

/* Lookup a text. If it does not exist, create it. */
char *
install_text(const char *str)
{
    char *text, *s;
    
    s = xstrdup(str);
    
    if (! ((text_table
	    || (text_table = hash_initialize(0, 0, 
					     text_hasher,
					     text_compare,
					     incl_free)))
	   && (text = hash_insert(text_table, s))))
	xalloc_die ();
    
    if (text != text) 
	free(s);
    return text;
}


static int
push_source(const char *name, int once)
{
    FILE *fp;
    struct buffer_ctx *ctx;
    struct stat st;
    int rc = stat(name, &st);
    
    if (context_stack) {
	if (rc) {
	    config_error(&LOCUS, errno, _("Cannot stat `%s'"), name);
	    return 1;
	}
	
	if (LOCUS.file && STAT_ID_EQ(st, context_stack->id)) {
	    config_error(&LOCUS, 0, _("Recursive inclusion"));
	    return 1;
	}
	
	if ((ctx = ctx_lookup(&st))) {
	    config_error(&LOCUS, 0, _("Recursive inclusion"));
	    if (ctx->prev)
		config_error(&ctx->prev->locus, 0,
			     _("`%s' already included here"),
			     name);
	    else
		config_error(&LOCUS, 0,
			     _("`%s' already included at top level"),
			     name);
	    return 1;
	}
    } else if (rc) {
	config_error(NULL, errno, _("Cannot stat `%s'"), name);
	return 1;
    }
    
    if (once && source_lookup(&st)) 
	return -1;
    
    fp = fopen(name, "r");
    if (!fp) {
	config_error(&LOCUS, errno, _("Cannot open `%s'"), name);
	return 1;
    }
    
    /* Push current context */
    ctx = xmalloc (sizeof (*ctx));
    ctx->locus.file = install_text(name);
    ctx->locus.line = 1;
    ctx->xlines = 0;
    ctx->namelen = strlen(ctx->locus.file);
    ctx->id.i_node = st.st_ino;
    ctx->id.device = st.st_dev;
    ctx->infile = fp;
    ctx->prev = context_stack;
    context_stack = ctx;

    if (yy_flex_debug)
	fprintf(stderr, "Processing file `%s'\n", name);

    pp_line_stmt();
    
    return 0;
}

static int
pop_source()
{
    struct buffer_ctx *ctx;
    
    if (!context_stack)
	return 1;
    
    fclose(INFILE);
    
    /* Restore previous context */
    ctx = context_stack->prev;
    free(context_stack);
    context_stack = ctx;
    
    if (!context_stack) {
	if (yy_flex_debug)
	    fprintf(stderr, "End of input\n");
	return 1;
    }
    
    LOCUS.line++;
    
    if (yy_flex_debug) 
	fprintf(stderr, "Resuming file `%s' at line %lu\n",
		LOCUS.file, (unsigned long) LOCUS.line);

    pp_line_stmt();
    
    return 0;
}

static int
try_file(const char *name, int allow_cwd, int err_not_found, char **newp)
{
    static char *cwd = ".";
    struct file_data fd;

    fd.name = name;
    fd.namelen = strlen(name);
    fd.buf = NULL;
    fd.buflen = 0;
    fd.found = 0;

    if (allow_cwd) {
	dico_list_push(include_path, cwd);
	dico_list_iterate(include_path, find_include_file, &fd);
	dico_list_pop(include_path);
    } else
	dico_list_iterate(include_path, find_include_file, &fd);

    if (!fd.found) {
	dico_list_iterate(std_include_path, find_include_file, &fd);
	
	if (!fd.found && err_not_found) {
	    config_error(&LOCUS, 0,
			 _("%s: No such file or directory"), name);
	    *newp = NULL;
	} 
    }
    if (fd.found)
	*newp = fd.buf;
    return fd.found;
}

static int
parse_include(const char *text, int once)
{
    int argc;
    char **argv;
    char *tmp = NULL;
    char *p = NULL;
    int rc = 1;
    
    if (dico_argcv_get(text, "", NULL, &argc, &argv)) 
	config_error(&LOCUS, 0, _("Cannot parse include line"));
    else if (argc != 2) 
	config_error(&LOCUS, 0, _("invalid include statement"));
    else {
	size_t len;
	int allow_cwd;
	
	p = argv[1];
	len = strlen(p);
	
	if (p[0] == '<' && p[len - 1] == '>') {
	    allow_cwd = 0;
	    p[len - 1] = 0;
	    p++;
	} else 
	    allow_cwd = 1;
	
	if (p[0] != '/' && try_file(p, allow_cwd, 1, &tmp))
	    p = tmp;
    }

    if (p)
	rc = push_source(p, once);
    free(tmp);
    dico_argcv_free(argc, argv);
    return rc;
}

int
pp_init(const char *name)
{
    return push_source(name, 0);
}

void
pp_done()
{
    if (incl_sources)
	hash_free(incl_sources);
    if (text_table)
	hash_free(text_table);
    free(linebuf);
    free(putback_buffer);
}

int
preprocess_config(const char *extpp)
{
    size_t i;
    char buffer[512];

    if (pp_init(config_file))
	return 1;
    if (extpp) {
	FILE *outfile;
	char *setup_file = NULL;
	char *cmd;
	
	if (try_file("pp-setup", 1, 0, &setup_file)) 
	    asprintf(&cmd, "%s %s -", extpp, setup_file);
	else
	    cmd = (char*) extpp;
	XDICO_DEBUG_F1(2, "Running preprocessor: `%s'", cmd);
	outfile = popen(cmd, "w");
	if (!outfile) {
	    dico_log(L_ERR, errno,
		     _("Unable to start external preprocessor `%s'"),
		     cmd);
	    if (setup_file) {
		free(setup_file);
		free(cmd);
	    }
	    return 1;
	}
	
	while ((i = pp_fill_buffer(buffer, sizeof buffer)))
	    fwrite(buffer, 1, i, outfile);
	pclose(outfile);
	if (setup_file) {
	    free(setup_file);
	    free(cmd);
	}
    } else {
	while ((i = pp_fill_buffer(buffer, sizeof buffer)))
	    fwrite(buffer, 1, i, stdout);
    }
    pp_done();
    return 0;
}

void
pp_make_argcv(int *pargc, const char ***pargv)
{
    size_t n = 0;
    int argc;
    const char **argv;
    dico_iterator_t itr;
    char *cp;
    
    n = dico_list_count(include_path);
    argc = 9 + 2 * n;
    argv = xcalloc(argc + 1, sizeof argv[0]);
	
    argc = 0;
    argv[argc++] = dico_invocation_name;
    argv[argc++] = "-E";
    argv[argc++] = "--preprocessor";
    argv[argc++] = preprocessor;
    argv[argc++] = log_to_stderr ? "--stderr" : "--syslog";
    itr = xdico_list_iterator(include_path);
    for (cp = dico_iterator_first(itr); cp; cp = dico_iterator_next(itr)) {
	argv[argc++] = "-I";
	argv[argc++] = cp;
    }
    dico_iterator_destroy(&itr);
    argv[argc++] = "--config";
    argv[argc++] = config_file;
    if (debug_level) {
	argv[argc++] = "--debug";
	argv[argc++] = debug_level_str;
    }
    argv[argc] = NULL;

    *pargc = argc;
    *pargv = argv;
}

FILE *
pp_extrn_start(int argc, const char **argv, pid_t *ppid)
{
    int pout[2];
    pid_t pid;
    int i;
    char *ppcmd = "unknown";
    FILE *fp = NULL;
    
    dico_argcv_string(argc, argv, &ppcmd);
    XDICO_DEBUG_F1(2, "Running preprocessor: `%s'", ppcmd);
	
    pipe(pout);
    switch (pid = fork()) {
	/* The child branch.  */
    case 0:
	if (pout[1] != 1) {
	    close(1);
	    dup2(pout[1], 1);
	}
	
	/* Close unneeded descripitors */
	for (i = getmaxfd(); i > 2; i--)
	    close(i);

	if (!log_to_stderr) {
	    int p[2];
	    char *buf = NULL;
	    size_t size = 0;
	    FILE *fp;
	    
	    signal(SIGCHLD, SIG_DFL);
	    pipe(p);
	    switch (pid = fork()) {
		/* Grandchild */
	    case 0:
		if (p[1] != 2) {
		    close(2);
		    dup2(p[1], 2);
		}
		close(p[0]);
		
		execvp(argv[0], (char**)argv);
		exit(EX_UNAVAILABLE);
		
	    case -1:
		/*  Fork failed */
		dicod_log_setup();
		dico_log(L_ERR, errno, _("Cannot run `%s'"), ppcmd);
		exit(EX_OSERR);

	    default:
		/* Sub-master */
		close(p[1]);
		fp = fdopen(p[0], "r");
		dicod_log_setup();
		while (getline(&buf, &size, fp) > 0)
		    dico_log(L_ERR, 0, "%s", buf);
		exit(EX_OK);
	    }
	} else {
	    execvp(argv[0], (char**)argv);
	    dicod_log_setup();
	    dico_log(L_ERR, 0, _("Cannot run `%s'"), ppcmd);
	    exit(EX_UNAVAILABLE);
	}
	
    case -1:
	/* Fork failed */
	dico_log(L_ERR, 0, _("Cannot run `%s'"), ppcmd);
	break;
		
    default:
	close(pout[1]);
	fp = fdopen(pout[0], "r");
	break;
    }
    *ppid = pid;
    return fp;
}

void
pp_extrn_shutdown(pid_t pid)
{
    int status;
    waitpid(pid, &status, 0);
}

void
run_lint()
{
    size_t n = 0;
    int argc;
    const char **argv;
    dico_iterator_t itr;
    char *cp;

    n = dico_list_count(include_path);
    argc = 9 + 2 * n;
    argv = xcalloc(argc + 1, sizeof argv[0]);
	
    argc = 0;
    argv[argc++] = dico_invocation_name;
    argv[argc++] = "--lint";
    argv[argc++] = "--preprocessor";
    argv[argc++] = preprocessor;
    if (log_to_stderr)
	argv[argc++] = "--stderr";
    else 
	dicod_log_encode_envar();
    itr = xdico_list_iterator(include_path);
    for (cp = dico_iterator_first(itr); cp; cp = dico_iterator_next(itr)) {
	argv[argc++] = "-I";
	argv[argc++] = cp;
    }
    dico_iterator_destroy(&itr);
    argv[argc++] = "--config";
    argv[argc++] = config_file;
    if (debug_level) {
	argv[argc++] = "--debug";
	argv[argc++] = debug_level_str;
    }
    argv[argc] = NULL;
    
    execv(argv[0], (char**) argv);
}

