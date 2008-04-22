# $Id$

dictdir=@dictdir@

INCLUDES = -I${top_srcdir}/include -I${top_srcdir}/lib
AM_YFLAGS=-vt
AM_CFLAGS="-DDICTPATH=\"$(dictdir)\""

bin_PROGRAMS=gjdictd
gjdictd_SOURCES=\
 bushu.c\
 cmdline.c\
 main.c\
 server.c\
 gram.y\
 log.c\
 dict.c\
 utils.c\
 search.c\
 server.h

gjdictd_LDADD=../dbs/libdbs.a ../lib/libgjdict.a

bushu.c: $(top_srcdir)/lib/bushu.dat $(srcdir)/bushu.code.c
	$(AWK) -f $(top_srcdir)/utils/bushu.awk $(top_srcdir)/lib/bushu.dat |\
                  cat - $(srcdir)/bushu.code.c > bushu.c

SUFFIXES=.opt .c

.opt.c:
	m4 -s $(top_srcdir)/utils/getopt.m4 $< | sed '1d' > $@

BUILT_SOURCES = bushu.c cmdline.c
EXTRA_DIST = bushu.code.c cmdline.opt

