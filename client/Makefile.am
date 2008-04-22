INCLUDES = @X_CFLAGS@ -I${top_srcdir}/include -I${top_srcdir}/lib

bin_PROGRAMS=gjdict
pkgdata_DATA=gjdict.chl mini-gjdict.xpm

gjdict_LDADD= @XLDADD@ @X_PRE_LIBS@ -lX11 @X_EXTRA_LIBS@ ../dbs/libdbs.a ../lib/libgjdict.a
gjdict_LDFLAGS=@X_LIBS@
AM_CPPFLAGS=-DPKGDATADIR=\"$(pkgdatadir)\"

gjdict_SOURCES=\
 bushu.c\
 bushusearch.c\
 client.c\
 cmdline.c\
 convert.c\
 cornersearch.c\
 cystack.c\
 fallback.h\
 getfile.c\
 help.c\
 helpfile.c\
 init.c\
 kanasearch.c\
 license.c\
 main.c\
 search.c\
 skipsearch.c\
 util.c\
 widgets.c\
 xref.c\
 client.h\
 cystack.h\
 help.h\
 options.h\
 search.h

bushu.c: $(top_srcdir)/lib/bushu.dat $(srcdir)/bushu.code.c
	$(AWK) -f $(top_srcdir)/utils/bushu.awk $(top_srcdir)/lib/bushu.dat |\
		 cat - $(srcdir)/bushu.code.c > bushu.c

fallback.h: $(srcdir)/Gjdict.ad $(srcdir)/resource.sed
	sed -f $(srcdir)/resource.sed \
		$(srcdir)/Gjdict.ad > fallback-t.h; \
	if test -s fallback-t.h; then \
	  echo "/* -*- buffer-read-only: t -*- vi: set ro:" > fallback.h; \
	  echo "   THIS FILE IS GENERATED AUTOMATICALLY.  PLEASE DO NOT EDIT."\
               >>fallback.h; \
	  echo "*/" >>fallback.h; \
	  cat fallback-t.h >> fallback.h; \
	fi; \
	rm fallback-t.h

gjdict.chl: gjdict.help tmac.gjdict
	GROFF_TMAC_PATH=$(srcdir) \
	$(top_srcdir)/utils/helpcmp.pl \
               -v -mgjdict -ogjdict.chl -in0 -po2n -ll55n gjdict.help


SUFFIXES=.opt .c

.opt.c:
	m4 -s $(top_srcdir)/utils/getopt.m4 $< | sed '1d' > $@

BUILT_SOURCES=bushu.c fallback.h gjdict.chl cmdline.c
EXTRA_DIST=\
 Gjdict.ad\
 bushu.code.c\
 cmdline.opt\
 gjdict.help\
 resource.sed\
 tmac.gjdict\
 $(pkgdata_DATA)
