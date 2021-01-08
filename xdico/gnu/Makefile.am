AUTOMAKE_OPTIONS = 1.11 gnits subdir-objects

SUBDIRS =
noinst_HEADERS =
noinst_LIBRARIES =
noinst_LTLIBRARIES =
EXTRA_DIST =
BUILT_SOURCES =
SUFFIXES =
MOSTLYCLEANFILES = 
MOSTLYCLEANDIRS =
CLEANFILES =
DISTCLEANFILES =
MAINTAINERCLEANFILES =

# Starting from gnulib commit 52ab50a7ebc469b542bca0063430db71aefb67af,
# xalloc.h declares xalloc_die only if GNULIB_XALLOC_DIE is defined.
# Without this define each inclusion of xalloc.h will spit out a number
# of warnings about this function not being declared.
# See also include/xdico.h
AM_CPPFLAGS = @DICO_PROG_CONFIG@ -I$(srcdir) -I$(builddir) -DGNULIB_XALLOC_DIE=1
AM_CFLAGS =

include gnulib.am
