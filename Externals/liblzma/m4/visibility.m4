dnl SPDX-License-Identifier: FSFULLR

# visibility.m4
# serial 9
dnl Copyright (C) 2005, 2008, 2010-2024 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Bruno Haible.

dnl Tests whether the compiler supports the command-line option
dnl -fvisibility=hidden and the function and variable attributes
dnl __attribute__((__visibility__("hidden"))) and
dnl __attribute__((__visibility__("default"))).
dnl Does *not* test for __visibility__("protected") - which has tricky
dnl semantics (see the 'vismain' test in glibc) and does not exist e.g. on
dnl Mac OS X.
dnl Does *not* test for __visibility__("internal") - which has processor
dnl dependent semantics.
dnl Does *not* test for #pragma GCC visibility push(hidden) - which is
dnl "really only recommended for legacy code".
dnl Set the variable CFLAG_VISIBILITY.
dnl Defines and sets the variable HAVE_VISIBILITY.

AC_DEFUN([gl_VISIBILITY],
[
  AC_REQUIRE([AC_PROG_CC])
  CFLAG_VISIBILITY=
  HAVE_VISIBILITY=0
  if test -n "$GCC"; then
    dnl First, check whether -Werror can be added to the command line, or
    dnl whether it leads to an error because of some other option that the
    dnl user has put into $CC $CFLAGS $CPPFLAGS.
    AC_CACHE_CHECK([whether the -Werror option is usable],
      [gl_cv_cc_vis_werror],
      [gl_saved_CFLAGS="$CFLAGS"
       CFLAGS="$CFLAGS -Werror"
       AC_COMPILE_IFELSE(
         [AC_LANG_PROGRAM([[]], [[]])],
         [gl_cv_cc_vis_werror=yes],
         [gl_cv_cc_vis_werror=no])
       CFLAGS="$gl_saved_CFLAGS"
      ])
    dnl Now check whether visibility declarations are supported.
    AC_CACHE_CHECK([for simple visibility declarations],
      [gl_cv_cc_visibility],
      [gl_saved_CFLAGS="$CFLAGS"
       CFLAGS="$CFLAGS -fvisibility=hidden"
       dnl We use the option -Werror and a function dummyfunc, because on some
       dnl platforms (Cygwin 1.7) the use of -fvisibility triggers a warning
       dnl "visibility attribute not supported in this configuration; ignored"
       dnl at the first function definition in every compilation unit, and we
       dnl don't want to use the option in this case.
       if test $gl_cv_cc_vis_werror = yes; then
         CFLAGS="$CFLAGS -Werror"
       fi
       AC_COMPILE_IFELSE(
         [AC_LANG_PROGRAM(
            [[extern __attribute__((__visibility__("hidden"))) int hiddenvar;
              extern __attribute__((__visibility__("default"))) int exportedvar;
              extern __attribute__((__visibility__("hidden"))) int hiddenfunc (void);
              extern __attribute__((__visibility__("default"))) int exportedfunc (void);
              void dummyfunc (void);
              int hiddenvar;
              int exportedvar;
              int hiddenfunc (void) { return 51; }
              int exportedfunc (void) { return 1225736919; }
              void dummyfunc (void) {}
            ]],
            [[]])],
         [gl_cv_cc_visibility=yes],
         [gl_cv_cc_visibility=no])
       CFLAGS="$gl_saved_CFLAGS"
      ])
    if test $gl_cv_cc_visibility = yes; then
      CFLAG_VISIBILITY="-fvisibility=hidden"
      HAVE_VISIBILITY=1
    fi
  fi
  AC_SUBST([CFLAG_VISIBILITY])
  AC_SUBST([HAVE_VISIBILITY])
  AC_DEFINE_UNQUOTED([HAVE_VISIBILITY], [$HAVE_VISIBILITY],
    [Define to 1 or 0, depending whether the compiler supports simple visibility declarations.])
])
