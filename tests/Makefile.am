# This file is part of GNU Dico
# Copyright (C) 2008, 2010, 2012 Sergey Poznyakoff
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

SUFFIXES = .cfin .conf

noinst_DATA=dicod.conf

CLEANFILES=dicod.conf

.cfin.conf:
	$(AM_V_GEN)sed 's|<BUILDDIR>|'`cd $(top_builddir); pwd`'|g;s|<SRCDIR>|'`cd $(top_srcdir); pwd`'|g' \
		$< > $@

LDADD=\
 ../lib/libxdico.a\
 ../gnu/libgnu.a\
 ../lib/libextra.a\
 ../lib/libdico.la\
 @LIBINTL@ @LIBICONV@
INCLUDES=-I$(top_srcdir)/include -I$(top_srcdir)/gnu -I../gnu

EXTRA_DIST=dicod.cfin 
