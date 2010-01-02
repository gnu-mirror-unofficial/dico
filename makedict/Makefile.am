# This file is part of GNU Dico.
# Copyright (C) 1998, 1999, 2000, 2008, 2010 Sergey Poznyakoff
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

JDICT_PROGS=makedict

bin_PROGRAMS=@BUILD_JDICT_PROGS@
EXTRA_PROGRAMS=makedict

makedict_SOURCES=\
 compile.c\
 index.c\
 main.c\
 makedict.h\
 makedict.c\
 romaji.c\
 utils.c


LDADD=\
 ../lib/libxdico.a\
 ../gnu/libgnu.a\
 ../lib/libextra.a\
 ../lib/libdico.la\
 @DB_LIBS@

INCLUDES=-I$(top_srcdir)/include -I$(top_srcdir)/gnu @DB_INCLUDES@

SUFFIXES=.opt .c

.opt.c:
	m4 -s $(top_srcdir)/utils/getopt.m4 $< | sed '1d' > $@

BUILT_SOURCES=makedict.c 
EXTRA_DIST=makedict.opt 

