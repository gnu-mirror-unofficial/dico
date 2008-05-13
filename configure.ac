dnl Process this file with -*- autoconf -*- to produce a configure script. 
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

AC_INIT([dico], 1.99.90, [gray@gnu.org.ua])
AC_CONFIG_SRCDIR([dictd/main.c])
AM_CONFIG_HEADER(config.h)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR(m4)	
AM_INIT_AUTOMAKE([1.9 gnits tar-ustar std-options])

dnl Some variables
AC_SUBST(DICO_MODDIR,'$(libdir)/$(PACKAGE)')

dnl Checks for programs.
AC_PROG_CC
gl_EARLY	
AC_PROG_CPP
AC_PROG_AWK
AC_PROG_YACC
AC_PROG_LEX

LT_PREREQ(2.2.5a)
LT_CONFIG_LTDL_DIR([libltdl])
LT_INIT([dlopen])
LTDL_INIT([recursive])
LT_WITH_LTDL
AC_CONFIG_FILES([libltdl/Makefile])

dnl Checks for libraries.
AC_CHECK_LIB(socket, socket)
AC_CHECK_LIB(nsl, gethostbyaddr)
AC_CHECK_LIB(rt, nanosleep)

dnl Checks for header files.
AC_HEADER_DIRENT dnl not needed ?
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h limits.h strings.h sys/time.h syslog.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_HEADER_TIME

dnl Checks for library functions.
AC_FUNC_SETVBUF_REVERSED
AC_TYPE_SIGNAL
AC_CHECK_FUNCS(gethostname select socket strdup strerror strtol \
               setegid setregid setresgid seteuid setreuid \
	       sysconf getdtablesize)

gl_INIT
				   
# Check for Berkeley DB
db_path=
db_version=4
	
AC_ARG_WITH([berkeley-db],
            AC_HELP_STRING([--with-berkeley-db=version],
                           [use this Berkeley DB version]),
            [
case "${withval}" in
  /*)       db_path=${withval};;
  [[0-9]]*) db_version=${withval}
            ver=`echo $withval | tr -d '.'`
	    if test "$ver" -lt 4; then
	      AC_MSG_ERROR([Berkeley DB version 4 or greater is required])
	    fi;;
  *)        AC_MSG_ERROR([erroneous or missing argument to --with-berkeley-db]);;
esac])

## Set the variable status_dbm to $1 if:
##
##  1. Function $3 is defined in the library $2;
##  2. Header file db.h is available;
##  3. Version triplets defined in db.h and in the library match.        
##
## Then check if the major version, minor version and patchlevel of the
## library matches those from the header. If so, define WITH_BDB
## to the version (i.e. $1 with all dots removed). Otherwise, report
## an error and stop.
##
check_bdb() {
        ver=`echo $1 | tr -d '.'`
	major=`expr $ver : '\(.\).*'`
	save_LIBS=$LIBS
	AC_CHECK_LIB($2, $3,
                     [AC_CHECK_HEADERS(db.h)
                      if test $ac_cv_header_db_h = yes; then
		        LIBS="$LIBS -l$2"
                        status_dbm=$1
                      fi])
	if test $status_dbm = no; then
	  :
	else
	  AC_RUN_IFELSE(
           [AC_LANG_PROGRAM([#include "db.h"],
            [int v_major, v_minor, v_patch;
             db_version(&v_major, &v_minor, &v_patch);
             return !(DB_VERSION_MAJOR == $major
                       && v_major == DB_VERSION_MAJOR
                       && v_minor == DB_VERSION_MINOR
                       && v_patch == DB_VERSION_PATCH);
            ])],
           [DB_LIBS="-l$2"],
           [LIBS=$save_LIBS
	    unset ac_cv_header_db_h
	    status_dbm=no])
	fi
}

AC_SUBST(DB_INCLUDES)
AC_SUBST(DB_LIBS)
	
## Check for the Berkeley DB library version $1, assuming Slackware-like
## installation layout (header files in /usr/incude/db$vn and library named
## libdb-$version.so, where $version is the library version and $vn is
## $version with all dots removed.
##
check_slackware_bdb() {
	dir=db`echo $1|tr -d '.'`
	save_CPPFLAGS=$CPPFLAGS
	while test -n "$dir"
	do
		if test -d /usr/include/$dir; then
			CPPFLAGS="$CPPFLAGS -I/usr/include/$dir"
			break
		elif test -d /usr/local/include/$dir; then
			CPPFLAGS="$CPPFLAGS -I/usr/local/include/$dir"
			break
		fi		
		dir=`expr "$dir" : '\(.*\)[[0-9]]$'`
	done
	check_bdb $1 db-$1 db_create
	CPPFLAGS=$save_CPPFLAGS
	if test $status_dbm = "$1"; then
	    DB_INCLUDES="$DB_INCLUDES -I/usr/include/$dir"
	fi
}
	
check_bdb $db_version db db_create
if test $status_dbm = no; then
   check_slackware_bdb $db_version
   if test $status_dbm = no; then
     AC_MSG_ERROR([No suitable Berkeley DB library found.])
   fi
fi   	  
		  			   
# Check X configuration
AC_PATH_XTRA

XAW="Xaw"
AC_ARG_WITH(xaw3d,
 [AC_HELP_STRING([--with-xaw3d], [Use Xaw3d])],
 [XAW="Xaw3d"
  AC_DEFINE(USE_XAW3D,1,[Define to 1 if Xaw3d is beign used])])

AC_SUBST(XLDADD)
XLDADD="-l$XAW -lXmu -lXt"

AC_SUBST(fontdir)
fontdir='$(pkgdatadir)'
AC_ARG_WITH(x-fontdir,
 [AC_HELP_STRING([--with-fontdir=PATH], [Use specified font path])],
 [ fontdir=$withval ])

##FIXME
AC_SUBST(MKFONTDIR)
MKFONTDIR=mkfontdir
AC_SUBST(BDFTOPCF)
BDFTOPCF=bdftopcf
AC_SUBST(GZIP)
GZIP=gzip

log_facility="LOG_LOCAL1"
AC_MSG_CHECKING(for syslog facility to use)
AC_ARG_WITH(log-facility,
 [AC_HELP_STRING([--with-log-facility=facility],
                 [enable logging to the given facility])],
 AC_TRY_COMPILE([#include <syslog.h>], int lf = $withval,
	log_facility=$withval))
AC_DEFINE_UNQUOTED(LOGFACILITY, $log_facility, [Syslog facility to use])
AC_MSG_RESULT($log_facility)


AC_SUBST(inputdictpath)
inputdictpath=/usr/local/lib
AC_ARG_WITH(input-dict-path,
 [AC_HELP_STRING([--with-input-dict-path],
                 [Specify alternate path for input dictionaries])],
 [inputdictpath=$withval])

AC_SUBST(dictdir)
dictdir='$(pkgdatadir)'
AC_ARG_WITH(dict-path,
 [AC_HELP_STRING([--with-dict-path],
                 [Specify alternate path for dictionaries])],
 [dictdir=$withval])

# FIXME: This is not a right place for this
AH_BOTTOM([
/* Provide placeholders for future i18n framework */
#define N_(s) s
#define _(s) s

#if defined (SETVBUF_REVERSED)
# define SETVBUF(str,buf,mode,size) setvbuf(str,mode,buf,size)
#else
# define SETVBUF(str,buf,mode,size) setvbuf(str,buf,mode,size)
#endif
])

