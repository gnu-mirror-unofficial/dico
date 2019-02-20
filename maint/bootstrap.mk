# SYNOPSIS
#   make bootstrap
#
# DESRIPTION
#   Bootstrap a freshly cloned copy of dico.
#

ChangeLog: Makefile
	$(MAKE) ChangeLog

Makefile: Makefile.am configure
	./configure

configure: configure.ac

configure.ac: configure.boot
	./bootstrap

