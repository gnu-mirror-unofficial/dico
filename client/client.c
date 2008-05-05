#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <limits.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <X11/Intrinsic.h>
#include <sys/time.h>

#include <client.h>
#include <options.h>
#include <dict.h>
#include <dico.h>

static int sockfd = -1;
static int data_socket;
static struct sockaddr_in data_addr;
#define BUFFER_SIZE 1024

static FILE *cin, *cout;
char reply_buf[BUFFER_SIZE];


int get_reply();
void drain_input();


int
init_connection()
{
    int length;
    struct sockaddr salocal;
    struct sockaddr saremote;
    struct sockaddr_in *sin;
	
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
	die(1, L_CRIT, errno, "socket");
	
    length = sizeof(salocal);
    sin = (struct sockaddr_in *) & saremote;
    memset((char *) sin, 0, (size_t) length);
	
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = htonl(server_addr);
    sin->sin_port = (unsigned short) htons(port_no);
    logmsg(L_INFO, 0, "connecting to %s:%d",
	 inet_ntoa(sin->sin_addr), port_no);
    if (connect(sockfd, (struct sockaddr *) sin, length) < 0) {
	logmsg(L_ERR, errno, "cannot connect to %s:%d",
	       inet_ntoa(sin->sin_addr), port_no);
	close (sockfd);
	sockfd = -1;
	return -1;
    }

	
    cin = fdopen(sockfd, "r");
    cout = fdopen(sockfd, "w");
    if (!cin || !cout) {
	logmsg(L_ERR, errno, "fdopen failed");
	if (cin)
	    fclose(cin);
	if (cout)
	    fclose(cout);
	close(sockfd);
	sockfd = -1;
	return -1;
    }
    SETVBUF(cin, NULL, _IONBF, 0);
    SETVBUF(cout, NULL, _IOLBF, 0);

    get_reply();
    setup_server_options();
    return 0;
}
 
int
open_data_connection()
{
    char *p, *a;
    int a0, a1, a2, a3, p0, p1;
    char buf[BUFFER_SIZE];
    
    sendf("PASV");
    if (get_lines(buf, sizeof(buf)) != RC_OK) 
	return -1;
    p = strchr(buf, '(');
    
    if (!p ||
	sscanf(p, "(%d,%d,%d,%d,%d,%d)",
	   &a0, &a1, &a2, &a3, &p0, &p1) != 6) {
	logmsg(L_ERR, 0,
	       "passive mode address scan failure. Shouldn't happen!");
	return -1;
    }

    data_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (data_socket == -1) {
	logmsg(L_ERR, errno, "socket");
	return -1;
    }
    memset(&data_addr, 0, sizeof(data_addr));
    data_addr.sin_family = AF_INET;
    a = (char *)&data_addr.sin_addr.s_addr;
    a[0] = a0 & 0xff;
    a[1] = a1 & 0xff;
    a[2] = a2 & 0xff;
    a[3] = a3 & 0xff;
    p = (char *)&data_addr.sin_port;
    p[0] = p0 & 0xff;
    p[1] = p1 & 0xff;

    if (connect(data_socket, (struct sockaddr *)&data_addr,
		sizeof(data_addr)) < 0) {
	logmsg(L_ERR, errno, "connect");
	close(data_socket);
	data_socket = -1;
	return -1;
    }
#ifdef IP_TOS
# ifdef IPTOS_THROUGHPUT
 {
     int on = IPTOS_THROUGHPUT;
     if (setsockopt(data_socket, IPPROTO_IP, IP_TOS, (char *)&on,
		    sizeof(int)) < 0)
	 /*warn("setsockopt TOS (ignored)")*/;
 }
# endif
#endif
    sendf("GET");
    if (get_reply() != RC_OK) {
	logmsg(L_ERR, 0, "cannot establish data connection");
	logmsg(L_ERR, 0, "server reply: %s", reply_buf);
	close(data_socket);
	return -1;
    }
    return 0;
}

void
close_data_connection()
{
    sendf("DONE");
    get_reply();
    close(data_socket);
}

void
drop_connection()
{
    if (sockfd != -1) {
	fclose(cin);
	fclose(cout);
	close(sockfd);
	sockfd = -1;
    }
}

void
close_connection()
{
    if (sockfd != -1) {
	if (!broken_pipe) {
	    sendf("QUIT");
	    get_reply();
	}
	fclose(cin);
	fclose(cout);
	close(sockfd);
	sockfd = -1;
    }
}

void
lostpeer()
{
    broken_pipe = True;
}

void
sendf(char *fmt, ...)
{
    va_list ap;
    
    va_start(ap, fmt);
    vfprintf(cout, fmt, ap);
    if (config.transcript) {
	vfprintf(stdout, fmt, ap);
	fprintf(stdout, "\n");
    }
    fprintf(cout, "\r\n");
    va_end(ap);
}


char eofline[] = "\r\n";

int
has_data(int fd, int sec)
{
    struct timeval t;
    fd_set rfd;

    t.tv_sec = (long) sec;
    t.tv_usec = 0;
    FD_ZERO(&rfd);
    FD_SET(fd, &rfd);
    return select(fd, &rfd, NULL, NULL, &t);
}

