/* Copyright (C) 2003 Free Software Foundation, Inc.
   This file is part of the GNU CHARSET Library.

   The GNU CHARSET Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU CHARSET Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with the GNU CHARSET Library; see the file COPYING.LIB.  If not,
   see <https://www.gnu.org/licenses/>.  */

#ifndef _LIBCHARSET_H
#define _LIBCHARSET_H

#if 1 && BUILDING_LIBCHARSET
# define LIBCHARSET_SHLIB_EXPORTED __attribute__((__visibility__("default")))
#elif defined _MSC_VER && BUILDING_LIBCHARSET
/* When building with MSVC, exporting a symbol means that the object file
   contains a "linker directive" of the form /EXPORT:symbol.  This can be
   inspected through the "objdump -s --section=.drectve FILE" or
   "dumpbin /directives FILE" commands.
   The symbols from this file should be exported if and only if the object
   file gets included in a DLL.  Libtool, on Windows platforms, defines
   the C macro DLL_EXPORT (together with PIC) when compiling for a shared
   library (called DLL under Windows) and does not define it when compiling
   an object file meant to be linked statically into some executable.  */
# if defined DLL_EXPORT
#  define LIBCHARSET_SHLIB_EXPORTED __declspec(dllexport)
# else
#  define LIBCHARSET_SHLIB_EXPORTED
# endif
#else
# define LIBCHARSET_SHLIB_EXPORTED
#endif

#include <localcharset.h>


#ifdef __cplusplus
extern "C" {
#endif


/* Support for relocatable packages.  */

/* Sets the original and the current installation prefix of the package.
   Relocation simply replaces a pathname starting with the original prefix
   by the corresponding pathname with the current prefix instead.  Both
   prefixes should be directory names without trailing slash (i.e. use ""
   instead of "/").  */
extern LIBCHARSET_SHLIB_EXPORTED void libcharset_set_relocation_prefix (const char *orig_prefix,
                                              const char *curr_prefix);


#ifdef __cplusplus
}
#endif


#endif /* _LIBCHARSET_H */
