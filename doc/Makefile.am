# This file is part of GNU Dico
# Copyright (C) 2008 Sergey Poznyakoff
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

info_TEXINFOS=dico.texi
dico_TEXINFOS=\
 fdl.texi\
 libdico.texi\
 macros.texi\
 proto.texi\
 rendition.texi\
 strftime.texi

EXTRA_DIST = \
 check-docs.sh\
 gendocs_template\
 mastermenu.el\
 untabify.el

clean-local:
	rm -rf manual

# Checking
check-format:
	@if test -n "`cat $(info_TEXINFOS) $(dico_TEXINFOS) | tr -d -c '\t'`"; then \
		echo "Sources contain tabs; run make untabify"; \
		false; \
	fi

check-options:
	@check-docs.sh options \
	   '/OPTIONS_BEGIN/,/OPTIONS_END/s/OPTION( *\([^,][^,]*\),.*/\1/pg' \
	   '/@c option --/{s///;s/^\(help\|version\|usage\)//;p}' \
	   $(top_srcdir)/dicod/cmdline.opt -- \
	   $(MAKEINFO) $(AM_MAKEINFOFLAGS) $(MAKEINFOFLAGS) -I $(srcdir) -E - \
		$(info_TEXINFOS) 

check-refs:
	@for file in $(info_TEXINFOS) $(dico_TEXINFOS); \
	do \
	  sed -e = $$file | \
           sed -n 'N;/@FIXME-.*ref/{s/\(^[0-9][0-9]*\).*@FIXME-.*ref{\([^}]*\)}.*/'$$file':\1: \2/gp}'; \
	done > $@-t; \
	if [ -s $@-t ]; then \
	  echo "Unresolved cross-references:"; \
	  cat $@-t;\
	  rm $@-t; \
	else \
	  rm -f $@-t; \
	fi

check-fixmes:
	@for file in $(info_TEXINFOS); \
	do \
	  sed -e = $$file | \
           sed -n 'N;/@FIXME{/{s/\(^[0-9][0-9]*\).*@FIXME{\([^}]*\).*/'$$file':\1: \2/gp}'; \
	done > $@-t; \
	if [ -s $@-t ]; then \
	  echo "Unresolved FIXMEs:"; \
	  cat $@-t; \
	  rm $@-t; \
	  false; \
	else \
          rm -f $@-t; \
	fi

check-unrevised:
	@grep -Hn @UNREVISED $(info_TEXINFOS) $(dico_TEXINFOS) > $@-t; \
	if [ -s $@-t ]; then \
	  echo "Unrevised nodes:"; \
	  cat $@-t; \
	  rm $@-t; \
	  false;\
	else \
          rm $@-t; \
	fi

all-check-docs: check-format check-options check-refs check-fixmes check-unrevised

check-docs: 
	$(MAKE) -k all-check-docs

#

master-menu:
	emacs -batch -l mastermenu.el -f make-master-menu $(info_TEXINFOS)

untabify:
	emacs -batch -l untabify.el $(info_TEXINFOS) $(dico_TEXINFOS)

final: untabify master-menu

# The rendering level is one of PUBLISH, DISTRIB or PROOF.
# Just call `make RENDITION=PROOF [target]' if you want PROOF rendition.

MAKEINFOFLAGS=-D$(RENDITION)

GENDOCS=gendocs.sh 

TEXI2DVI=texi2dvi -t '@set $(RENDITION)' -E

# Make sure you set TEXINPUTS.
# TEXINPUTS=/usr/share/texmf/pdftex/plain/misc/ is ok for most distributions
manual:
	TEXINPUTS=$(srcdir):$(top_srcdir)/build-aux:$(TEXINPUTS) \
	 MAKEINFO="$(MAKEINFO) $(MAKEINFOFLAGS)" \
	 TEXI2DVI="$(TEXI2DVI) -t @finalout" \
	 $(GENDOCS) --texi2html $(PACKAGE) '$(PACKAGE_NAME) manual'

