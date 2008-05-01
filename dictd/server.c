/* This file is part of Gjdict.
   Copyright (C) 2008 Sergey Poznyakoff

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
#include <sys/types.h>
#include <sys/wait.h>

int *fdtab;
size_t fdcount;
int fdmax;

pid_t *childtab;
unsigned long num_children;

static int
address_family_to_domain (int family)
{
    switch (family) {
    case AF_UNIX:
	return PF_UNIX;

    case AF_INET:
	return PF_INET;

    default:
	abort();
    }
}

void
open_sockets()
{
    size_t i;
    dict_iterator_t itr;
    sockaddr_union_t *sp;
    struct stat st;
    int t;
    int socklen;
    char *p;
    
    fdcount = dict_list_count(listen_addr);
    if (fdcount == 0) {
	/* Provide defaults */
	struct sockaddr_in *sp = xmalloc(sizeof(*sp));
	
	if (!listen_addr)
	    listen_addr = dict_list_create();
	sp->sin_family = AF_INET;
	sp->sin_addr.s_addr = INADDR_ANY;
	sp->sin_port = htons(DICT_PORT);
	dict_list_append(listen_addr, sp);
	fdcount = 1;
    }
    fdtab = xcalloc(fdcount, sizeof fdtab[0]);
    fdmax = 0;
    itr = dict_iterator_create(listen_addr);
    for (i = 0, sp = dict_iterator_first(itr); sp;
	 sp = dict_iterator_next(itr)) {
	int fd = socket(address_family_to_domain(sp->s.sa_family),
			SOCK_STREAM, 0);
	if (fd == -1) {
	    logmsg(L_ERR, errno, "socket");
	    continue;
	}

	switch (sp->s.sa_family) {
	case AF_UNIX:
	    if (stat(sp->s_un.sun_path, &st)) {
		if (errno != ENOENT) {
		    logmsg(L_ERR, errno,
			   _("file %s exists but cannot be stat'd"),
			   sp->s_un.sun_path);
		    close(fd);
		    continue;
		}
	    } else if (!S_ISSOCK(st.st_mode)) {
		logmsg(L_ERR, 0,
		       _("file %s is not a socket"),
		       sp->s_un.sun_path);
		close(fd);
		continue;
	    } else if (unlink(sp->s_un.sun_path)) {
		logmsg(L_ERR, errno,
		       _("cannot unlink file %s"),
		       sp->s_un.sun_path);
		close(fd);
		continue;
	    }
	    socklen = sizeof(sp->s_un);
	    break;
	
	case AF_INET:
	    t = 1;	 
	    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t));
	    socklen = sizeof(sp->s_in);
	    break;
	}

	if (bind(fd, &sp->s, socklen) == -1) {
	    p = sockaddr_to_astr(&sp->s, socklen);
	    logmsg(L_ERR, errno, ("cannot bind to %s"), p);
	    free(p);
	    close(fd);
	    continue;
	}

	if (listen(fd, 8) == -1) {
	    p = sockaddr_to_astr(&sp->s, socklen);
	    logmsg(L_ERR, errno, "cannot listen on %s", p);
	    free(p);
	    close(fd);
	    continue;
	}

	fdtab[i++] = fd;
	if (fd > fdmax)
	    fdmax = fd;
    }
    dict_iterator_destroy(&itr);
    fdcount = i;

    if (fdcount == 0)
	die(1, L_ERR, 0, _("No sockets opened"));
}

void
close_sockets()
{
    size_t i;

    for (i = 0; i < fdcount; i++)
	close(fdtab[i]);
    free(fdtab);
    fdtab = NULL;
    fdcount = 0;
    fdmax = 0;
}


unsigned long
find_pid(pid_t pid)
{
    unsigned long i;
    for (i = 0; i < max_children; i++)
	if (childtab[i] == pid) 
	    return i;
    return (unsigned long)-1;
}

void
register_child(pid_t pid)
{
    unsigned long i = find_pid(0);
    if (i != (unsigned long)-1)
	childtab[i] = pid;
    ++num_children;
}

