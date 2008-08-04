/* This file is part of GNU Dico.
   Copyright (C) 2008 Sergey Poznyakoff

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
#include <sys/wait.h>

struct dicod_server {
    int fd;               /* Socket descriptor */
    struct sockaddr *addr;
    int addrlen;
};

struct dicod_server *srvtab;
size_t srvcount;
int fdmax;

pid_t *childtab;
unsigned long num_children;
unsigned long total_forks;

struct sockaddr server_addr;
int server_addrlen;

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
    dico_iterator_t itr;
    sockaddr_union_t *sp;
    struct stat st;
    int t;
    int socklen;
    char *p;
    
    srvcount = dico_list_count(listen_addr);
    if (srvcount == 0) {
	/* Provide defaults */
	struct sockaddr_in *sp = xmalloc(sizeof(*sp));
	
	if (!listen_addr)
	    listen_addr = xdico_list_create();
	sp->sin_family = AF_INET;
	sp->sin_addr.s_addr = INADDR_ANY;
	sp->sin_port = htons(DICO_DICT_PORT);
	xdico_list_append(listen_addr, sp);
	srvcount = 1;
    }
    srvtab = xcalloc(srvcount, sizeof srvtab[0]);
    fdmax = 0;
    itr = xdico_iterator_create(listen_addr);
    for (i = 0, sp = dico_iterator_first(itr); sp;
	 sp = dico_iterator_next(itr)) {
	int fd = socket(address_family_to_domain(sp->s.sa_family),
			SOCK_STREAM, 0);
	if (fd == -1) {
	    dico_log(L_ERR, errno, "socket");
	    continue;
	}

	switch (sp->s.sa_family) {
	case AF_UNIX:
	    if (stat(sp->s_un.sun_path, &st)) {
		if (errno != ENOENT) {
		    dico_log(L_ERR, errno,
			   _("file %s exists but cannot be stat'd"),
			   sp->s_un.sun_path);
		    close(fd);
		    continue;
		}
	    } else if (!S_ISSOCK(st.st_mode)) {
		dico_log(L_ERR, 0,
		       _("file %s is not a socket"),
		       sp->s_un.sun_path);
		close(fd);
		continue;
	    } else if (unlink(sp->s_un.sun_path)) {
		dico_log(L_ERR, errno,
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
	    dico_log(L_ERR, errno, ("cannot bind to %s"), p);
	    free(p);
	    close(fd);
	    continue;
	}

	if (listen(fd, 8) == -1) {
	    p = sockaddr_to_astr(&sp->s, socklen);
	    dico_log(L_ERR, errno, "cannot listen on %s", p);
	    free(p);
	    close(fd);
	    continue;
	}

	srvtab[i].addr = &sp->s;
	srvtab[i].addrlen = socklen;
	srvtab[i].fd = fd;
	i++;
	if (fd > fdmax)
	    fdmax = fd;
    }
    dico_iterator_destroy(&itr);
    srvcount = i;

    if (srvcount == 0)
	dico_die(1, L_ERR, 0, _("No sockets opened"));
}

void
close_sockets()
{
    size_t i;

    for (i = 0; i < srvcount; i++) 
	close(srvtab[i].fd);
    free(srvtab);
    srvtab = NULL;
    srvcount = 0;
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
    ++total_forks;
}

static void
print_status(pid_t pid, int status, int expect_term)
{
    if (WIFEXITED(status)) {
	if (WEXITSTATUS(status) == 0)
	    dico_log(L_DEBUG, 0,
		   _("%lu exited successfully"),
		   (unsigned long) pid);
	else
	    dico_log(L_ERR, 0,
		   _("%lu failed with status %d"),
		   (unsigned long) pid,
		   WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
	int prio;
	
	if (expect_term && WTERMSIG(status) == SIGTERM)
	    prio = L_DEBUG;
	else
	    prio = L_ERR;
	dico_log(prio, 0,
	       _("%lu terminated on signal %d"),
	       (unsigned long) pid, WTERMSIG(status));
    } else if (WIFSTOPPED(status))
	dico_log(L_ERR, 0,
	       _("%lu stopped on signal %d"),
	       (unsigned long) pid, WSTOPSIG(status));
#ifdef WCOREDUMP
    else if (WCOREDUMP(status))
	dico_log(L_ERR, 0, _("%lu dumped core"), (unsigned long) pid);
#endif
    else
	dico_log(L_ERR, 0, _("%lu terminated with unrecognized status"),
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
	    dico_log(L_INFO, 0, "subprocess %lu finished",
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

    if (!childtab)
	return;
    for (i = 0; i < max_children; i++)
	if (childtab[i])
	    kill(sig, childtab[i]);
}

void
stop_children()
{
    int i;
    if (!childtab)
	return;
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
	dico_die(1, L_ERR, errno, _("Cannot open pidfile `%s'"), name);
    }
    if (fscanf(fp, "%lu", &pid) != 1) {
	dico_log(L_ERR, 0, _("Cannot get pid from pidfile `%s'"), name);
    } else {
	if (kill(pid, 0) == 0) {
	    dico_die(1, L_ERR, 0,
		_("%s appears to run with pid %lu. "
		  "If it does not, remove `%s' and retry."),
		dico_program_name,
		pid,
		name);
	}
    }
    fclose(fp);
    if (unlink(name)) 
	dico_die(1, L_ERR, errno, _("Cannot unlink pidfile `%s'"), name);
}

void
create_pidfile(char *name)
{
    FILE *fp = fopen(name, "w");

    if (!fp) 
	dico_die(1, L_ERR, errno, _("Cannot create pidfile `%s'"), name);
    fprintf(fp, "%lu", (unsigned long)getpid());
    fclose(fp);
}

void
remove_pidfile(char *name)
{
    if (unlink(name)) 
	dico_log(L_ERR, errno, _("Cannot unlink pidfile `%s'"), name);
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

struct sockaddr client_addr;
int client_addrlen;

#define ACCESS_DENIED_MSG "530 Access denied\r\n"
#define TEMP_FAIL_MSG "420 Server temporarily unavailable\r\n"
#define SWRITE(fd, s) write(fd, s, sizeof(s)-1)

int
handle_connection(int n)
{
    int status = 0;
    int connfd;
    int listenfd = srvtab[n].fd;

    server_addr = *srvtab[n].addr;
    server_addrlen = srvtab[n].addrlen;
    
    client_addrlen = sizeof(client_addr);
    connfd = accept(listenfd, &client_addr, &client_addrlen);

    if (connfd == -1) {
	if (errno == EINTR)
	    return -1;
	dico_log(L_ERR, errno, "accept");
	return -1;
	/*exit (EXIT_FAILURE);*/
    }

    if (dicod_acl_check(connect_acl, 1) == 0) {
	char *p = sockaddr_to_astr(&client_addr, client_addrlen);
	dico_log(L_NOTICE, 0,
		 _("connection from %s denied"),
		 p);
	free(p);
	SWRITE(connfd, ACCESS_DENIED_MSG);
	close(connfd);
	return 0;
    }
    
    /*FIXME: log_connection(&addr, addrlen);*/

    if (single_process) {
	int status;
	dico_stream_t str;

	str = dico_fd_io_stream_create(connfd, connfd);
	dico_stream_set_buffer(str, dico_buffer_line, DICO_MAX_BUFFER);
	dico_stream_set_buffer(str, dico_buffer_line, DICO_MAX_BUFFER);
	status = dicod_loop(str);
	dico_stream_close(str);
	dico_stream_destroy(&str);
    } else {
	pid_t pid = fork();
	if (pid == -1) {
	    dico_log(L_ERR, errno, "fork");
	    SWRITE(connfd, TEMP_FAIL_MSG);
	} else if (pid == 0) {
	    /* Child.  */
	    dico_stream_t str;
	    
	    close(listenfd);
	    
	    signal(SIGTERM, SIG_DFL);
	    signal(SIGQUIT, SIG_DFL);
	    signal(SIGINT, SIG_DFL);
	    signal(SIGCHLD, SIG_DFL);
	    signal(SIGHUP, SIG_DFL);
        
	    str = dico_fd_io_stream_create(connfd, connfd);
	    dico_stream_set_buffer(str, lb_in, 512);
	    dico_stream_set_buffer(str, lb_out, 512);
	    status = dicod_loop(str);
	    dico_stream_close(str);
	    dico_stream_destroy(&str);
	    exit(status);
	} else {
	    register_child(pid);
	}
    }
    close(connfd);
    return status;
}

static int
pre_restart_lint_internal()
{
    pid_t pid;
    time_t ts;
    pid = fork();
    if (pid == 0)
	run_lint();
    else if (pid == -1) {
	dico_log(L_ERR, errno, _("fork failed"));
	return 1;
    }
    /* Master process */
    ts = time(NULL);
    while (1) {
	int status;
	pid_t n = waitpid(pid, &status, WNOHANG);
	if (n == pid) {
	    /* Child exited, examine status */
	    if (WIFEXITED(status)) {
		if (WEXITSTATUS(status) == 0) {
		    /* OK to restart */
		    return 0;
		} else {
		    dico_log(L_NOTICE, 0,
			     _("refusing to restart due to errors in "
			       "configuration file"));
		    return 1;
		}
	    } else {
		dico_log(L_NOTICE, 0,
			 _("refusing to restart due to unexpected "
			   "return status of configuration checker"));
		print_status(pid, status, 0);
		return 1;
	    }
	} else if (pid == 0) {
	    if (time(NULL) - ts > 5) {
		dico_log(L_NOTICE, 0,
			 _("refusing to restart: "
			   "configuration checker did not exit within "
			   "5 seconds"));
		kill(pid, SIGKILL);
		break;
	    }
	} else if (errno == EINTR || errno == EAGAIN)
	    continue;
	else {
	    dico_log(L_ERR, errno, _("waitpid failed"));
	    dico_log(L_NOTICE, 0, _("refusing to restart"));
	    kill(pid, SIGKILL);
	    break;
	}
    }
    return 1;
}

static int
pre_restart_lint()
{
    int rc;
    RETSIGTYPE (*sf)(int);
    
    sf = signal(SIGCHLD, SIG_DFL);
    rc = pre_restart_lint_internal();
    signal(SIGCHLD, sf);
    return rc;
}

int
server_loop()
{
    size_t i;
    fd_set fdset;

    FD_ZERO(&fdset);
    for (i = 0; i < srvcount; i++)
	FD_SET(srvtab[i].fd, &fdset);
    
    for (;;) {
	int rc;
	fd_set rdset;

	if (need_cleanup) {
	    cleanup_children(0);
	    need_cleanup = 1;
	}

	if (num_children > max_children) {
	    dico_log(L_ERR, 0, _("too many children (%lu)"), num_children);
	    pause();
	    continue;
	}

	rdset = fdset;
	rc = select (fdmax + 1, &rdset, NULL, NULL, NULL);
	if (rc == -1 && errno == EINTR) {
	    if (stop)
		break;
	    else if (restart) {
		if (pre_restart_lint() == 0)
		    break;
		else
		    restart = 0;
	    }
	    continue;
	} else if (rc < 0) {
	    dico_log(L_CRIT, errno, _("select error"));
	    return 1;
	}

	for (i = 0; i < srvcount; i++)
	    if (FD_ISSET(srvtab[i].fd, &rdset))
		handle_connection(i);
    }
    return 0;
}

void
dicod_server(int argc, char **argv)
{
    int rc;
    
    dico_log(L_INFO, 0, _("%s started"), program_version);

    if (user_id && switch_to_privs(user_id, group_id, group_list))
	dico_die(1, L_CRIT, 0, "exiting");

    if (!foreground)
	check_pidfile(pidfile_name);

    signal(SIGTERM, sig_stop);
    signal(SIGQUIT, sig_stop);
    signal(SIGINT, sig_stop);
    signal(SIGCHLD, sig_child);
    if (argv[0][0] != '/') {
	dico_log(L_WARN, 0, _("dicod started without full file name"));
	dico_log(L_WARN, 0, _("restart (SIGHUP) will not work"));
	signal(SIGHUP, sig_stop);
    } else if (config_file[0] != '/') {
	dico_log(L_WARN, 0, _("configuration file is not given with a full file name"));
	dico_log(L_WARN, 0, _("restart (SIGHUP) will not work"));
	signal(SIGHUP, sig_stop);
    } else	
	signal(SIGHUP, sig_restart);

    if (!foreground) {
	if (daemon(0, 0) == -1) 
	    dico_die(1, L_CRIT, errno, _("Cannot become a daemon"));

	create_pidfile(pidfile_name);
    }

    open_sockets();
    if (!single_process) 
	childtab = xcalloc(max_children, sizeof(childtab[0]));
			   
    rc = server_loop();

    stop_children();
    free(childtab);
    dicod_server_cleanup();
    close_sockets();

    if (rc) 
	dico_log(L_NOTICE, errno, _("Exit code = %d, last error status"), rc);

    remove_pidfile(pidfile_name);

    if (restart) {
	int i;
		
	dico_log(L_INFO, 0, _("%s restarting"), program_version);
	for (i = getmaxfd(); i > 2; i--)
	    close(i);
	execv(argv[0], argv);
	dico_die(127, L_ERR|L_CONS, errno, _("Cannot restart"));
    } else 
	dico_log(L_INFO, 0, _("%s terminating"), program_version);
    exit(rc);
}
    


    

	
    
	
	
