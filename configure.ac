dnl Process this file with -*- autoconf -*- to produce a configure script. 
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

AC_INIT([GNU dico], 1.99.97, [bug-dico@gnu.org.ua])
AC_CONFIG_SRCDIR([dicod/main.c])
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
AC_CHECK_HEADERS(fcntl.h limits.h strings.h sys/time.h syslog.h unistd.h \
                 readline/readline.h)

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

# Gettext.
AM_ICONV
AM_GNU_GETTEXT([external], [need-formatstring-macros])
AM_GNU_GETTEXT_VERSION([0.17])
AC_CONFIG_LINKS(include/gettext.h:gnu/gettext.h)

# Check for zlib
AC_CHECK_HEADER(zlib.h,
                AC_CHECK_LIB(z, inflate,
                             [AC_DEFINE_UNQUOTED([USE_LIBZ], 1,
			              [Define this if you have libz library])
	                      AC_SUBST(LIBZ, -lz)]))

LOG_FACILITY="LOG_DAEMON"

AC_ARG_VAR([LOG_FACILITY],
	   [Default syslog facility])
if test -n "$LOG_FACILITY"; then
   logfacility=`echo $LOG_FACILITY | tr a-z A-Z`
   case $logfacility in
   USER|DAEMON|AUTH|AUTHPRIV|MAIL|CRON|LOCAL[[0-7]])
      LOG_FACILITY=LOG_$logfacility;;
   LOG_USER|LOG_DAEMON|LOG_AUTH|LOG_AUTHPRIV|LOG_MAIL|LOG_CRON|LOG_LOCAL[[0-7]])
      LOG_FACILITY=$logfacility;;
   *) AC_MSG_ERROR([Invalid value of LOG_FACILITY]);;
   esac
fi		      
AC_DEFINE_UNQUOTED([LOG_FACILITY],$LOG_FACILITY,
                   [Default syslog facility.])
	
# FIXME: This is not a right place for this
AH_BOTTOM([
#if defined (SETVBUF_REVERSED)
# define SETVBUF(str,buf,mode,size) setvbuf(str,mode,buf,size)
#else
# define SETVBUF(str,buf,mode,size) setvbuf(str,buf,mode,size)
#endif
])


# Check for Berkeley DB
db_path=
db_version=4
status_dbm=no

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
           [unset ac_cv_header_db_h
	    status_dbm=no])
	   LIBS=$save_LIBS
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
fi   	  

if test $status_dbm != no; then
  AC_SUBST(BUILD_JDICT_PROGS,'$(JDICT_PROGS)')
fi

AC_ARG_WITH([preprocessor],
            AC_HELP_STRING([--without-preprocessor],
                           [do not use external preprocessor]),
            [
case "${withval}" in
yes) use_ext_pp=yes ;;
no)  use_ext_pp=no ;;
*)   AC_MSG_ERROR(bad value ${withval} for --with-preprocessor) ;;
esac],[use_ext_pp=yes])

if test $use_ext_pp != no; then
  # Check for default preprocessor
  AC_ARG_VAR([DEFAULT_PREPROCESSOR],
             [Set default preprocessor name])
  if test -z "$DEFAULT_PREPROCESSOR" ; then
    DEFAULT_PREPROCESSOR="m4 -s"
  fi
  
  save_PATH=$PATH
  PREPROC_OPTIONS=`echo $DEFAULT_PREPROCESSOR | sed -n 's/[[^ ]][[^ ]]* //p'`
  case "$DEFAULT_PREPROCESSOR" in
  /*) PATH=`expr $DEFAULT_PREPROCESSOR : '\(.*\)/.*'`:$PATH
      DEFAULT_PREPROCESSOR=`expr $DEFAULT_PREPROCESSOR : '.*/\(.*\)'`;;
  esac
  AC_PATH_PROG(PPBIN, $DEFAULT_PREPROCESSOR)
  DEFAULT_PREPROCESSOR=$PPBIN
  if test -n "$DEFAULT_PREPROCESSOR"; then
    DEFAULT_PREPROCESSOR="$DEFAULT_PREPROCESSOR $PREPROC_OPTIONS"
    case $PPBIN in
    *m4) AC_SUBST(PP_SETUP_FILE,'pp-setup');; # Install default pp-setup
    esac
  fi
  PATH=$save_PATH
  DEFAULT_PREPROCESSOR="\\\"$DEFAULT_PREPROCESSOR\\\""
else
  DEFAULT_PREPROCESSOR=NULL
fi

AC_ARG_VAR([DEFAULT_DICT_SERVER],
           [Set the name of the default DICT server for dico utility])
if test -z "$DEFAULT_DICT_SERVER" ; then
   DEFAULT_DICT_SERVER="gnu.org.ua"
fi

# Check for Guile
AC_ARG_WITH([guile],
            AC_HELP_STRING([--without-guile],
                           [do not build guile interface]),
            [
case "${withval}" in
  yes) status_guile=yes ;;
  no)  status_guile=no ;;
  *)   AC_MSG_ERROR(bad value ${withval} for --without-guile) ;;
esac],[status_guile=yes])

if test "$status_guile" = yes; then
  MU_CHECK_GUILE(,[
            AC_SUBST(GUILE_LIBS)
	    AC_SUBST(GUILE_INCLUDES)
            AC_SUBST([BUILD_DICO_GUILE_MODULES],['$(DICO_GUILE_MODULES)'])],
	         [status_guile=no])
