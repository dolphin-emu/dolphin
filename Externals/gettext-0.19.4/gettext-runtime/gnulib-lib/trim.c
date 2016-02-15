/* Removes leading and/or trailing whitespaces
   Copyright (C) 2006-2014 Free Software Foundation, Inc.

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

/* Written by Davide Angelocola <davide.angelocola@gmail.com> */

#include <config.h>

/* Specification.  */
#include "trim.h"

#include <ctype.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#include "mbchar.h"
#include "mbiter.h"
#include "xalloc.h"

/* Use this to suppress gcc's "...may be used before initialized" warnings. */
#ifdef lint
# define IF_LINT(Code) Code
#else
# define IF_LINT(Code) /* empty */
#endif

char *
trim2 (const char *s, int how)
{
  char *d;

  d = strdup (s);

  if (!d)
    xalloc_die ();

  if (MB_CUR_MAX > 1)
    {
      mbi_iterator_t i;

      /* Trim leading whitespaces. */
      if (how != TRIM_TRAILING)
        {
          mbi_init (i, d, strlen (d));

          for (; mbi_avail (i) && mb_isspace (mbi_cur (i)); mbi_advance (i))
            ;

          memmove (d, mbi_cur_ptr (i), strlen (mbi_cur_ptr (i)) + 1);
        }

      /* Trim trailing whitespaces. */
      if (how != TRIM_LEADING)
        {
          unsigned int state = 0;
          char *r IF_LINT (= NULL); /* used only while state = 2 */

          mbi_init (i, d, strlen (d));

          for (; mbi_avail (i); mbi_advance (i))
            {
              if (state == 0 && mb_isspace (mbi_cur (i)))
                continue;

              if (state == 0 && !mb_isspace (mbi_cur (i)))
                {
                  state = 1;
                  continue;
                }

              if (state == 1 && !mb_isspace (mbi_cur (i)))
                continue;

              if (state == 1 && mb_isspace (mbi_cur (i)))
                {
                  state = 2;
                  r = (char *) mbi_cur_ptr (i);
                }
              else if (state == 2 && mb_isspace (mbi_cur (i)))
                {
                  /* empty */
                }
              else
                {
                  state = 1;
                }
            }

          if (state == 2)
            *r = '\0';
        }
    }
  else
    {
      char *p;

      /* Trim leading whitespaces. */
      if (how != TRIM_TRAILING)
        {
          for (p = d; *p && isspace ((unsigned char) *p); p++)
            ;

          memmove (d, p, strlen (p) + 1);
        }

      /* Trim trailing whitespaces. */
      if (how != TRIM_LEADING)
        {
          for (p = d + strlen (d) - 1;
               p >= d && isspace ((unsigned char) *p); p--)
            *p = '\0';
        }
    }

  return d;
}
