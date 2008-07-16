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

INCLUDES = -I${top_srcdir}/include -I${top_srcdir}/gnu

lib_LTLIBRARIES = libdico.la

noinst_LIBRARIES=libxdico.a libextra.a

libdico_la_SOURCES=\
 argcv.c\
 assoc.c\
 base64.c\
 diag.c\
 fdiostr.c\
 fdstream.c\
 fltstream.c\
 iostr.c\
 levenshtein.c\
 linebuf.c\
 list.c\
 logstream.c\
 mapstream.c\
 parseopt.c\
 qp.c\
 soundex.c\
 strat.c\
 stream.c\
 xlat.c\
 url.c\
 utf8.c\
 util.c

libdico_la_LDFLAGS = -version-info 0:0:0

libxdico_a_SOURCES=\
 iputil.c\
 jiscvt.c\
 timer.c\
 tokenize.c\
 userprivs.c\
 xscript.c\
 xstream.c\
 xutil.c

libextra_a_SOURCES=xalloc-die.c

noinst_HEADERS=\
 cursor.xbm\
 bushu.h

EXTRA_DIST=bushu.dat
