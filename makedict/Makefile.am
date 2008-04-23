# This file is part of Gjdict.
# Copyright (C) 1998-2000, 2008 Sergey Poznyakoff
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

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


LDADD=../gnu/libgnu.a ../lib/libgjdict.a @DB_LIBS@
AM_CFLAGS=-DDICTPATH=\"$(dictdir)\" -DINPUTDICTPATH=\"$(inputdictpath)\"
INCLUDES=-I$(top_srcdir)/include -I$(top_srcdir)/gnu @DB_INCLUDES@

SUFFIXES=.opt .c

.opt.c:
	m4 -s $(top_srcdir)/utils/getopt.m4 $< | sed '1d' > $@

BUILT_SOURCES=makedict.c 
EXTRA_DIST=makedict.opt 

