## This file is a source for configure.ac. -*- autoconf -*-
## Run ./bootstrap to create it.
## Run ./bootstrap --help for a detailed description.
## A short reminder:
##   1. Comments starting with ## are removed from the output.
##   2. A construct <NAME> is replaced with the value of the variable NAME.
##   3. <NAME#TEXT> on a line alone (arbitrary leading characters allowed)
##      is replaced with (multiline) expansion of NAME.  The second and
##      subsequent lines are prefixed with TEXT (or leading characters, if
##      TEXT is empty).
##   4. Everything else is reproduced verbatim.
dnl <HEADING#>
dnl Process this file with -*- autoconf -*- to produce a configure script. 
# This file is part of GNU Dico
# Copyright (C) 1998-2000, 2008-2010, 2012-2019 Sergey Poznyakoff
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

AC_PREREQ(2.63)
AC_INIT([GNU dico], 2.9, [bug-dico@gnu.org])
AC_CONFIG_SRCDIR([dicod/main.c])
AC_CONFIG_HEADERS(include/prog/config.h include/lib/config.h)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR(m4)	
AM_INIT_AUTOMAKE([1.11 nostdinc gnits tar-ustar dist-bzip2 dist-xz std-options subdir-objects])

dnl Enable silent rules by default:
AM_SILENT_RULES([yes])

dnl Some variables
AC_SUBST(DICO_MODDIR,'$(libdir)/$(PACKAGE)')

dnl Checks for programs.
AC_PROG_CC
gl_EARLY	
AC_PROG_CPP
AC_PROG_AWK
AC_PROG_YACC
AC_PROG_LEX

LT_PREREQ(2.4)
LT_CONFIG_LTDL_DIR([libltdl])
LT_INIT([dlopen])
LTDL_INIT([nonrecursive])

dnl Checks for libraries.
AC_CHECK_LIB(socket, socket)
AC_CHECK_LIB(nsl, gethostbyaddr)
AC_CHECK_LIB(rt, nanosleep)

dnl Checks for header files.
AC_HEADER_DIRENT dnl not needed ?
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h limits.h strings.h sys/time.h \
                 sys/socket.h socket.h syslog.h unistd.h \
                 crypt.h readline/readline.h)

dnl Checks for typedefs, structures, and compiler characteristics.
gl_INIT

AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_SYS_LARGEFILE
AC_CHECK_TYPE([socklen_t],,
  AC_DEFINE(socklen_t, int, [Define to int if <sys/types.h> does not define]),
[
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif
#ifdef HAVE_SOCKET_H
# include <socket.h>
#endif
])

dnl Checks for library functions.
AC_TYPE_SIGNAL
AC_CHECK_FUNCS(gethostname select socket strdup strerror strtol \
               setegid setregid setresgid seteuid setreuid \
	       sysconf getdtablesize)

