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

dictdir=@dictdir@
inputdictpath=@inputdictpath@

ACLOCAL_AMFLAGS = -I m4

SUBDIRS=include utils gnu lib libltdl dictd makedict
#FIXME: add these when ready client fonts

dict:
	$(top_builddir)/makedict/makedict -cv -o$(dictdir) -d$(inputdictpath)

distcleancheck_listfiles = find . -type f -not -name 'bushu.pcf.gz' -print
