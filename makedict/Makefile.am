# This file is part of Dico.
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

dictdir=@dictdir@
inputdictpath=@inputdictpath@
bin_PROGRAMS=makedict
makedict_SOURCES=\
 compile.c\
 index.c\
 main.c\
 makedict.h\
 makedict.c\
 romaji.c\
 utils.c


LDADD=../lib/libdico.a  ../gnu/libgnu.a ../lib/libextra.a @DB_LIBS@
AM_CFLAGS=-DDICTPATH=\"$(dictdir)\" -DINPUTDICTPATH=\"$(inputdictpath)\"
INCLUDES=-I$(top_srcdir)/include -I$(top_srcdir)/gnu @DB_INCLUDES@

SUFFIXES=.opt .c

.opt.c:
	m4 -s $(top_srcdir)/utils/getopt.m4 $< | sed '1d' > $@

BUILT_SOURCES=makedict.c 
EXTRA_DIST=makedict.opt 

