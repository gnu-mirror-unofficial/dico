#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <gjdictd.h>
#include <server.h>

FILE *in;
FILE *out;
FILE *datafile;
int sock;
int binary;
int data_socket = -1;
int pasv_fd = -1;
int usedefault = 1;

struct sockaddr_in myctl_addr;
struct sockaddr_in pasv_addr;
struct sockaddr_in data_dest;
struct sockaddr_in data_source;
struct sockaddr_in his_addr;

#define	SWAITMAX	90	/* wait at most 90 seconds */
#define	SWAITINT	5	/* interval between retries */

int	swaitmax = SWAITMAX;
int	swaitint = SWAITINT;

static FILE * dataconn(char *mode);
RETSIGTYPE sig_pipe(int sig);
RETSIGTYPE sig_alrm(int sig);

int
set_fd_ctl_addr(int fd)
{
    int len = sizeof(myctl_addr);
    if (getsockname(fd, (struct sockaddr *)&myctl_addr, &len) < 0) {
	logmsg(L_ERR, errno, "getsockname");
	close(fd);
	return -1;
    }
    return 0;
}

#ifndef INADDR_LOOPBACK
# define INADDR_LOOPBACK 0x7f000001
#endif

void
set_lo_ctl_addr()
{
    myctl_addr.sin_family = AF_INET;
    myctl_addr.sin_port = 0;
    myctl_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
}

int
server(FILE *infile, FILE *outfile)
{
    in = infile;
    out = outfile;
    SETVBUF(in, NULL, _IOLBF, 0);
    SETVBUF(out, NULL, _IOLBF, 0);
    
    signal(SIGPIPE, sig_pipe);
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGUSR1, SIG_IGN);
    signal(SIGUSR2, SIG_IGN);
    signal(SIGALRM, sig_alrm);
    
    msg(RC_OK, "%s gjdict server version %s ready", myhostname, VERSION);
    status.start_time = time(NULL);

    yyparse();

    cmsg(RC_OK, "thank you for using gjdict service at %s", myhostname);
    msg(RC_OK, "good bye");
    
    fclose(in);
    fclose(out);
    
    return 0;
}

RETSIGTYPE
sig_alrm(int sig)
{
    logmsg(L_ERR, 0, "timeout reached");
    fclose(in);
    fclose(out);
    close(sock);
    exit(0);
}

RETSIGTYPE
sig_pipe(int sig)
{
    logmsg(L_ERR, 0, "Broken pipe. Exit");
    exit(0);
}

void
active(int a1, int a2, int a3, int a4, int p1, int p2)
{
    char *a, *p;
    
    data_dest.sin_family = AF_INET;
    p = (char *)&data_dest.sin_port;
    p[0] = p1; p[1] = p2;
    a = (char *)&data_dest.sin_addr;
    a[0] = a1; a[1] = a2; a[2] = a3; a[3] = a4;
    msg(RC_OK, "OK");
}

int
passive()
{
    int len;
    char *p, *a;

    if (pasv_fd >= 0)		/* close old port if one set */
	close(pasv_fd);

    pasv_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (pasv_fd < 0) {
	msg(RC_ERR, "cannot open passive connection");
	return -1;
    }

    /* seteuid((uid_t)0); */

    pasv_addr = myctl_addr;
    pasv_addr.sin_port = 0;
    if (bind(pasv_fd, (struct sockaddr *)&pasv_addr, sizeof(pasv_addr)) < 0)
	goto pasv_error;

    /* seteuid((uid_t)pw->pw_uid); */

    len = sizeof(pasv_addr);
    if (getsockname(pasv_fd, (struct sockaddr *) &pasv_addr, &len) < 0)
	goto pasv_error;
    if (listen(pasv_fd, 1) < 0)
	goto pasv_error;
    a = (char *) &pasv_addr.sin_addr;
    p = (char *) &pasv_addr.sin_port;

#define UC(b) (((int) b) & 0xff)

    msg(RC_OK, "Entering Passive Mode (%d,%d,%d,%d,%d,%d)", UC(a[0]),
	UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]), UC(p[1]));
    logmsg(L_INFO, 0, "entering passive mode (%d,%d,%d,%d,%d,%d)", UC(a[0]),
	   UC(a[1]), UC(a[2]), UC(a[3]), UC(p[0]), UC(p[1]));
    return 0;

pasv_error:
    /* seteuid((uid_t)pw->pw_uid); */
    close(pasv_fd);
    pasv_fd = -1;
    msg(RC_ERR, "cannot open passive connection");
    return -1;
}

int
opendata()
{
    if (binary) {
	if (datafile)
	    msg(RC_OK, "using existing data connection");
	else if ((datafile = dataconn("w")) == NULL)
	    return -1;
    } else {
	msg(RC_OK, "OK");
    }
    return 0;
}

