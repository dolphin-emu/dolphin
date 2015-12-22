/* Class autosprintf - formatted output to an ostream.
   Copyright (C) 2002, 2013 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2002.

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

/* Tell glibc's <stdio.h> to provide a prototype for vasprintf().
   This must come before <config.h> because <config.h> may include
   <features.h>, and once <features.h> has been included, it's too late.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE    1
#endif

/* Specification.  */
#include "autosprintf.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "lib-asprintf.h"

/* std::swap() is in <utility> since C++11.  */
#if __cplusplus >= 201103L
# include <utility>
#else
# include <algorithm>
#endif

namespace gnu
{

  /* Constructor: takes a format string and the printf arguments.  */
  autosprintf::autosprintf (const char *format, ...)
  {
    va_list args;
    va_start (args, format);
    if (vasprintf (&str, format, args) < 0)
      str = NULL;
    va_end (args);
  }

  /* Copy constructor.  Necessary because the destructor is nontrivial.  */
  autosprintf::autosprintf (const autosprintf& src)
  {
    str = (src.str != NULL ? strdup (src.str) : NULL);
  }

  /* Copy constructor.  Necessary because the destructor is nontrivial.  */
  autosprintf& autosprintf::operator = (autosprintf copy)
  {
    std::swap (copy.str, this->str);
    return *this;
  }

  /* Destructor: frees the temporarily allocated string.  */
  autosprintf::~autosprintf ()
  {
    free (str);
  }

  /* Conversion to string.  */
  autosprintf::operator char * () const
  {
    if (str != NULL)
      {
        size_t length = strlen (str) + 1;
        char *copy = new char[length];
        memcpy (copy, str, length);
        return copy;
      }
    else
      return NULL;
  }
  autosprintf::operator std::string () const
  {
    return std::string (str ? str : "(error in autosprintf)");
  }
}
