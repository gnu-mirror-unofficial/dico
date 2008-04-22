/* $Id$ */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <strings.h>
#include <wait.h>
#include <stdarg.h>
#include <gjdictd.h>
#include <server.h>

char myhostname[128];
int foreground = 0;
int verbose = 0;
char *portstr = "gjdict";
int port = GJDICT_PORT;
unsigned num_children;
unsigned max_children = 16;
int single_user;
long inactivity_timeout = 3600;
IPADDR ipaddr = INADDR_ANY;
int inetd;

char *dictpath = DICTPATH;
char msg2many[] = "500 too many sessions open. Try again later\r\n";
struct status status;

int daemon_mode;

void say(char *);
RETSIGTYPE sig_cleanup(int);
RETSIGTYPE sig_hup(int);
RETSIGTYPE sig_usr1(int);
RETSIGTYPE sig_usr2(int);
RETSIGTYPE sig_child(int);
void initsocket(int);
void get_port(void);
void run_daemon(void);

int
main(int argc, char **argv)
{
 
    set_program_name(argv[0]);
    set_log_printer(syslog_log_printer);

    get_options(argc, argv);
    
    gethostname(myhostname, sizeof(myhostname));

    initlog();
    initdict();
    initsearch();

    set_lo_ctl_addr();
    if (inetd) 
	return server(stdin, stdout);
    else
	run_daemon();
    return 0;
}

void
run_daemon()
{
    int len, result;
    fd_set rfds;
    struct sockaddr_in address;
    int listen_socket;
    int sock;
    int reuse_addr = 1;
    pid_t pid;
    extern struct sockaddr_in his_addr;

    get_port();
    
    signal(SIGTERM, sig_cleanup);
    signal(SIGINT, sig_cleanup);
    signal(SIGCHLD, sig_child);
    signal(SIGHUP, sig_hup);
    signal(SIGUSR1, sig_usr1);
    signal(SIGUSR2, sig_usr2);

    memset((char *) &address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(ipaddr);

    listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_socket < 0) {
	logmsg(L_ERR|L_CONS, errno, "socket");
	exit(1);
    }

    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR,
	       &reuse_addr, sizeof(reuse_addr));

    if (bind(listen_socket,
	     (struct sockaddr *) &address, sizeof(address)) < 0) {
	close(listen_socket);
	logmsg(L_ERR|L_CONS, errno, "bind");
	exit(1);
    }

    if (!foreground) {
	int fd;
	
	pid = fork();
	if (pid < 0) {
	    logmsg(L_ERR|L_CONS, errno, "cannot fork");
	    exit(1);
	}
	
	if (pid > 0) {
	    FILE *fp = fopen(PIDFILE, "w");
	    if (fp) {
		fprintf(fp, "%lu", (unsigned long) pid);
		fclose(fp);
	    } else {
		logmsg(L_ERR|L_CONS, errno, "cannot write pidfile %s",
		       PIDFILE);
	    }
	    logmsg(L_INFO|L_CONS, 0, "installed gjdict daemon at pid %lu",
	           (unsigned long) pid);
	    exit(0);
	}

	close(0);
	close(1);
	close(2);
	chdir("/");
	setsid();

	fd = open("/dev/console", O_WRONLY | O_NOCTTY);
	if (fd != 2) {
	    dup2(fd, 2);
	    close(fd);
	}

	daemon_mode = 1;
    } else
	console_log++;

    status.load_time = time(NULL);
    
    listen(listen_socket, 8);

    while (1) {
	FD_ZERO(&rfds);
	FD_SET(listen_socket, &rfds);
	
	result = select(listen_socket+1, &rfds, NULL, NULL, NULL);
	if (result == -1) {
	    if (errno == EINTR) 
		continue;
	    logmsg(L_ERR, errno, "select");
	    exit(1);
	}

	len = sizeof(his_addr);
	if ((sock = accept(listen_socket, (struct sockaddr *)&his_addr, &len)) < 0) {
	    logmsg(L_ERR, errno, "accept");
	    exit(1);
	}
	
	if (num_children > max_children) {
	    logmsg(L_ERR, 0, "rejecting service to %s: too many sessions open",
		   ip_hostname(ntohl(his_addr.sin_addr.s_addr)));
	    write(sock, msg2many, strlen(msg2many));
	    close(sock);
	    continue;
	}
	num_children++;

	if (set_fd_ctl_addr(sock) == 0) {
	    if (!single_user) {
		pid = fork();
		if (pid < 0) {
		    close(sock);
		    logmsg(L_ERR, errno, "cannot fork");
		    continue;
		}		
	    
		if (pid > 0) {
		    /* parent branch */
		    logmsg(L_INFO, 0, "PID %lu serving %s",
			   (unsigned long) pid, 
			   ip_hostname(ntohl(his_addr.sin_addr.s_addr)));
		    close(sock);
		    continue;
		}

		/* Child branch */
		close(listen_socket);
		result = server(fdopen(sock, "r"), fdopen(sock, "w"));
		exit(result);
	    } else /* single-process mode */
		result = server(fdopen(sock, "r"), fdopen(sock, "w"));
	}
	close(sock);
    }
}

