/* Reading symbolic links without size limitation.

   Copyright (C) 2001, 2003-2004, 2007, 2009-2014 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Jim Meyering <jim@meyering.net>  */

extern char *xreadlink (char const *filename);

#if GNULIB_XREADLINKAT
extern char *xreadlinkat (int fd, char const *filename);
#endif
