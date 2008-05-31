# This file is part of Dico
# Copyright (C) 1998-2000, 2008 Sergey Poznyakoff
#
# Dico is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# Dico is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Dico.  If not, see <http://www.gnu.org/licenses/>.

bin_PROGRAMS=dictd
dictd_SOURCES=\
 auth.c\
 capa.c\
 cmdline.c\
 commands.c\
 config-format.c\
 config-gram.y\
 config-lex.l\
 dbtext.c\
 dictd.c\
 fdio.c\
 lev.c\
 logstr.c\
 loader.c\
 main.c\
 mime.c\
 ostream.c\
 pp.c\
 regex.c\
 server.c\
 stat.c\
 udb.c\
 xscript.c

noinst_HEADERS=dictd.h config-gram.h

incdir=$(pkgdatadir)/$(VERSION)/include
inc_DATA = $(PP_SETUP_FILE)

AM_CFLAGS = \
 -DSYSCONFIG=\"$(sysconfdir)\"\
 -DDICO_MODDIR=\"$(DICO_MODDIR)\"\
 -DDEFAULT_VERSION_INCLUDE_DIR=\"$(incdir)\"\
 -DDEFAULT_INCLUDE_DIR=\"$(pkgdatadir)/include\"\
 -DDEFAULT_PREPROCESSOR="$(DEFAULT_PREPROCESSOR)"

AM_YFLAGS = -dtv
AM_LFLAGS = -dvp

LDADD=\
 ../lib/libxdico.a\
 ../gnu/libgnu.a\
 ../lib/libextra.a\
 ../lib/libdico.la\
 @LIBLTDL@  

INCLUDES=-I$(top_srcdir)/include -I$(top_srcdir)/gnu @LTDLINCL@
# dictd_DEPENDENCIES=@LTDLDEPS@

config-lex.c: config-gram.h
config-gram.h: config-gram.c

SUFFIXES=.opt .c

.opt.c:
	m4 -s $(top_srcdir)/utils/getopt.m4 $< | sed '1d' > $@

BUILT_SOURCES=cmdline.c 
EXTRA_DIST=cmdline.opt pp-setup