void
closedata()
{
    if (datafile) {
	fclose(datafile);
	datafile = NULL;
    }
    msg(RC_OK, "OK");
}

static FILE *
getdatasock(char *mode)
{
	int on = 1, s, t, tries;

	if (data_socket >= 0)
	    return fdopen(data_socket, mode);
/*	(void) seteuid((uid_t)0);*/
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
	    goto bad;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
		       (char *) &on, sizeof(on)) < 0)
	    goto bad;
	/* anchor socket to avoid multi-homing problems */
	data_source.sin_family = AF_INET;
	data_source.sin_addr = myctl_addr.sin_addr;
	for (tries = 1; ; tries++) {
	    if (bind(s, (struct sockaddr *)&data_source,
		     sizeof(data_source)) >= 0)
		break;
	    if (errno != EADDRINUSE || tries > 10)
		goto bad;
	    sleep(tries);
	}
/*	(void) seteuid((uid_t)pw->pw_uid);**/
#ifdef IP_TOS
	on = IPTOS_THROUGHPUT;
	if (setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&on, sizeof(int)) < 0)
	    logmsg(L_ERR, errno, "setsockopt (IP_TOS)");
#endif
#ifdef TCP_NOPUSH
	/*
	 * Turn off push flag to keep sender TCP from sending short packets
	 * at the boundaries of each write().  Should probably do a SO_SNDBUF
	 * to set the send buffer size as well, but that may not be desirable
	 * in heavy-load situations.
	 */
	on = 1;
	if (setsockopt(s, IPPROTO_TCP, TCP_NOPUSH, (char *)&on, sizeof on) < 0)
	    logmsg(L_ERR, errno, "setsockopt (TCP_NOPUSH)");
#endif
#ifdef SO_SNDBUF
	on = 65536;
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)&on, sizeof on) < 0)
	    logmsg(L_ERR, errno, "setsockopt (SO_SNDBUF)");
#endif

	return fdopen(s, mode);
bad:
	/* Return the real value of errno (close may change it) */
	t = errno;
/*	(void) seteuid((uid_t)pw->pw_uid);*/
	(void) close(s);
	errno = t;
	return NULL;
}

static FILE *
dataconn(char *mode)
{
    int retry = 0, tos;
    FILE *file;
    
    if (pasv_fd >= 0) {
	struct sockaddr_in from;
	int s, fromlen = sizeof(from);
	struct timeval timeout;
	fd_set set;
	
	FD_ZERO(&set);
	FD_SET(pasv_fd, &set);

	timeout.tv_usec = 0;
	timeout.tv_sec = 120;

	if (select(pasv_fd+1, &set, (fd_set *) 0, (fd_set *) 0, &timeout) == 0 ||
	    (s = accept(pasv_fd, (struct sockaddr *) &from, &fromlen)) < 0) {
	    msg(RC_ERR, "cannot open data connection.");
	    close(pasv_fd);
	    pasv_fd = -1;
	    return NULL;
	}
	close(pasv_fd);
	pasv_fd = s;
#ifdef IP_TOS
	tos = IPTOS_THROUGHPUT;
	setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&tos, sizeof(int));
#endif
	msg(RC_OK, "opening data connection");
	return fdopen(pasv_fd, mode);
    }
    if (data_socket >= 0) {
	msg(RC_OK, "Using existing data connection");
	return fdopen(data_socket, mode);
    }
    if (usedefault)
	data_dest = his_addr;
    usedefault = 1;
    file = getdatasock(mode);
    if (file == NULL) {
	msg(RC_ERR, "cannot create data socket (%s,%d): %s.",
	    inet_ntoa(data_source.sin_addr),
	    ntohs(data_source.sin_port), strerror(errno));
	return NULL;
    }
    data_socket = fileno(file);
    while (connect(data_socket, (struct sockaddr *)&data_dest,
		   sizeof(data_dest)) < 0) {
	if (errno == EADDRINUSE && retry < swaitmax) {
	    sleep((unsigned) swaitint);
	    retry += swaitint;
	    continue;
	}
	msg(RC_ERR, "cannot build data connection");
	fclose(file);
	data_socket = -1;
	return NULL;
    }
    msg(RC_OK, "Opening data connection");
    return file;
}


void
xlat_entry(DictEntry *dst, DictEntry *src)
{
    dst->bushu = htons(src->bushu);               
    dst->numstrokes = htons(src->numstrokes);
    dst->Qindex = htons(src->Qindex);
    dst->skip = htonl(src->skip);
    dst->Jindex = htons(src->Jindex);              
    dst->Uindex = htons(src->Uindex);		
    dst->Nindex = htons(src->Nindex);		
    dst->Hindex = htons(src->Hindex);		
    dst->frequency = htons(src->frequency);
    dst->grade_level = src->grade_level;
    dst->refcnt = htonl(src->refcnt);                 
    dst->english = htonl(src->english);		
    dst->pronunciation = htonl(src->pronunciation);	
    dst->kanji = htonl(src->kanji);		
    dst->pinyin = htonl(src->pinyin);
}

