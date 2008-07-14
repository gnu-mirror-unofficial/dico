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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <xalloc.h>
#include <ctype.h>
#include <syslog.h>
#include <inttypes.h>
#include <limits.h>
#include <size_max.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <ltdl.h>

#include <xdico.h>
#include <inttostr.h>
#include <c-strcase.h>
#include <gettext.h>

enum dico_client_mode {
    mode_define,
    mode_match,
    mode_dbs,
    mode_strats,
    mode_help,
    mode_info,
    mode_server
};

#define DICO_CLIENT_ID PACKAGE_STRING 

extern char *host;
extern int port;
extern char *database;
extern char *strategy;
extern char *user;
extern char *key;
extern char *client;
extern struct dico_request req;
extern enum dico_client_mode mode;

extern void get_options (int argc, char *argv[], int *index);