fi

# Check for Python
AC_ARG_WITH([python],
            AC_HELP_STRING([--without-python],
                           [do not build Python interface]),
            [
case "${withval}" in
  yes) status_python=yes ;;
  no)  status_python=no ;;
  *)   AC_MSG_ERROR(bad value ${withval} for --without-python) ;;
esac],[status_python=yes])

if test "$status_python" = yes; then
  AC_ARG_VAR([PYTHON_CONFIG], [The name of python-config binary])
  AC_PATH_PROG([PYTHON_CONFIG], python-config)
  if test -n "$PYTHON_CONFIG"; then
    AC_SUBST(PYTHON_LIBS,`python-config --libs`)
    AC_SUBST(PYTHON_INCLUDES,`python-config --includes`)
    AC_SUBST([BUILD_DICO_PYTHON_MODULES],['$(DICO_PYTHON_MODULES)'])
  else
    status_python=no     
  fi	
fi

#
AC_ARG_WITH(autologin-file,
            AC_HELP_STRING([--with-autologin-file@<:@=NAME@:>@],
	                   [Use the autologin file (default NAME is .dicologin)]),
            [case $withval in
	     yes) DEFAULT_AUTOLOGIN_FILE=".dicologin";;
	     no)  DEFAULT_AUTOLOGIN_FILE=;;
	     *)   DEFAULT_AUTOLOGIN_FILE=$withval;;
	     esac],
	    [DEFAULT_AUTOLOGIN_FILE=".dicologin"])
if test -n "$DEFAULT_AUTOLOGIN_FILE" ; then
   AC_DEFINE_UNQUOTED(DEFAULT_AUTOLOGIN_FILE, "$DEFAULT_AUTOLOGIN_FILE",
                      [Define to the name of a netrc-style autologin file])
else
   AC_DEFINE_UNQUOTED(DEFAULT_AUTOLOGIN_FILE, NULL,
                      [Define to the name of a netrc-style autologin file])
fi

AC_ARG_WITH([readline],
            AC_HELP_STRING([--without-readline],
                           [do not use readline]),
            [
case "${withval}" in
  yes) status_readline=yes ;;
  no)  status_readline=no ;;
  *)   AC_MSG_ERROR(bad value ${withval} for --without-readline) ;;
esac],[status_readline=yes])

# Test for GNU Readline
AC_SUBST(READLINE_LIBS)

if test "$status_readline" != "no"; then
  
  dnl FIXME This should only link in the curses libraries if it's
  dnl really needed!
  
  dnl Check for Curses libs.
  CURSES_LIBS=
  for lib in ncurses curses termcap
  do
    AC_CHECK_LIB($lib, tputs, [CURSES_LIBS="-l$lib"; break])
  done

  saved_LIBS=$LIBS
  LIBS="$LIBS $CURSES_LIBS"
  AC_CHECK_LIB(readline, readline, mf_have_readline=yes)
  LIBS=$saved_LIBS
  
  if test "$mf_have_readline" = "yes"; then
    AC_CHECK_HEADERS(readline/readline.h,
  		     AC_DEFINE(WITH_READLINE,1,[Enable use of readline]))
    READLINE_LIBS="-lreadline $CURSES_LIBS"
    saved_LIBS=$LIBS
    LIBS="$LIBS $READLINE_LIBS"
    AC_CHECK_FUNCS(rl_completion_matches)
    LIBS=$saved_LIBS
    status_readline="yes"
  else
    if test "$status_readline" = "yes"; then
      AC_MSG_WARN(readline requested but does not seem to be installed)
    fi
    status_readline = "no"
  fi
fi

AH_BOTTOM([
/* Some older versions of readline have completion_matches */
#ifndef HAVE_RL_COMPLETION_MATCHES
# define rl_completion_matches completion_matches
#endif])

# GNU SASL
status_gsasl=no
MU_CHECK_GSASL(0.2.5, [
    AC_DEFINE(WITH_GSASL,1,[Define if GNU SASL is present])
    status_gsasl=yes
    AC_SUBST(BUILD_LIBDICOSASL,'libdicosasl.a')
    AC_SUBST(LIBDICOSASL,'../lib/libdicosasl.a')])

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

AC_CONFIG_COMMANDS([status],[
cat <<EOT

*******************************************************************
Dico configured with the following settings:

Guile ........................... $status_guile
Python .......................... $status_python
GSASL ........................... $status_gsasl
Readline ........................ $status_readline
Preprocessor .................... $dicopp

Default server .................. $defserver
Autologin file .................. $autologin_file
*******************************************************************
EOT
],
[status_guile=$status_guile
status_python=$status_python
status_gsasl=$status_gsasl
status_readline=$status_readline
if test $use_ext_pp = no; then
  dicopp=no
else
  dicopp="$DEFAULT_PREPROCESSOR"
fi
defserver="$DEFAULT_DICT_SERVER"
autologin_file="$DEFAULT_AUTOLOGIN_FILE"
])
	
AC_CONFIG_FILES([Makefile
                 include/Makefile
                 include/dico/Makefile
                 utils/Makefile
		 gnu/Makefile
                 lib/Makefile
                 dicod/Makefile
		 modules/Makefile
		 modules/outline/Makefile
		 modules/dict.org/Makefile
		 modules/guile/Makefile
		 modules/python/Makefile
		 dico/Makefile
		 tests/Makefile
		 doc/Makefile
		 po/Makefile.in])

AC_OUTPUT

