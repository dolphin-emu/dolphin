/* Counting the multibyte characters in a string.
   Copyright (C) 2007-2014 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2007.

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

#include <config.h>

/* Specification.  */
#include <string.h>

#include <stdlib.h>

#include "mbuiter.h"

/* Return the number of multibyte characters in the character string STRING.  */
size_t
mbslen (const char *string)
{
  if (MB_CUR_MAX > 1)
    {
      size_t count;
      mbui_iterator_t iter;

      count = 0;
      for (mbui_init (iter, string); mbui_avail (iter); mbui_advance (iter))
        count++;

      return count;
    }
  else
    return strlen (string);
}