static void
print_status (pid_t pid, int status, int expect_term)
{
    if (WIFEXITED(status)) {
	if (WEXITSTATUS(status) == 0)
	    logmsg(L_DEBUG, 0,
		   _("%lu exited successfully"),
		   (unsigned long) pid);
	else
	    logmsg(L_ERR, 0,
		   _("%lu failed with status %d"),
		   (unsigned long) pid,
		   WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
	int prio;
	
	if (expect_term && WTERMSIG(status) == SIGTERM)
	    prio = L_DEBUG;
	else
	    prio = L_ERR;
	logmsg(prio, 0,
	       _("%lu terminated on signal %d"),
	       (unsigned long) pid, WTERMSIG(status));
    } else if (WIFSTOPPED(status))
	logmsg(L_ERR, 0,
	       _("%lu stopped on signal %d"),
	       (unsigned long) pid, WSTOPSIG(status));
#ifdef WCOREDUMP
    else if (WCOREDUMP(status))
	logmsg(L_ERR, 0, _("%lu dumped core"), (unsigned long) pid);
#endif
    else
	logmsg(L_ERR, 0, _("%lu terminated with unrecognized status"),
	       (unsigned long) pid);
}


void
cleanup_children(int term)
{
    pid_t pid;
    int status;

    while ((pid = waitpid ((pid_t)-1, &status, WNOHANG)) > 0) {
	unsigned long i = find_pid(pid);
	if (i == (unsigned long)-1) {
	    logmsg(L_INFO, 0, "subprocess %lu finished",
		   (unsigned long) pid);
	} else {
	    print_status(pid, status, term);
	}
	childtab[i] = 0;
	--num_children;
    }
	    

}

void
stop_all(int sig)
{
    unsigned long i;
    for (i = 0; i < max_children; i++)
	if (childtab[i])
	    kill(sig, childtab[i]);
}

void
stop_children()
{
    int i;
    stop_all(SIGTERM);
    for (i = 0; i < shutdown_timeout; i++) {
	cleanup_children(1);
	if (num_children == 0)
	    return;
	sleep(1);
    }
    stop_all(SIGKILL);
}



/* Check whether pidfile NAME exists and if so, whether its PID is still
   active. Exit if it is. */
void
check_pidfile(char *name)
{
    unsigned long pid;
    FILE *fp = fopen(name, "r");

    if (!fp) {
	if (errno == ENOENT)
	    return;
	die(1, L_ERR, errno, _("Cannot open pidfile `%s'"), name);
    }
    if (fscanf(fp, "%lu", &pid) != 1) {
	logmsg(L_ERR, 0, _("Cannot get pid from pidfile `%s'"), name);
    } else {
	if (kill(pid, 0) == 0) {
	    die(1, L_ERR, 0,
		_("%s appears to run with pid %lu. "
		  "If it does not, remove `%s' and retry."),
		program_name,
		pid,
		name);
	}
    }
    fclose(fp);
    if (unlink(name)) 
	die(1, L_ERR, errno, _("Cannot unlink pidfile `%s'"), name);
}

void
create_pidfile(char *name)
{
    FILE *fp = fopen(name, "w");

    if (!fp) 
	die(1, L_ERR, errno, _("Cannot create pidfile `%s'"), name);
    fprintf(fp, "%lu", (unsigned long)getpid());
    fclose(fp);
}

void
remove_pidfile(char *name)
{
    if (unlink(name)) 
	logmsg(L_ERR, errno, _("Cannot unlink pidfile `%s'"), name);
}


int restart;
int stop;
int need_cleanup;

static RETSIGTYPE
sig_stop(int sig)
{
    stop = 1;
}

static RETSIGTYPE
sig_restart(int sig)
{
    restart = 1;
}

static RETSIGTYPE
sig_child(int sig)
{
    need_cleanup = 1;
}

int
handle_connection(int listenfd)
{
    sockaddr_union_t addr;
    int addrlen = sizeof addr;
    int connfd = accept(listenfd, (struct sockaddr *)&addr, &addrlen);
    int status = 0;
    
    if (connfd == -1) {
	if (errno == EINTR)
	    return -1;
	logmsg(L_ERR, errno, "accept");
	return -1;
	/*exit (EXIT_FAILURE);*/
    }

    /*FIXME: log_connection(&addr, addrlen);*/

    if (single_process) {
	int status;
	stream_t str;

	str = fd_stream_create(connfd, connfd);
	stream_set_buffer(str, lb_in, 512);
	stream_set_buffer(str, lb_out, 512);
	status = dictd_loop(str);
	stream_close(str);
    } else {
	pid_t pid = fork();
	if (pid == -1)
	    logmsg(LOG_ERR, errno, "fork");
	else if (pid == 0) {
	    /* Child.  */
	    stream_t str;
	    
	    close(listenfd);
	    
	    signal(SIGTERM, SIG_DFL);
	    signal(SIGQUIT, SIG_DFL);
	    signal(SIGINT, SIG_DFL);
	    signal(SIGCHLD, SIG_DFL);
	    signal(SIGHUP, SIG_DFL);
        
	    str = fd_stream_create(connfd, connfd);
	    stream_set_buffer(str, lb_in, 512);
	    stream_set_buffer(str, lb_out, 512);
	    status = dictd_loop(str);
	    stream_close(str);
	    exit(status);
	} else {
	    register_child(pid);
	}
    }
    close(connfd);
    return status;
}

int
server_loop()
{
    size_t i;
    fd_set fdset;

    FD_ZERO (&fdset);
    for (i = 0; i < fdcount; i++)
	FD_SET (fdtab[i], &fdset);
    
    for (;;) {
	int rc;
	fd_set rdset;

	if (need_cleanup) {
	    cleanup_children(0);
	    need_cleanup = 1;
	}

	if (num_children > max_children) {
	    logmsg(L_ERR, 0, _("too many children (%lu)"), num_children);
	    pause();
	    continue;
	}

	rdset = fdset;
	rc = select (fdmax + 1, &rdset, NULL, NULL, NULL);
	if (rc == -1 && errno == EINTR) {
	    if (stop || restart)
		break;
	    continue;
	} else if (rc < 0) {
	    logmsg(L_CRIT, errno, _("select error"));
	    return 1;
	}

	for (i = 0; i < fdcount; i++)
	    if (FD_ISSET(fdtab[i], &rdset))
		handle_connection(fdtab[i]);
    }
    return 0;
}


#if defined HAVE_SYSCONF && defined _SC_OPEN_MAX
# define getmaxfd() sysconf(_SC_OPEN_MAX)
#elif defined (HAVE_GETDTABLESIZE)
# define getmaxfd() getdtablesize()
#else
# define getmaxfd() 256
#endif


void
dictd_server(int argc, char **argv)
{
    int rc;
    
    logmsg(L_INFO, 0, _("%s started"), program_version);

    if (user_id && switch_to_privs(user_id, group_id, group_list))
	die(1, L_CRIT, 0, "exiting");

    if (!foreground)
	check_pidfile(pidfile_name);

    signal(SIGTERM, sig_stop);
    signal(SIGQUIT, sig_stop);
    signal(SIGINT, sig_stop);
    signal(SIGCHLD, sig_child);
    if (argv[0][0] != '/') {
	logmsg(L_WARN, 0, _("gjdict started without full file name"));
	logmsg(L_WARN, 0, _("restart (SIGHUP) will not work"));
	signal(SIGHUP, sig_stop);
    } else if (config_file[0] != '/') {
	logmsg(L_WARN, 0, _("configuration file is not given with a full file name"));
	logmsg(L_WARN, 0, _("restart (SIGHUP) will not work"));
	signal(SIGHUP, sig_stop);
    } else	
	signal(SIGHUP, sig_restart);

    if (!foreground) {
	if (daemon(0, 0) == -1) 
	    die(1, L_CRIT, errno, _("Cannot become a daemon"));

	create_pidfile(pidfile_name);
    }

    open_sockets();
    if (!single_process) 
	childtab = xcalloc(max_children, sizeof(childtab[0]));
			   
    rc = server_loop();

    stop_children();
    
    free(childtab);
    close_sockets();

    if (rc) 
	logmsg(L_NOTICE, errno, _("Exit code = %d, last error status"), rc);

    if (restart) {
	int i;
		
	logmsg(L_INFO, 0, _("gjdict restarting"));
	remove_pidfile(pidfile_name);
	for (i = getmaxfd(); i > 2; i--)
	    close(i);
	execv(argv[0], argv);
	die(127, L_ERR|L_CONS, errno, _("Cannot restart"));
    } else 
	logmsg(L_INFO, 0, _("gjdict terminating"));
    exit(rc);
}
    


    

	
    
	
	
