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

SUBDIRS=. tests
bin_PROGRAMS=dicod
dicod_SOURCES=\
 accesslog.c\
 acl.c\
 alias.c\
 auth.c\
 capa.c\
 ckpass.c\
 cmdline.c\
 commands.c\
 database.c\
 dbtext.c\
 dicod.c\
 gsasl.c\
 ident.c\
 lang.c\
 lev.c\
 lint.c\
 loader.c\
 main.c\
 markup.c\
 mime.c\
 ostream.c\
 regex.c\
 result.c\
 server.c\
 stat.c\
 stratcl.c\
 xidle.c\
 xversion.c\
 virtual.c

noinst_HEADERS=dicod.h

AM_CPPFLAGS = \
 -DSYSCONFIG=\"$(sysconfdir)\"\
 -DLOCALSTATEDIR=\"$(localstatedir)\"\
 -DDICO_MODDIR=\"$(DICO_MODDIR)\"\
 -DDEFAULT_VERSION_INCLUDE_DIR=\"$(DEFAULT_VERSION_INCLUDE_DIR)\"\
 -DDEFAULT_INCLUDE_DIR=\"$(DEFAULT_INCLUDE_DIR)\"\
 -DDEFAULT_PREPROCESSOR="$(DEFAULT_PREPROCESSOR)"\
 -DDEFAULT_PIDFILE_NAME=\"$(DEFAULT_PIDFILE_NAME)\"\
 -I$(srcdir)\
 @DICO_PROG_INCLUDES@ @LTDLINCL@

AM_YFLAGS = -dtv
AM_LFLAGS = -dvp

LDADD=\
 ../xdico/libxdico.la\
 ../lib/libdico.la\
 @LIBINTL@ @LIBICONV@\
 @LIBDICOSASL@\
 @GSASL_LIBS@\
 @LIBLTDL@  

# dicod_DEPENDENCIES=@LTDLDEPS@

SUFFIXES=.opt .c

.opt.c:
	$(AM_V_GEN)m4 -s $(top_srcdir)/@GRECS_SUBDIR@/build-aux/getopt.m4 $< > $@

BUILT_SOURCES=cmdline.c 
EXTRA_DIST=cmdline.opt pp-setup

incdir=$(pkgdatadir)/$(VERSION)/include
inc_DATA = pp-setup