void
get_port()
{
    if (isdigit(portstr[0])) {
	char *errpos;
	port = strtol(portstr, &errpos, 0);
	if (*errpos) {
	    logmsg(L_ERR|L_CONS, 0,
	           "error converting port number near %s", errpos);
	    exit(1);
	}
    } else {
	struct servent *serv = getservbyname(portstr, "tcp");

	if (!serv) {
	    logmsg(L_ERR|L_CONS, 0,
	           "service %s/tcp not found in /etc/services",
		   portstr);
	    exit(1);
	} else
	    port = ntohs(serv->s_port);	
	
    }
}


void
set_inactivity_timeout(char *str)
{
    long t, hrs, min, sec;
    char *p;
    
    hrs = min = sec = 0;
    t = strtol(str, &p, 10);
    switch (*p++) {
    case 'h':
	hrs = t;
	break;
    case 'm':
	min = t;
	break;
    case 0:
	p = NULL;
	/*FALLTHRU*/
    case 's':
	sec = t;
	break;
    default:
	logmsg(L_ERR|L_CONS, 0,
	       "wrong time format near `%s'", p-1);
	return;
    }

    if (p && *p) {
	t = strtol(p, &p, 10);
	switch (*p++) {
	case 0:
	    p = NULL;
	    /*FALLTHRU*/
	case 'm':
	    min = t;
	    break;
	case 's':
	    sec = t;
	    break;
	default:
	    logmsg(L_ERR|L_CONS, 0,
		   "wrong time format near `%s'", p-1);
	    return;
	}
    
	if (p && *p) {
	    t = strtol(p, &p, 10);
	    switch (*p) {
	    case 0:
		/*FALLTHRU*/
	    case 's':
		sec = t;
		break;
	    default:
		logmsg(L_ERR|L_CONS, 0,
		       "wrong time format near `%s'", p);
		return;
	    }
	}
    }
    inactivity_timeout = (hrs*60 + min)*60 + sec;
}

/*ARGSUSED*/
RETSIGTYPE
sig_child(int num)
{
    pid_t childpid;
    int status;

    signal(SIGCHLD, sig_child);    
    childpid = waitpid((pid_t)-1, &status, 0);
    if (WIFEXITED(status))
	status = WEXITSTATUS(status);
    logmsg(L_INFO, 0, "child %lu exited with status %d", 
           (unsigned long) childpid, status);
    num_children--;
}

/*ARGSUSED*/
RETSIGTYPE
sig_cleanup(int num)
{
    unlink(PIDFILE);
    logmsg(L_INFO, 0, "exited on signal %d", num);
    exit(0);
}

/*ARGSUSED*/
RETSIGTYPE
sig_hup(int num)
{
    signal(SIGHUP, sig_hup);
}

/*ARGSUSED*/
RETSIGTYPE
sig_usr1(int num)
{
    signal(SIGUSR1, sig_usr1);
}

/*ARGSUSED*/
RETSIGTYPE
sig_usr2(int num)
{
    signal(SIGUSR2, sig_usr2);
}

char usage[] = "\
usage: gjdictd ...\
\n";

char version[] =
"gjdictd " VERSION "\n"
"    Copyright (C) 1999, 2008 Sergey Poznyakoff\n"
"Compilation options:\n"
#ifdef DEBUG
"    DEBUG\n"
#endif
#ifdef GNU_STYLE_OPTIONS
"    GNU_STYLE_OPTIONS\n"
#endif
"\n";

char licence[] = "\
gjdictd " VERSION "\n\
    Copyright (C) 1999 Gray\n\
\n\
    This program is free software; you can redistribute it and/or modify\n\
    it under the terms of the GNU General Public License as published by\n\
    the Free Software Foundation; either version 2 of the License, or\n\
    (at your option) any later version.\n\
\n\
    This program is distributed in the hope that it will be useful,\n\
    but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
    GNU General Public License for more details.\n\
\n\
    You should have received a copy of the GNU General Public License\n\
    along with this program; if not, write to the Free Software\n\
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n\
"; 


void
say(char *s)
{
    printf("%s", s);
    exit(0);
}