# Crypt
AC_CHECK_DECLS(crypt,,,[
#include <unistd.h>
#ifdef HAVE_CRYPT_H
# include <crypt.h>
#endif])
AH_BOTTOM([
#if !HAVE_DECL_CRYPT
char *crypt(const char *key, const char *salt);
#endif])

AC_CHECK_LIB(crypt, crypt)

# Gettext.
AM_ICONV
AM_GNU_GETTEXT([external], [need-formatstring-macros])
AM_GNU_GETTEXT_VERSION([0.18])
AC_CONFIG_LINKS(include/gettext.h:xdico/gnu/gettext.h)

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

AC_ARG_VAR([DEFAULT_DICT_SERVER],
           [Set the name of the default DICT server for dico utility])
if test -z "$DEFAULT_DICT_SERVER" ; then
   DEFAULT_DICT_SERVER="gnu.org.ua"
fi

AC_ARG_VAR([DEFAULT_INCLUDE_DIR],
           [Default preprocessor include directory])
if test -z "$DEFAULT_INCLUDE_DIR"; then
   DEFAULT_INCLUDE_DIR='$(pkgdatadir)/include'
fi   
AC_ARG_VAR([DEFAULT_VERSION_INCLUDE_DIR],
           [Default version-specific include directory])
if test -z "$DEFAULT_VERSION_INCLUDE_DIR"; then
   DEFAULT_VERSION_INCLUDE_DIR='$(pkgdatadir)/$(VERSION)/include'
fi   
AC_ARG_VAR([DEFAULT_PIDFILE_NAME],
           [Default name for the dicod pid file])
if test -z "$DEFAULT_PIDFILE_NAME"; then
   DEFAULT_PIDFILE_NAME='$(localstatedir)/run/dicod.pid'
fi
AC_SUBST([DICO_PROG_CONFIG],['-I$(top_builddir)/include/prog'])
AC_SUBST([DICO_LIB_CONFIG],['-I$(top_builddir)/include/lib'])

AC_SUBST([DICO_PROG_INCLUDES],[dnl
'$(DICO_PROG_CONFIG)\
 -I$(top_srcdir)/include\
 -I$(top_builddir)/include\
 -I$(top_srcdir)/xdico/gnu\
 -I$(top_builddir)/xdico/gnu\
 $(GRECS_INCLUDES)'])

AC_SUBST([DICO_MODULE_INCLUDES],[dnl
'-I$(srcdir)\
 $(DICO_LIB_CONFIG)\
 -I$(top_srcdir)/include\
 -I$(top_builddir)/include\
 $(GRECS_INCLUDES)'])

# Grecs configuration system
GRECS_SETUP(grecs, [shared tests getopt git2chg sockaddr-list])
GRECS_HOST_PROJECT_INCLUDES='$(DICO_LIB_CONFIG) -I$(top_builddir)/include'

# Tcl/tk
AC_ARG_WITH([tk],
            AC_HELP_STRING([--with-tk],
	                   [build Tcl/Tk-based applications (GCIDER)]),
	    [
case "${withval}" in
  yes) status_tk=yes ;;
  no)  status_tk=no;;
  *) AC_MSG_ERROR([bad value ${withval} for --with-tk])
esac],[status_tk=yes])

if test $status_tk=yes; then
  AC_PATH_PROG([WISH], wish)
  if test -z "$WISH"; then
    status_tk=no
  fi
  AM_CONDITIONAL([TK_WISH_COND],[test $status_tk = yes])
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
    status_readline="no"
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
    AC_SUBST(LIBDICOSASL,'$(top_builddir)/xdico/libdicosasl.a')])
AM_CONDITIONAL([COND_LIBDICOSASL],[test "$status_gsasl" = yes])

<MODULES>
    
# Imprimatur
IMPRIMATUR_INIT

# Tests
DICO_TESTS(lib)
DICO_TESTS(dicod)

AC_CONFIG_COMMANDS([status],[
cat <<EOT

*******************************************************************
Dico configured with the following settings:

GSASL ........................... $status_gsasl
Readline ........................ $status_readline
Preprocessor .................... $dicopp

<STATUS_OUTPUT>

Default server .................. $defserver
Autologin file .................. $autologin_file
*******************************************************************
EOT
],
[status_gsasl=$status_gsasl
status_readline=$status_readline
if test $use_ext_pp = no; then
  dicopp=no
else
  dicopp="$DEFAULT_PREPROCESSOR"
fi
defserver="$DEFAULT_DICT_SERVER"
autologin_file="$DEFAULT_AUTOLOGIN_FILE"
<STATUS_DEFN>
])

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 include/dico/Makefile
		 examples/Makefile 
		 gint/Makefile
		 xdico/gnu/Makefile
		 xdico/Makefile
                 lib/Makefile
                 dicod/Makefile
		 modules/Makefile
		 dico/Makefile
		 scripts/Makefile
		 doc/Makefile
		 po/Makefile.in
		 app/python/dicod.conf])

AC_OUTPUT
