dnl This file is part of GNU mailutils.
dnl Copyright (C) 2001, 2006, 2007, 2010 Free Software Foundation, Inc.
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 3 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software Foundation,
dnl Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
dnl

dnl MU_RESULT_ACTIONS -- generate shell code for the result of a test
dnl   $1 -- CVAR  -- cache variable to check
dnl   $2 -- NAME  -- if not empty, used to generate a default value TRUE:
dnl                  `AC_DEFINE(HAVE_NAME)'
dnl   $3 -- TRUE  -- what to do if the CVAR is not `no'
dnl   $4 -- FALSE -- what to do otherwise; defaults to `:'
dnl
AC_DEFUN([MU_RESULT_ACTIONS], [
[if test "$$1" != "" -a "$$1" != no; then
  ]ifelse([$3], ,
	  [AC_DEFINE(HAVE_]translit($2, [a-z ./<>], [A-Z___])[,1,[FIXME])],
	  [$3])[
else
  ]ifelse([$4], , [:], [$4])[
fi]])dnl

AC_DEFUN([MU_CHECK_GUILE],
[
 if test "x$mu_cv_lib_guile" = x; then
   cached=""
   AC_PATH_PROG(GUILE_CONFIG, guile-config, no, $PATH)
   if test $GUILE_CONFIG = no; then
     mu_cv_lib_guile=no
   else
     GUILE_INCLUDES=`$GUILE_CONFIG compile`
     GUILE_LIBS=`$GUILE_CONFIG link`
   fi

   if test $GUILE_CONFIG != no; then
     AC_MSG_CHECKING(for guile version 1.8 or higher)
     GUILE_VERSION=`($GUILE_CONFIG --version 2>&1; echo '')|sed -n 's/guile-config [[^0-9]]* \([[0-9]][[0-9]]*\)\.\([[0-9]][[0-9]]*\).*/\1\2/p'`
     case "x$GUILE_VERSION" in
     x[[0-9]]*)
       if test $GUILE_VERSION -lt 18; then
	 AC_MSG_RESULT([Nope. Version number too low.])
	 mu_cv_lib_guile=no
       else
	 AC_DEFINE_UNQUOTED(GUILE_VERSION, $GUILE_VERSION,
			    [Guile version number: MAX*10 + MIN])
	 AC_MSG_RESULT(OK)
	 save_LIBS=$LIBS
	 save_CFLAGS=$CFLAGS
	 LIBS="$LIBS $GUILE_LIBS"
	 CFLAGS="$CFLAGS $GUILE_INCLUDES"
	 AC_TRY_LINK([#include <libguile.h>],
		     ifelse([$1], , scm_shell(0, NULL);, [$1]),
		     [mu_cv_lib_guile=yes],
		     [mu_cv_lib_guile=no])
	 LIBS=$save_LIBS
	 CFLAGS=$save_CFLAGS
       fi ;;
     *) AC_MSG_RESULT(Nope. Unknown version number)
	mu_cv_lib_guile=no;;
     esac
   fi
 else
   cached=" (cached) "
   GUILE_INCLUDES=`$GUILE_CONFIG compile`
   GUILE_LIBS=`$GUILE_CONFIG link`
 fi
 AC_MSG_CHECKING(whether to build guile support)
 MU_RESULT_ACTIONS([mu_cv_lib_guile],[LIBGUILE],[$2],[$3])
 AC_MSG_RESULT(${cached}$mu_cv_lib_guile)
 if test $mu_cv_lib_guile = yes; then
   AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <libguile.h>]],
			[SCM_DEVAL_P = 1;
			 SCM_BACKTRACE_P = 1;
			 SCM_RECORD_POSITIONS_P = 1;
			 SCM_RESET_DEBUG_MODE;])],
			[mu_cv_guile_debug=yes],
			[mu_cv_guile_debug=no])
   if test $mu_cv_guile_debug = yes; then
     AC_DEFINE_UNQUOTED(GUILE_DEBUG_MACROS, 1,
			[Define to 1 if SCM_DEVAL_P, SCM_BACKTRACE_P, SCM_RECORD_POSITIONS_P and SCM_RESET_DEBUG_MODE are defined])
   fi
   AC_CHECK_TYPES([scm_t_off],[],[],[#include <libguile.h>])
   AC_ARG_WITH([guiledir],
	       AC_HELP_STRING([--with-guiledir=DIR],
			      [Specify the directory to install guile modules to]),
	       [case $withval in
		/*) GUILE_SITE=$withval;;
		yes) GUILE_SITE=`$GUILE_CONFIG info pkgdatadir`/site;;
		*)  AC_MSG_ERROR([Argument to --with-guiledir must be an absolute directory name]);;
		esac],
	       [GUILE_SITE=`$GUILE_CONFIG info pkgdatadir`/site
		pfx=$prefix
		test "x$pfx" = xNONE && pfx=$ac_default_prefix
		case $GUILE_SITE in
		$pfx/*) ;; # OK
		*) AC_MSG_WARN([guile site directory "$GUILE_SITE" lies outside your current prefix ($pfx).])
		   GUILE_SITE='$(pkgdatadir)/$(VERSION)/guile'
		   AC_MSG_WARN([Falling back to ${GUILE_SITE} instead. Use --with-guiledir to force using site directory.])
		   ;;
		esac])
 fi
 AC_SUBST(GUILE_SITE)
 AC_SUBST(GUILE_INCLUDES)
 AC_SUBST(GUILE_LIBS)
])
