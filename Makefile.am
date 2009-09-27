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

ACLOCAL_AMFLAGS = -I m4 -I am

SUBDIRS=include utils gnu lib libltdl dicod modules dico tests doc po
#FIXME: add these when ready: makedict client fonts

EXTRA_DIST = ChangeLog.2008

dist-hook:
	tar -C $(srcdir) -c -f - --exclude-vcs app dicoweb | \
	      tar -C $(distdir) -x -f -

# Define the following variables in order to use the ChangeLog rule below:
#  prev_change_log  [optional]  Name of the previous ChangeLog file.
#  gen_start_date   [optional]  Start ChangeLog from this date.
#  changelog_dir    [mandatory] Directory where to create ChangeLog
gen_start_date =
prev_change_log =
changelog_dir = .

.PHONY: ChangeLog
ChangeLog: 
	$(AM_V_GEN)if test -d .git; then                                   \
	  cmd=$(top_srcdir)/build-aux/gitlog-to-changelog;                 \
	  if test -n "$(gen_start_date)"; then                             \
	    cmd="$$cmd --since=\"$(gen_start_date)\"";                     \
	  fi;                                                              \
	  $$cmd --format='%s%n%n%b%n' |                                    \
            sed '/<unknown>$$/d' | fmt -s > $(changelog_dir)/cl-t;         \
          if test -n "$(prev_change_log)" && test -f "$(prev_change_log)"; \
	  then                                                             \
	    echo "" >> $(changelog_dir)/cl-t;                              \
	    cat "$(prev_change_log)" |                                     \
	      sed '/^Local Variables:/,/^End:/d' >> $(changelog_dir)/cl-t; \
	  fi;                                                              \
	  echo "Local Variables:" >> $(changelog_dir)/cl-t;                \
	  echo "mode: change-log" >> $(changelog_dir)/cl-t;                \
	  echo "version-control: never"  >> $(changelog_dir)/cl-t;         \
	  echo "buffer-read-only: t" >> $(changelog_dir)/cl-t;             \
	  echo "End:" >> $(changelog_dir)/cl-t;                            \
	  rm -f $(changelog_dir)/ChangeLog;                                \
	  mv $(changelog_dir)/cl-t $(changelog_dir)/ChangeLog;             \
	fi