AC_ARG_VAR([LOG_FACILITY],
	   [Default syslog facility])
if test -n "$LOG_FACILITY"; then
   case $LOG_FACILITY in
   USER|DAEMON|AUTH|AUTHPRIV|MAIL|CRON|LOCAL[[0-7]])
      LOG_FACILITY=LOG_$LOG_FACILITY;;
   LOG_USER|LOG_DAEMON|LOG_AUTH|LOG_AUTHPRIV|LOG_MAIL|LOG_CRON|LOG_LOCAL[[0-7]])
      ;;
   *) AC_MSG_ERROR([Invalid value of LOG_FACILITY]);;
   esac
   AC_DEFINE_UNQUOTED([LOG_FACILITY],$LOG_FACILITY,
                      [Default syslog facility.])
fi		      
	
AC_ARG_VAR([SCROLL_WHEN_NEEDED],
           [Override the value of XawtextScrollWhenNeeded constant. In libXaw
            version 1.0.3, scrollVertical is boolean and the value whenNeeded
            is handled as False.  In libXaw3d, it seems to work as documented.
            Default is True for libXaw, and XawtextScrollWhenNeeded for
            libXaw3d.])
if test -z "$SCROLL_WHEN_NEEDED"; then
  if test "$XAW" = "Xaw3d"; then
    SCROLL_WHEN_NEEDED=XawtextScrollWhenNeeded
  else
    SCROLL_WHEN_NEEDED=XawtextScrollAlways
  fi
fi
case $SCROLL_WHEN_NEEDED in
XawtextScrollWhenNeeded|XawtextScrollAlways|True)
   AC_DEFINE_UNQUOTED([myXawtextScrollWhenNeeded],
                      $SCROLL_WHEN_NEEDED,
                      [Value of XawtextScrollWhenNeeded]);;
*) AC_MSG_ERROR(Invalid value for SCROLL_WHEN_NEEDED);;
esac

# Doc hints.
# Select a rendition level:
#  DISTRIB for stable releases (at most one dot in the version number)
#  and maintenance releases (two dots, patchlevel < 50)
#  PROOF for alpha releases.
#  PUBLISH can only be required manually when running make in doc/
AC_SUBST(RENDITION)
case `echo $VERSION|sed  's/[[^.]]//g'` in
""|".")  RENDITION=DISTRIB;;
"..")  if test `echo $VERSION | sed  's/.*\.//'` -lt 50; then
	 RENDITION=DISTRIB
       else
         RENDITION=PROOF
       fi;;
*)     RENDITION=PROOF;;
esac

	
AC_CONFIG_FILES([Makefile
                 include/Makefile
                 include/dico/Makefile
                 utils/Makefile
		 gnu/Makefile
                 lib/Makefile
                 dictd/Makefile
		 modules/Makefile
		 modules/outline/Makefile
                 makedict/Makefile
		 tests/Makefile
		 doc/Makefile])

AC_OUTPUT

