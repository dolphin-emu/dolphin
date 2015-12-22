/* Substring search in a NUL terminated string of UNIT elements,
   using the Knuth-Morris-Pratt algorithm.
   Copyright (C) 2005-2014 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2005.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.  */

/* Before including this file, you need to define:
     UNIT                    The element type of the needle and haystack.
     CANON_ELEMENT(c)        A macro that canonicalizes an element right after
                             it has been fetched from needle or haystack.
                             The argument is of type UNIT; the result must be
                             of type UNIT as well.  */

/* Knuth-Morris-Pratt algorithm.
   See http://en.wikipedia.org/wiki/Knuth-Morris-Pratt_algorithm
   HAYSTACK is the NUL terminated string in which to search for.
   NEEDLE is the string to search for in HAYSTACK, consisting of NEEDLE_LEN
   units.
   Return a boolean indicating success:
   Return true and set *RESULTP if the search was completed.
   Return false if it was aborted because not enough memory was available.  */
static bool
knuth_morris_pratt (const UNIT *haystack,
                    const UNIT *needle, size_t needle_len,
                    const UNIT **resultp)
{
  size_t m = needle_len;

  /* Allocate the table.  */
  size_t *table = (size_t *) nmalloca (m, sizeof (size_t));
  if (table == NULL)
    return false;
  /* Fill the table.
     For 0 < i < m:
       0 < table[i] <= i is defined such that
       forall 0 < x < table[i]: needle[x..i-1] != needle[0..i-1-x],
       and table[i] is as large as possible with this property.
     This implies:
     1) For 0 < i < m:
          If table[i] < i,
          needle[table[i]..i-1] = needle[0..i-1-table[i]].
     2) For 0 < i < m:
          rhaystack[0..i-1] == needle[0..i-1]
          and exists h, i <= h < m: rhaystack[h] != needle[h]
          implies
          forall 0 <= x < table[i]: rhaystack[x..x+m-1] != needle[0..m-1].
     table[0] remains uninitialized.  */
  {
    size_t i, j;

    /* i = 1: Nothing to verify for x = 0.  */
    table[1] = 1;
    j = 0;

    for (i = 2; i < m; i++)
      {
        /* Here: j = i-1 - table[i-1].
           The inequality needle[x..i-1] != needle[0..i-1-x] is known to hold
           for x < table[i-1], by induction.
           Furthermore, if j>0: needle[i-1-j..i-2] = needle[0..j-1].  */
        UNIT b = CANON_ELEMENT (needle[i - 1]);

        for (;;)
          {
            /* Invariants: The inequality needle[x..i-1] != needle[0..i-1-x]
               is known to hold for x < i-1-j.
               Furthermore, if j>0: needle[i-1-j..i-2] = needle[0..j-1].  */
            if (b == CANON_ELEMENT (needle[j]))
              {
                /* Set table[i] := i-1-j.  */
                table[i] = i - ++j;
                break;
              }
            /* The inequality needle[x..i-1] != needle[0..i-1-x] also holds
               for x = i-1-j, because
                 needle[i-1] != needle[j] = needle[i-1-x].  */
            if (j == 0)
              {
                /* The inequality holds for all possible x.  */
                table[i] = i;
                break;
              }
            /* The inequality needle[x..i-1] != needle[0..i-1-x] also holds
               for i-1-j < x < i-1-j+table[j], because for these x:
                 needle[x..i-2]
                 = needle[x-(i-1-j)..j-1]
                 != needle[0..j-1-(x-(i-1-j))]  (by definition of table[j])
                    = needle[0..i-2-x],
               hence needle[x..i-1] != needle[0..i-1-x].
               Furthermore
                 needle[i-1-j+table[j]..i-2]
                 = needle[table[j]..j-1]
                 = needle[0..j-1-table[j]]  (by definition of table[j]).  */
            j = j - table[j];
          }
        /* Here: j = i - table[i].  */
      }
  }

  /* Search, using the table to accelerate the processing.  */
  {
    size_t j;
    const UNIT *rhaystack;
    const UNIT *phaystack;

    *resultp = NULL;
    j = 0;
    rhaystack = haystack;
    phaystack = haystack;
    /* Invariant: phaystack = rhaystack + j.  */
    while (*phaystack != 0)
      if (CANON_ELEMENT (needle[j]) == CANON_ELEMENT (*phaystack))
        {
          j++;
          phaystack++;
          if (j == m)
            {
              /* The entire needle has been found.  */
              *resultp = rhaystack;
              break;
            }
        }
      else if (j > 0)
        {
          /* Found a match of needle[0..j-1], mismatch at needle[j].  */
          rhaystack += table[j];
          j -= table[j];
        }
      else
        {
          /* Found a mismatch at needle[0] already.  */
          rhaystack++;
          phaystack++;
        }
  }

  freea (table);
  return true;
}

#undef CANON_ELEMENT
