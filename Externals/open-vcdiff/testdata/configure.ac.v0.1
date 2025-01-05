## Process this file with autoconf to produce configure.
## In general, the safest way to proceed is to run ./autogen.sh

# make sure we're interpreted by some minimal autoconf
AC_PREREQ(2.57)

AC_INIT(open-vcdiff, 0.1, opensource@google.com)
AC_CONFIG_SRCDIR(README)
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(src/config.h)

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AM_CONDITIONAL(GCC, test "$GCC" = yes)   # let the Makefile know if we're gcc
AC_CANONICAL_HOST

AC_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)

# Check whether some low-level functions/files are available
AC_HEADER_STDC

case $host in
  *86*-*-gnu*) AC_DEFINE(VCDIFF_USE_BLOCK_COMPARE_WORDS, 1,
			 Use custom compare function instead of memcmp)
esac

AC_CHECK_HEADERS([ext/rope])
AC_CHECK_HEADERS([getopt.h])
AC_CHECK_HEADERS([malloc.h])
AC_CHECK_HEADERS([sys/mman.h])
AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([unistd.h])
AC_CHECK_HEADERS([windows.h])
AC_CHECK_FUNCS([gettimeofday QueryPerformanceCounter])
AC_CHECK_FUNCS([memalign posix_memalign])
AC_CHECK_FUNCS([mprotect])

# Start of definitions needed by gflags package

AC_CHECK_HEADERS([stdint.h sys/types.h inttypes.h])
AC_CHECK_HEADERS([fnmatch.h])
AC_CHECK_FUNCS([InitializeCriticalSection])
AC_CHECK_FUNCS([InterlockedCompareExchange])
AC_CHECK_FUNCS([strtoll strtoq])
AC_CHECK_TYPES([uint16_t, u_int16_t, __int16])

AX_C___ATTRIBUTE__

ACX_PTHREAD

# End of definitions needed by gflags package

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
