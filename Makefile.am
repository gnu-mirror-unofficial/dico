# This file is part of GNU Dico
# Copyright (C) 1998-2000, 2008, 2009 Sergey Poznyakoff
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

dicodir=@dicodir@
inputdictpath=@inputdictpath@

ACLOCAL_AMFLAGS = -I m4 -I am

SUBDIRS=include utils gnu lib libltdl dicod modules dico tests doc po
#FIXME: add these when ready: makedict client fonts

EXTRA_DIST = ChangeLog.2008 app dicoweb

gen_start_date = 2008-05-05
.PHONY: make-ChangeLog
make-ChangeLog: 
	if test -d .git; then                                           \
	  $(top_srcdir)/build-aux/gitlog-to-changelog                   \
	    --since=$(gen_start_date) |                                 \
            sed '/<unknown>$$/d' | fmt -s > $(distdir)/cl-t;            \
	  echo "" >> $(distdir)/cl-t;                                 \
	  echo "Local Variables:" >> $(distdir)/cl-t;                   \
	  echo "mode: change-log" >> $(distdir)/cl-t;                   \
	  echo "version-control: never"  >> $(distdir)/cl-t;            \
	  echo "buffer-read-only: t" >> $(distdir)/cl-t;                \
	  echo "End:" >> $(distdir)/cl-t;                               \
	  rm -f $(distdir)/ChangeLog;                                   \
	  mv $(distdir)/cl-t $(distdir)/ChangeLog;                      \
	fi

dist-hook: make-ChangeLog

.PHONY: ChangeLog
ChangeLog: 
	if test -d .git; then                                           \
	  $(top_srcdir)/build-aux/gitlog-to-changelog                   \
	    --since=$(gen_start_date) |                                 \
            sed '/<unknown>$$/d' | fmt -s > ChangeLog;                  \
	  echo "" >> ChangeLog;                                       \
	  echo "Local Variables:" >> ChangeLog;                         \
	  echo "mode: change-log" >> ChangeLog;                         \
	  echo "version-control: never"  >> ChangeLog;                  \
	  echo "buffer-read-only: t" >> ChangeLog;                      \
	  echo "End:" >> ChangeLog;                                     \
        fi

