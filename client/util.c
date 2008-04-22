/* $Id$ */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <client.h>

/* ******************************************************************** */

XChar2b *
dup_16(XChar2b *str)
{
    int len;
    XChar2b *ret_str;
    
    len = strlen((char *) str);
    
    ret_str = (XChar2b *) malloc(sizeof(char) * len + sizeof(XChar2b));
    if (ret_str) 
	memcpy(ret_str, str, sizeof(char)*len + sizeof(XChar2b));
    return ret_str;
}

void
Beep()
{
    if (config.bell)
	XBell(display, 100);
}

int
GetWidgetNumber(Widget w, int fl)
{
    String str;
    char fmt[] = "%d";
    int ret;
    
    XtVaGetValues(w, XtNstring, &str, NULL);
    fmt[1] = fl;
    if (sscanf(str, fmt, &ret) != 1)
	return -1;
    return ret;
}


int
GetWidgetSkipCode(Widget w, String *sptr)
{
    String str;
    int num = 0;

    XtVaGetValues(w, XtNstring, &str, NULL);
    if (sptr)
	*sptr = str;
    num = strtol(str, (char**)&str, 10);
    if (*str != '-') {
	setstatus(True, "Invalid SKIP pattern: '-' expected");
	return 0;
    }
    num <<= 8;
    num |= strtol(str+1, (char**)&str, 10);
    if (*str != '-') {
	setstatus(True, "Invalid SKIP pattern: '-' expected");
	return 0;
    }
    num <<= 8;
    num |= strtol(str+1, (char**)&str, 10);
    while (*str && isspace(*str))
	str++;
    if (*str) {
	setstatus(True, "Invalid SKIP pattern: junk at the end");
	return 0;
    }
    return num;
}

void
SetWidgetNumber(Widget w, char *fmt, int val)
{
    char tempstr[100];
    if (val == 0)
	tempstr[0] = '\0';
    else
	sprintf(tempstr, fmt, val);
    XtVaSetValues(w, XtNstring, tempstr, NULL);
    return;
}

void
SetWidgetSkipCode(Widget w, int code)
{
    char buf[24];

    sprintf(buf, "%d-%d-%d", code >> 16, (code >> 8) & 0xf, code & 0xf);
    XtVaSetValues(w, XtNstring, buf, NULL);
}

void
SetWidgetBushu(Widget w, int radical, int strokes)
{
    char buf[24];

    print_bushu(buf, radical, strokes);
    XtVaSetValues(w, XtNstring, buf, NULL);
}


int
xtoi(char *s)
{
    register int out = 0;

    if (*s == '0' && (s[1] == 'x' || s[1] == 'X'))
	s += 2;
    for (; *s; s++) {
	if (*s >= 'a' && *s <= 'f')
	    out = (out << 4) + *s - 'a' + 10;
	else if (*s >= 'A' && *s <= 'F')
	    out = (out << 4) + *s - 'A' + 10;
	else if (*s >= '0' && *s <= '9')
	    out = (out << 4) + *s - '0';
	else 
	    break;
    }
    return out;
}

char *dictentry_buf;

void
init_dict_entry()
{
    logmsg(L_INFO, 0, "Allocating %d+%d bytes for dictionary entry buffer",
	 config.dict_entry.buf_size,
	 config.dict_entry.buf_incr);
    dictentry_buf = malloc(config.dict_entry.buf_size +
			   config.dict_entry.buf_incr);
    if (!dictentry_buf)
	logmsg(L_WARN, 0,
	       "cannot allocate dictionary entry buffer: using raw entry format");
}

char *
format_string(char *str)
{
    char *p;
    int num;
    
    if (!dictentry_buf)
	return str;
    if (str[0] == 0) 
	dictentry_buf[0] = 0;
    else {
	num = 0;
	p = dictentry_buf;
	num++;
	p += sprintf(p, "%d. ", num);
	while (*str)
	    if (*str == '|') {
		*p++ = ';';
		*p++ = '\n';
		str++;
		while (*str && *str == ' ')
		    str++;
		if (*str) {
		    num++;
		p += sprintf(p, "%d. ", num);
		}
	    } else
		*p++ = *str++;
	*p++ = ';';
	*p = 0;
    }
    
    return dictentry_buf;
}

char *
format_string16(char *str)
{
    char *p;
    int len;
    int num;

    if (!dictentry_buf)
	return str;
    len = strlen(str);
    shin_to_euc(str, len);
    num = 1;
    p = dictentry_buf;
    p += sprintf(p, "%d. ", num);
    while (*str) {
	if (*str == '\xa1' && str[1] == '\xa1') {
	    *p++ = '\n';
	    str += 2;
	    while (*str && *str == '\xa1' && str[1] == '\xa1')
		    str += 2;
	    
	    if (*str) {
		num++;
		p += sprintf(p, "%d. ", num);
	    }
	} else {
	    *p++ = *str++;
	    *p++ = *str++;
	}
    }
    *p = 0;
    return dictentry_buf;
}

/* Error - handling functions
 */
void
parm_error(char *func_name, int parm_no, char *val)
{
    logmsg(L_ERR, 0, "Parameter %d to %s(). Value passed: `%s'\n",
	   parm_no, func_name, val);
}

void
parm_cnt_error(char *func_name, int parm_cnt, int passed_cnt)
{
    logmsg(L_ERR, 0, "Too %s (%d) arguments to %s(). Expected %d\n",
	   (passed_cnt < parm_cnt) ? "few" : "many", passed_cnt,
	   func_name, parm_cnt);
}


int
decode_keyword(struct keyword *kw, char *string)
{
    for ( ; kw->name; kw++)
	if (strcmp(kw->name, string) == 0)
	    return kw->code;
    return -1;
}

#define MIN_ESC_BUF_SIZE 128

static char *escape_buf;
static int escape_len;

static void
alloc_escape_buf(int len)
{
    if (escape_len >= len)
	return;
    escape_len = (len > MIN_ESC_BUF_SIZE) ? len : MIN_ESC_BUF_SIZE;
    escape_buf = XtRealloc(escape_buf, escape_len);
}
    
    

char *
escape_string(char *str)
{
    char *p;
    
    alloc_escape_buf(2*strlen(str)+1);

    p = escape_buf;

    while (*str) {
	if (*str == '\\' || *str == '"') 
	    *p++ = '\\';
	*p++ = *str++;
    }
    *p = 0;
    return escape_buf;
}
	    
