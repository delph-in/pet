##### http://autoconf-archive.cryp.to/ax_boost_regex.html
# ATTENTION: original file modified for PET by Peter Adolphs
#
# SYNOPSIS
#
#   AX_BOOST_REGEX([action-if-found], [action-if-not-found])
#
# DESCRIPTION
#
#   Test for Regex library from the Boost C++ libraries. The macro
#   requires a preceding call to AX_BOOST_BASE. Further documentation
#   is available at <http://randspringer.de/boost/index.html>.
#
#   This macro calls:
#
#     AC_SUBST(BOOST_REGEX_LIB)
#
#   And sets:
#
#     HAVE_BOOST_REGEX
#
# LAST MODIFICATION
#
#   2008-02-25 by Peter Adolphs
#              - fixed broken indentation (tabs/spaces)
#              - added ACTION-IF-FOUND and ACTION-IF-NOT-FOUND
#              - notice instead of error when boost was not found
#              - changed help message
#
# COPYLEFT
#
#   Copyright (c) 2007 Thomas Porschberg <thomas@randspringer.de>
#   Copyright (c) 2007 Michael Tindal
#   Copyright (c) 2008 Peter Adolphs
#
#   Copying and distribution of this file, with or without
#   modification, are permitted in any medium without royalty provided
#   the copyright notice and this notice are preserved.

AC_DEFUN([AX_BOOST_REGEX],
[
  AC_ARG_WITH([boost-regex],
    [ AS_HELP_STRING(
        [--with-boost-regex@<:@=ARG@:>@],
        [use standard Boost.Regex library (ARG=yes),
         use specific Boost.Regex library (ARG=<name>, e.g. ARG=boost_regex-gcc-mt-d-1_33_1),
         or disable it (ARG=no)
         @<:@ARG=yes@:>@ ]) ],
    [
      if test "$withval" = "no"; then
        want_boost="no"
      elif test "$withval" = "yes"; then
        want_boost="yes"
        ax_boost_user_regex_lib=""
      else
        want_boost="yes"
        ax_boost_user_regex_lib="$withval"
      fi
    ],
    [want_boost="yes"]
  )
  
  if test "x$want_boost" = "xyes"; then
    AC_REQUIRE([AX_BOOST_BASE])
    CPPFLAGS_SAVED="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
    export CPPFLAGS
    
    LDFLAGS_SAVED="$LDFLAGS"
    LDFLAGS="$LDFLAGS $BOOST_LDFLAGS"
    export LDFLAGS
    
    AC_CACHE_CHECK([whether the Boost::Regex library is available],
      [ax_cv_boost_regex],
      [
        AC_LANG_PUSH([C++])
        AC_COMPILE_IFELSE(AC_LANG_PROGRAM([[@%:@include <boost/regex.hpp>
          ]],
          [[boost::regex r(); return 0;]]),
        [ax_cv_boost_regex=yes],
        [ax_cv_boost_regex=no])
        AC_LANG_POP([C++])
      ]
    )
    if test "x$ax_cv_boost_regex" = "xyes"; then
      AC_DEFINE(HAVE_BOOST_REGEX,,[define if the Boost::Regex library is available])
      BN_BOOST_REGEX=boost_regex
      BOOSTLIBDIR=`echo $BOOST_LDFLAGS | sed -e 's/@<:@^\/@:>@*//'`
      if test "x$ax_boost_user_regex_lib" = "x"; then
        for libextension in `ls $BOOSTLIBDIR/libboost_regex*.{so,a}* | sed 's,.*/,,' | sed -e 's;^libboost_regex\(.*\)\.so.*$;\1;' -e 's;^libboost_regex\(.*\)\.a*$;\1;'` ; do
          ax_lib=${BN_BOOST_REGEX}${libextension}
          AC_CHECK_LIB($ax_lib, exit,
                 [BOOST_REGEX_LIB="-l$ax_lib"; AC_SUBST(BOOST_REGEX_LIB) link_regex="yes"; break],
                 [link_regex="no"])
        done
      else
        for ax_lib in $ax_boost_user_regex_lib $BN_BOOST_REGEX-$ax_boost_user_regex_lib; do
          AC_CHECK_LIB($ax_lib, main,
                 [BOOST_REGEX_LIB="-l$ax_lib"; AC_SUBST(BOOST_REGEX_LIB) link_regex="yes"; break],
                 [link_regex="no"])
        done
      fi
      if test "x$link_regex" = "xyes"; then
        # execute ACTION-IF-FOUND (if present):
        ifelse([$1], , :, [$1])
      else
        AC_MSG_NOTICE(Could not link against $ax_lib !)
        # execute ACTION-IF-NOT-FOUND (if present):
        ifelse([$2], , :, [$2])
      fi
    fi

    CPPFLAGS="$CPPFLAGS_SAVED"
    LDFLAGS="$LDFLAGS_SAVED"
  fi
])
