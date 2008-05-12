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
 fltstream.c\
 linebuf.c\
 list.c\
 strat.c\
 stream.c\
 xlat.c\
 url.c\
 utf8.c

libxdico_a_SOURCES=\
 diag.c\
 iputil.c\
 jiscvt.c\
 timer.c\
 userprivs.c\
 util.c

libextra_a_SOURCES=xalloc-die.c

noinst_HEADERS=\
 cursor.xbm\
 bushu.h

EXTRA_DIST=bushu.dat
