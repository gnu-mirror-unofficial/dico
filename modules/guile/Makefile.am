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

moddir=@DICO_MODDIR@

DICO_GUILE_MODULES=guile.la
mod_LTLIBRARIES=guile.la

guile_la_SOURCES = guile.c
guile_la_LIBADD = ../../lib/libdico.la @GUILE_LIBS@
AM_LDFLAGS = -module -avoid-version -no-undefined
INCLUDES = -I$(top_srcdir)/include -I$(top_builddir)/include @GUILE_INCLUDES@
