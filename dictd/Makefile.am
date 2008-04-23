# This file is part of Gjdict
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

bin_PROGRAMS=dictd
dictd_SOURCES=\
 cmdline.c\
 main.c
noinst_HEADERS=dictd.h

LDADD=../lib/libgjdict.a @DB_LIBS@
INCLUDES=-I$(top_srcdir)/include -I$(top_srcdir)/gnu @DB_INCLUDES@

SUFFIXES=.opt .c

.opt.c:
	m4 -s $(top_srcdir)/utils/getopt.m4 $< | sed '1d' > $@

BUILT_SOURCES=cmdline.c 
EXTRA_DIST=cmdline.opt 
