/* List of exported symbols of libintl on Cygwin.
   Copyright (C) 2006 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2006.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

 /* IMP(x) is a symbol that contains the address of x.  */
#if USER_LABEL_PREFIX_UNDERSCORE
# define IMP(x) _imp__##x
#else
# define IMP(x) __imp_##x
#endif

 /* Ensure that the variable x is exported from the library, and that a
    pseudo-variable IMP(x) is available.  */
#define VARIABLE(x) \
 /* Export x without redefining x.  This code was found by compiling a  \
    snippet:                                                            \
      extern __declspec(dllexport) int x; int x = 42;  */               \
 asm (".section .drectve\n");                                           \
 asm (".ascii \" -export:" #x ",data\"\n");                             \
 asm (".data\n");                                                       \
 /* Allocate a pseudo-variable IMP(x).  */                              \
 extern int x;                                                          \
 void * IMP(x) = &x;

VARIABLE(libintl_version)