void
send_entries(DictEntry *entry, int cnt)
{
    int n;
    DictEntry ce;

    n = ntohl(cnt);
    fwrite(&n, sizeof(n), 1, datafile);
    for (n = 0; n < cnt; n++, entry++) {
	if (binary) {
	    xlat_entry(&ce, entry);
	    fwrite(&ce, 1, sizeof(ce), datafile);
	} else {
	    udata("B:%u:%u Q:%u S:%u J:%u U:%u N:%u H:%u F:%u G:%u R:%u E:%u Y:%u K:%u P:%u",
		 entry->bushu,
		 entry->numstrokes,
		 entry->Qindex,
		 entry->skip,
		 entry->Jindex,
		 entry->Uindex,	
		 entry->Nindex,	
		 entry->Hindex,		
		 entry->frequency,
		 entry->grade_level,
		 entry->refcnt,
		 entry->english,
		 entry->pronunciation,
		 entry->kanji,
		 entry->pinyin);            

	}
    }
    if (binary)
	fflush(datafile);
}


int
send_recno(int cnt, Recno *buf)
{
    int i;

    if (binary) {
	for (i = 0; i < cnt; i++)
	    buf[i] = htonl(buf[i]);
	i = fwrite(buf, sizeof(buf[0]), cnt, datafile);
	fflush(datafile);
    } else {
	for (i = 0; i < cnt; i++)
	    udata("%d", buf[i]);
    }
    if (i < 0) {
	logmsg(L_ERR, errno, "sending recnos");
	return -1;
    }
    return 0;
    
}

int
quit(int code)
{
    fclose(in);
    fclose(out);
    close(sock);
    exit(code);
}

void
vmsg(int num, int cont, char *fmt, va_list ap)
{
    fprintf(out, "%d%c", num, cont ? '-' : ' ');
    vfprintf(out, fmt, ap);
    fprintf(out, "\r\n");
}

void
cmsg(int num, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vmsg(num, 1, fmt, ap);
    va_end(ap);
}

void
msg(int num, char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vmsg(num, 0, fmt, ap);
    va_end(ap);
}

void
eofline()
{
    fprintf(out, "\r\n");
}

void
udata(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(out, "* ");
    vfprintf(out, fmt, ap);
    eofline();
}

void
getstr(Offset offset)
{
    char *ptr;
    int length, nlength;
    
    ptr = dict_string(offset);
    if (!ptr) {
	msg(RC_ERR, "no such string");
	return;
    }

    msg(RC_OK, "OK");
    if (binary) {
	length = strlen(ptr)+1;
	nlength = htonl(length);
	fwrite(&nlength, 1, sizeof(nlength), datafile);
	fwrite(ptr, 1, length, datafile);
	fflush(datafile);
    } else {
	udata("%d %s", length, ptr);
    }
}

void
find(char *str)
{
    switch (search_ctl.tree) {
    case TreeJis:
    case TreeUnicode:
    case TreeFreq:
    case TreeNelson:
    case TreeHalpern:
	search_number16(str);
	break;

    case TreeGrade:
	search_number8(str);
	break;
	
    case TreeCorner:
	search_corner(str);
	break;

    case TreeBushu:
	search_bushu(str);
	break;
	
    case TreeSkip:
	search_skip(str);
	break;
	
    case TreePinyin:
    case TreeEnglish:
    case TreeWords:
    case TreeRomaji:
	search_string16(str);
	break;
	
    case TreeKanji:
    case TreeYomi:
	search_string32(str);
	break;

    default:
	msg(RC_CRIT,
	    "find(): internal error: search_tree = %d", search_ctl.tree);
	quit(2);
    }
}

void
server_stat()
{
    int hrs, min, sec;
    
    cmsg(RC_OK, "%s gjdict server version %s", myhostname, VERSION);

    sec = time(NULL) - status.load_time;
    hrs = sec / 3600;
    sec -= hrs * 3600;
    min = sec / 60;
    sec -= min * 60;
    cmsg(RC_OK, "master process uptime %d:%d:%d", hrs, min, sec);

    sec = time(NULL) - status.start_time;
    hrs = sec / 3600;
    sec -= hrs * 3600;
    min = sec / 60;
    sec -= min * 60;
    cmsg(RC_OK, "uptime %d:%d:%d", hrs, min, sec);
    cmsg(RC_OK, "queries %d,%d,%d",
	status.query, status.total_match, status.match);
    msg(RC_OK, "KEY %s", tree_str[search_ctl.tree]);
}


















