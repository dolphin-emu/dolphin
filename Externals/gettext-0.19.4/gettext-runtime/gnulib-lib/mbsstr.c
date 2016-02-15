/* Searching in a string.
   Copyright (C) 2005-2014 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2005.

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

#include <stdbool.h>
#include <stddef.h>  /* for NULL, in case a nonstandard string.h lacks it */

#include "malloca.h"
#include "mbuiter.h"

/* Knuth-Morris-Pratt algorithm.  */
#define UNIT unsigned char
#define CANON_ELEMENT(c) c
#include "str-kmp.h"

/* Knuth-Morris-Pratt algorithm.
   See http://en.wikipedia.org/wiki/Knuth-Morris-Pratt_algorithm
   Return a boolean indicating success:
   Return true and set *RESULTP if the search was completed.
   Return false if it was aborted because not enough memory was available.  */
static bool
knuth_morris_pratt_multibyte (const char *haystack, const char *needle,
                              const char **resultp)
{
  size_t m = mbslen (needle);
  mbchar_t *needle_mbchars;
  size_t *table;

  /* Allocate room for needle_mbchars and the table.  */
  void *memory = nmalloca (m, sizeof (mbchar_t) + sizeof (size_t));
  void *table_memory;
  if (memory == NULL)
    return false;
  needle_mbchars = memory;
  table_memory = needle_mbchars + m;
  table = table_memory;

  /* Fill needle_mbchars.  */
  {
    mbui_iterator_t iter;
    size_t j;

    j = 0;
    for (mbui_init (iter, needle); mbui_avail (iter); mbui_advance (iter), j++)
      mb_copy (&needle_mbchars[j], &mbui_cur (iter));
  }

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
        mbchar_t *b = &needle_mbchars[i - 1];

        for (;;)
          {
            /* Invariants: The inequality needle[x..i-1] != needle[0..i-1-x]
               is known to hold for x < i-1-j.
               Furthermore, if j>0: needle[i-1-j..i-2] = needle[0..j-1].  */
            if (mb_equal (*b, needle_mbchars[j]))
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
    mbui_iterator_t rhaystack;
    mbui_iterator_t phaystack;

    *resultp = NULL;
    j = 0;
    mbui_init (rhaystack, haystack);
    mbui_init (phaystack, haystack);
    /* Invariant: phaystack = rhaystack + j.  */
    while (mbui_avail (phaystack))
      if (mb_equal (needle_mbchars[j], mbui_cur (phaystack)))
        {
          j++;
          mbui_advance (phaystack);
          if (j == m)
            {
              /* The entire needle has been found.  */
              *resultp = mbui_cur_ptr (rhaystack);
              break;
            }
        }
      else if (j > 0)
        {
          /* Found a match of needle[0..j-1], mismatch at needle[j].  */
          size_t count = table[j];
          j -= count;
          for (; count > 0; count--)
            {
              if (!mbui_avail (rhaystack))
                abort ();
              mbui_advance (rhaystack);
            }
        }
      else
        {
          /* Found a mismatch at needle[0] already.  */
          if (!mbui_avail (rhaystack))
            abort ();
          mbui_advance (rhaystack);
          mbui_advance (phaystack);
        }
  }

  freea (memory);
  return true;
}

/* Find the first occurrence of the character string NEEDLE in the character
   string HAYSTACK.  Return NULL if NEEDLE is not found in HAYSTACK.  */
