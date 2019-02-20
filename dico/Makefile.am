# This file is part of GNU Dico
# Copyright (C) 1998-2019 Sergey Poznyakoff
#
# GNU Dico is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# GNU Dico is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with GNU Dico.  If not, see <http://www.gnu.org/licenses/>.

bin_PROGRAMS = dico
dico_SOURCES = \
 autologin.c\
 cmdline.c\
 connect.c\
 dico-priv.h\
 dico.c\
 func.c\
 lookup.c\
 pager.c\
 saslauth.c\
 shell.c

LDADD=\
 ../xdico/libxdico.la\
 ../lib/libdico.la\
 $(READLINE_LIBS)\
 @LIBINTL@ @LIBICONV@\
 @LIBDICOSASL@\
 @GSASL_LIBS@

AM_CPPFLAGS=\
 -DDEFAULT_DICT_SERVER="\"$(DEFAULT_DICT_SERVER)\""\
 -I${srcdir}\
 @DICO_PROG_INCLUDES@

SUFFIXES=.opt .c

.opt.c:
	$(AM_V_GEN)m4 -s $(top_srcdir)/@GRECS_SUBDIR@/build-aux/getopt.m4 $< > $@

BUILT_SOURCES=cmdline.c 
EXTRA_DIST=cmdline.opt 