int
get_lines(char *buf, size_t bufsize)
{
    size_t offset = 0;
    int code;
    int more;
    
    do {
	int digit;
	size_t count;
	char *p = fgets(buf + offset, bufsize - offset, cin);
	if (!p) {
	    lostpeer();
	    offset++;
	    more = 0;
	    code = RC_ERR;	    
	    break;
	}
	count = strlen(p);
	if (count < 4) {
	    code = RC_ERR;
	    break;
	}
	code = 0;
	for (digit = 0; digit < 3 && isdigit(*p); digit++, p++)
	    code = code * 10 + *p - '0';
 	offset += count;
	switch (*p) {
	case '-':
	    more = 1;
	    break;
	case ' ':
	    more = 0;
	    break;
	default:
	    more = 0;
	    code = RC_ERR;
	    break;
	}
	buf[offset] = 0;
    } while (more && offset < bufsize);
    if (config.transcript) 
	printf("--> %s", buf);
    return code;
}

int
get_reply()
{
    return get_lines(reply_buf, sizeof(reply_buf));
}

void
tryagain(char *str)
{
    if (Xgetyn(str)) 
	return;
    exit (1);
}

unsigned
read_data(void *buf, size_t size)
{
    size_t rsize;
    unsigned offset;
    fd_set rfd;
    struct timeval to;
    int rc;
    
    offset = 0;
    do {
	FD_ZERO(&rfd);
	FD_SET(data_socket, &rfd);

	to.tv_sec = 30; /*FIXME*/
	to.tv_usec = 0;
	rc = select(data_socket+1, &rfd, NULL, NULL, &to);
	if (rc < 0) {
	    if (errno == EINTR)
		continue;
	    logmsg(L_ERR, errno, "select");
	    tryagain("Retry");
	    continue;
	} else if (rc == 0) {
	    logmsg(L_ERR, 0, "no data within 30 seconds");
	    tryagain("No data within 30 seconds. Try again?");
	    continue;
	}
	rsize = read(data_socket, (char*)buf + offset, size);
	if (rsize < 0) {
	    logmsg(L_ERR, errno, "cannot get entry");
	    return -1;
	}
	offset += rsize;
	size -= rsize;
    } while (size > 0);
    return offset;
}

int
get_entry(DictEntry *entry, int num)
{
    int cnt, n;
    size_t size;
    int rsize;
    
    rsize = read_data(&n, sizeof(n));
    if (rsize != sizeof(n)) {
	logmsg(L_ERR, errno, "cannot get size");
	return -1;
    }
    cnt = htonl(n);
    if (cnt > num) {
	logmsg(L_ERR, 0,
	       "get_entry(): reported count greater than requested (%d, %d)",
	       cnt, num);
	cnt = num;/*FIXME*/
    }
    
    size = cnt * sizeof(*entry);

    read_data(entry, size);

    for (n = 0; n < cnt; n++, entry++) {
	entry->bushu = ntohs(entry->bushu);               
	entry->numstrokes = ntohs(entry->numstrokes);
	entry->Qindex = ntohs(entry->Qindex);
	entry->skip = ntohl(entry->skip);
	entry->Jindex = ntohs(entry->Jindex);              
	entry->Uindex = ntohs(entry->Uindex);		
	entry->Nindex = ntohs(entry->Nindex);		
	entry->Hindex = ntohs(entry->Hindex);		
	entry->frequency = ntohs(entry->frequency);
	entry->grade_level = entry->grade_level;
	entry->refcnt = ntohl(entry->refcnt);                 
	entry->english = ntohl(entry->english);		
	entry->pronunciation = ntohl(entry->pronunciation);	
	entry->kanji = ntohl(entry->kanji);		
	entry->pinyin = ntohl(entry->pinyin);
    }
    return cnt;
}

int
get_recno(int *cnt_ptr, Recno *data)
{
    unsigned size;
    int i, cnt;
    
    size = read_data(&cnt, sizeof(cnt));
    if (size != sizeof(cnt)) {
	logmsg(L_ERR, errno, "cannot get entry");
	return -1;
    }
    *cnt_ptr = ntohl(cnt);

    size = *cnt_ptr * sizeof(Recno);
    read_data(data, size);
    
    for (i = 0; i < *cnt_ptr; i++) 
	data[i] = ntohl(data[i]);

    return 0;
}


char *
get_string(Offset offset)
{
    int len, nlen;
    static char buffer[2048];

    if (offset == 0)
	return "";
    
    sendf("STR %lu", offset);
    if (get_reply() != RC_OK) {
	logmsg(L_ERR, 0,
	       "cannot get string. Server replied: %s", reply_buf);
	return "N/A";
    }

    if (read_data(&nlen, sizeof(nlen)) != sizeof(nlen)) {
	logmsg(L_ERR, errno, "cannot get string. Not enough data in socket.");
	return "N/A";
    }
    len = ntohl(nlen);
    if (len < sizeof(buffer)) {
	if (read_data(buffer, len) != len) {
	    logmsg(L_ERR, errno, 
	           "cannot get string. Not enough data in socket.");
	    return "N/A";
	}
    } else {
	/*FIXME: handle this case properly */
    }
    return buffer;
}


void
drain_input()
{
    char buf[256];
    int rdbytes;
    fd_set rfd;
    struct timeval timeout;
    int rc = 0;
    
    do {
	FD_ZERO(&rfd);
	FD_SET(sockfd, &rfd);

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	rc = select(sockfd + 1, &rfd, NULL, NULL, &timeout);
	if (rc < 1) {
	    break;
	} else
	    rc = 0;

	rdbytes = read(sockfd, buf, sizeof(buf) - 1);
    } while (rdbytes > 0);
}