char *
mbsstr (const char *haystack, const char *needle)
{
  /* Be careful not to look at the entire extent of haystack or needle
     until needed.  This is useful because of these two cases:
       - haystack may be very long, and a match of needle found early,
       - needle may be very long, and not even a short initial segment of
         needle may be found in haystack.  */
  if (MB_CUR_MAX > 1)
    {
      mbui_iterator_t iter_needle;

      mbui_init (iter_needle, needle);
      if (mbui_avail (iter_needle))
        {
          /* Minimizing the worst-case complexity:
             Let n = mbslen(haystack), m = mbslen(needle).
             The naïve algorithm is O(n*m) worst-case.
             The Knuth-Morris-Pratt algorithm is O(n) worst-case but it needs a
             memory allocation.
             To achieve linear complexity and yet amortize the cost of the
             memory allocation, we activate the Knuth-Morris-Pratt algorithm
             only once the naïve algorithm has already run for some time; more
             precisely, when
               - the outer loop count is >= 10,
               - the average number of comparisons per outer loop is >= 5,
               - the total number of comparisons is >= m.
             But we try it only once.  If the memory allocation attempt failed,
             we don't retry it.  */
          bool try_kmp = true;
          size_t outer_loop_count = 0;
          size_t comparison_count = 0;
          size_t last_ccount = 0;                  /* last comparison count */
          mbui_iterator_t iter_needle_last_ccount; /* = needle + last_ccount */

          mbui_iterator_t iter_haystack;

          mbui_init (iter_needle_last_ccount, needle);
          mbui_init (iter_haystack, haystack);
          for (;; mbui_advance (iter_haystack))
            {
              if (!mbui_avail (iter_haystack))
                /* No match.  */
                return NULL;

              /* See whether it's advisable to use an asymptotically faster
                 algorithm.  */
              if (try_kmp
                  && outer_loop_count >= 10
                  && comparison_count >= 5 * outer_loop_count)
                {
                  /* See if needle + comparison_count now reaches the end of
                     needle.  */
                  size_t count = comparison_count - last_ccount;
                  for (;
                       count > 0 && mbui_avail (iter_needle_last_ccount);
                       count--)
                    mbui_advance (iter_needle_last_ccount);
                  last_ccount = comparison_count;
                  if (!mbui_avail (iter_needle_last_ccount))
                    {
                      /* Try the Knuth-Morris-Pratt algorithm.  */
                      const char *result;
                      bool success =
                        knuth_morris_pratt_multibyte (haystack, needle,
                                                      &result);
                      if (success)
                        return (char *) result;
                      try_kmp = false;
                    }
                }

              outer_loop_count++;
              comparison_count++;
              if (mb_equal (mbui_cur (iter_haystack), mbui_cur (iter_needle)))
                /* The first character matches.  */
                {
                  mbui_iterator_t rhaystack;
                  mbui_iterator_t rneedle;

                  memcpy (&rhaystack, &iter_haystack, sizeof (mbui_iterator_t));
                  mbui_advance (rhaystack);

                  mbui_init (rneedle, needle);
                  if (!mbui_avail (rneedle))
                    abort ();
                  mbui_advance (rneedle);

                  for (;; mbui_advance (rhaystack), mbui_advance (rneedle))
                    {
                      if (!mbui_avail (rneedle))
                        /* Found a match.  */
                        return (char *) mbui_cur_ptr (iter_haystack);
                      if (!mbui_avail (rhaystack))
                        /* No match.  */
                        return NULL;
                      comparison_count++;
                      if (!mb_equal (mbui_cur (rhaystack), mbui_cur (rneedle)))
                        /* Nothing in this round.  */
                        break;
                    }
                }
            }
        }
      else
        return (char *) haystack;
    }
  else
    {
      if (*needle != '\0')
        {
          /* Minimizing the worst-case complexity:
             Let n = strlen(haystack), m = strlen(needle).
             The naïve algorithm is O(n*m) worst-case.
             The Knuth-Morris-Pratt algorithm is O(n) worst-case but it needs a
             memory allocation.
             To achieve linear complexity and yet amortize the cost of the
             memory allocation, we activate the Knuth-Morris-Pratt algorithm
             only once the naïve algorithm has already run for some time; more
             precisely, when
               - the outer loop count is >= 10,
               - the average number of comparisons per outer loop is >= 5,
               - the total number of comparisons is >= m.
             But we try it only once.  If the memory allocation attempt failed,
             we don't retry it.  */
          bool try_kmp = true;
          size_t outer_loop_count = 0;
          size_t comparison_count = 0;
          size_t last_ccount = 0;                  /* last comparison count */
          const char *needle_last_ccount = needle; /* = needle + last_ccount */

          /* Speed up the following searches of needle by caching its first
             character.  */
          char b = *needle++;

          for (;; haystack++)
            {
              if (*haystack == '\0')
                /* No match.  */
                return NULL;

              /* See whether it's advisable to use an asymptotically faster
                 algorithm.  */
              if (try_kmp
                  && outer_loop_count >= 10
                  && comparison_count >= 5 * outer_loop_count)
                {
                  /* See if needle + comparison_count now reaches the end of
                     needle.  */
                  if (needle_last_ccount != NULL)
                    {
                      needle_last_ccount +=
                        strnlen (needle_last_ccount,
                                 comparison_count - last_ccount);
                      if (*needle_last_ccount == '\0')
                        needle_last_ccount = NULL;
                      last_ccount = comparison_count;
                    }
                  if (needle_last_ccount == NULL)
                    {
                      /* Try the Knuth-Morris-Pratt algorithm.  */
                      const unsigned char *result;
                      bool success =
                        knuth_morris_pratt ((const unsigned char *) haystack,
                                            (const unsigned char *) (needle - 1),
                                            strlen (needle - 1),
                                            &result);
                      if (success)
                        return (char *) result;
                      try_kmp = false;
                    }
                }

              outer_loop_count++;
              comparison_count++;
              if (*haystack == b)
                /* The first character matches.  */
                {
                  const char *rhaystack = haystack + 1;
                  const char *rneedle = needle;

                  for (;; rhaystack++, rneedle++)
                    {
                      if (*rneedle == '\0')
                        /* Found a match.  */
                        return (char *) haystack;
                      if (*rhaystack == '\0')
                        /* No match.  */
                        return NULL;
                      comparison_count++;
                      if (*rhaystack != *rneedle)
                        /* Nothing in this round.  */
                        break;
                    }
                }
            }
        }
      else
        return (char *) haystack;
    }
}
