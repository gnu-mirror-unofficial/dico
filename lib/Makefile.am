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

SUBDIRS = . tests

AM_CPPFLAGS = \
 @DICO_LIB_CONFIG@\
 -I${top_srcdir}/include\
 -I${top_builddir}/include\
 -DLOCALEDIR=\"$(localedir)\"

lib_LTLIBRARIES = libdico.la

libdico_la_SOURCES=\
 argcv.c\
 assoc.c\
 base64.c\
 bsearch.c\
 crlfstr.c\
 dbgstream.c\
 diag.c\
 fdiostr.c\
 fdstream.c\
 fltstream.c\
 header.c\
 iostr.c\
 key.c\
 levenshtein.c\
 libi18n.c\
 list.c\
 linetrimstr.c\
 logstream.c\
 mapstream.c\
 markup.c\
 mergesort.c\
 parseopt.c\
 qp.c\
 soundex.c\
 strat.c\
 stream.c\
 tokenize.c\
 udb.c\
 url.c\
 utf8.c\
 utf8_is_alnum.c\
 utf8_is_space.c\
 util.c\
 xlat.c

libdico_la_LDFLAGS = -version-info 2:0:0
libdico_la_LIBADD = ../grecs/src/libgrecs.la

