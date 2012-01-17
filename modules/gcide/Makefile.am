# This file is part of GNU Dico
# Copyright (C) 2012 Sergey Poznyakoff
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

moddir=@DICO_MODDIR@

mod_LTLIBRARIES=gcide.la
noinst_LTLIBRARIES=libgcide.la
libgcide_la_SOURCES=webchr.c ent.c markup.l

gcide_la_SOURCES = gcide.c idx.c 
gcide_la_LIBADD = ./libgcide.la ../../lib/libdico.la $(LIBICONV)
gcide_la_LDFLAGS = -module -avoid-version -no-undefined

libexec_PROGRAMS=idxgcide 
idxgcide_SOURCES=idxgcide.l 

noinst_PROGRAMS=degcide
degcide_LDADD = ./libgcide.la ../../lib/libdico.la $(LIBICONV)

noinst_HEADERS = gcide.h

AM_LFLAGS=-d
AM_CPPFLAGS=-DLIBEXECDIR=\"${libexecdir}\"

INCLUDES = -I$(top_srcdir)/include -I$(top_builddir)/include @GRECS_INCLUDES@
idxgcide_LDADD = ./libgcide.la ../../lib/libdico.la $(LIBICONV)