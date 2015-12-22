/* Localization of proper names.
   Copyright (C) 2006-2014 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2006.

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

/* Without this pragma, gcc 4.7.0 20111124 mistakenly suggests that
   the proper_name function might be candidate for attribute 'const'  */
#if (__GNUC__ == 4 && 6 <= __GNUC_MINOR__) || 4 < __GNUC__
# pragma GCC diagnostic ignored "-Wsuggest-attribute=const"
#endif

#include <config.h>

/* Specification.  */
#include "propername.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_ICONV
# include <iconv.h>
#endif

#include "trim.h"
#include "mbchar.h"
#include "mbuiter.h"
#include "localcharset.h"
#include "c-strcase.h"
#include "xstriconv.h"
#include "xalloc.h"
#include "gettext.h"


/* Tests whether STRING contains trim (SUB), starting and ending at word
   boundaries.
   Here, instead of implementing Unicode Standard Annex #29 for determining
   word boundaries, we assume that trim (SUB) starts and ends with words and
   only test whether the part before it ends with a non-word and the part
   after it starts with a non-word.  */
static bool
mbsstr_trimmed_wordbounded (const char *string, const char *sub)
{
  char *tsub = trim (sub);
  bool found = false;

  for (; *string != '\0';)
    {
      const char *tsub_in_string = mbsstr (string, tsub);
      if (tsub_in_string == NULL)
        break;
      else
        {
          if (MB_CUR_MAX > 1)
            {
              mbui_iterator_t string_iter;
              bool word_boundary_before;
              bool word_boundary_after;

              mbui_init (string_iter, string);
              word_boundary_before = true;
              if (mbui_cur_ptr (string_iter) < tsub_in_string)
                {
                  mbchar_t last_char_before_tsub;
                  do
                    {
                      if (!mbui_avail (string_iter))
                        abort ();
                      last_char_before_tsub = mbui_cur (string_iter);
                      mbui_advance (string_iter);
                    }
                  while (mbui_cur_ptr (string_iter) < tsub_in_string);
                  if (mb_isalnum (last_char_before_tsub))
                    word_boundary_before = false;
                }

              mbui_init (string_iter, tsub_in_string);
              {
                mbui_iterator_t tsub_iter;

                for (mbui_init (tsub_iter, tsub);
                     mbui_avail (tsub_iter);
                     mbui_advance (tsub_iter))
                  {
                    if (!mbui_avail (string_iter))
                      abort ();
                    mbui_advance (string_iter);
                  }
              }
              word_boundary_after = true;
              if (mbui_avail (string_iter))
                {
                  mbchar_t first_char_after_tsub = mbui_cur (string_iter);
                  if (mb_isalnum (first_char_after_tsub))
                    word_boundary_after = false;
                }

              if (word_boundary_before && word_boundary_after)
                {
                  found = true;
                  break;
                }

              mbui_init (string_iter, tsub_in_string);
              if (!mbui_avail (string_iter))
                break;
              string = tsub_in_string + mb_len (mbui_cur (string_iter));
            }
          else
            {
              bool word_boundary_before;
              const char *p;
              bool word_boundary_after;

              word_boundary_before = true;
              if (string < tsub_in_string)
                if (isalnum ((unsigned char) tsub_in_string[-1]))
                  word_boundary_before = false;

              p = tsub_in_string + strlen (tsub);
              word_boundary_after = true;
              if (*p != '\0')
                if (isalnum ((unsigned char) *p))
                  word_boundary_after = false;

              if (word_boundary_before && word_boundary_after)
                {
                  found = true;
                  break;
                }

              if (*tsub_in_string == '\0')
                break;
              string = tsub_in_string + 1;
            }
        }
    }
  free (tsub);
  return found;
}

/* Return the localization of NAME.  NAME is written in ASCII.  */

const char *
proper_name (const char *name)
{
  /* See whether there is a translation.   */
  const char *translation = gettext (name);

  if (translation != name)
    {
      /* See whether the translation contains the original name.  */
      if (mbsstr_trimmed_wordbounded (translation, name))
        return translation;
      else
        {
          /* Return "TRANSLATION (NAME)".  */
          char *result =
            XNMALLOC (strlen (translation) + 2 + strlen (name) + 1 + 1, char);

          sprintf (result, "%s (%s)", translation, name);
          return result;
        }
    }
  else
    return name;
}

/* Return the localization of a name whose original writing is not ASCII.
   NAME_UTF8 is the real name, written in UTF-8 with octal or hexadecimal
   escape sequences.  NAME_ASCII is a fallback written only with ASCII
   characters.  */

const char *
proper_name_utf8 (const char *name_ascii, const char *name_utf8)
{
  /* See whether there is a translation.   */
  const char *translation = gettext (name_ascii);

  /* Try to convert NAME_UTF8 to the locale encoding.  */
  const char *locale_code = locale_charset ();
  char *alloc_name_converted = NULL;
  char *alloc_name_converted_translit = NULL;
  const char *name_converted = NULL;
  const char *name_converted_translit = NULL;
  const char *name;

  if (c_strcasecmp (locale_code, "UTF-8") != 0)
    {
#if HAVE_ICONV
      name_converted = alloc_name_converted =
        xstr_iconv (name_utf8, "UTF-8", locale_code);

# if (((__GLIBC__ == 2 && __GLIBC_MINOR__ >= 2) || __GLIBC__ > 2) \
      && !defined __UCLIBC__) \
     || _LIBICONV_VERSION >= 0x0105
      {
        char *converted_translit;

        size_t len = strlen (locale_code);
        char *locale_code_translit = XNMALLOC (len + 10 + 1, char);
        memcpy (locale_code_translit, locale_code, len);
        memcpy (locale_code_translit + len, "//TRANSLIT", 10 + 1);

        converted_translit =
          xstr_iconv (name_utf8, "UTF-8", locale_code_translit);

        free (locale_code_translit);

        if (converted_translit != NULL)
          {
#  if !_LIBICONV_VERSION
            /* Don't use the transliteration if it added question marks.
               glibc's transliteration falls back to question marks; libiconv's
               transliteration does not.
               mbschr is equivalent to strchr in this case.  */
            if (strchr (converted_translit, '?') != NULL)
              free (converted_translit);
            else
#  endif
              name_converted_translit = alloc_name_converted_translit =
                converted_translit;
          }
      }
# endif
#endif
    }
  else
    {
      name_converted = name_utf8;
      name_converted_translit = name_utf8;
    }

  /* The name in locale encoding.  */
  name = (name_converted != NULL ? name_converted :
          name_converted_translit != NULL ? name_converted_translit :
          name_ascii);

  /* See whether we have a translation.  Some translators have not understood
     that they should use the UTF-8 form of the name, if possible.  So if the
     translator provided a no-op translation, we ignore it.  */
  if (strcmp (translation, name_ascii) != 0)
    {
      /* See whether the translation contains the original name.  */
      if (mbsstr_trimmed_wordbounded (translation, name_ascii)
          || (name_converted != NULL
              && mbsstr_trimmed_wordbounded (translation, name_converted))
          || (name_converted_translit != NULL
              && mbsstr_trimmed_wordbounded (translation, name_converted_translit)))
        {
          if (alloc_name_converted != NULL)
            free (alloc_name_converted);
          if (alloc_name_converted_translit != NULL)
            free (alloc_name_converted_translit);
          return translation;
        }
      else
        {
          /* Return "TRANSLATION (NAME)".  */
          char *result =
            XNMALLOC (strlen (translation) + 2 + strlen (name) + 1 + 1, char);

          sprintf (result, "%s (%s)", translation, name);

          if (alloc_name_converted != NULL)
            free (alloc_name_converted);
          if (alloc_name_converted_translit != NULL)
            free (alloc_name_converted_translit);
          return result;
        }
    }
  else
    {
      if (alloc_name_converted != NULL && alloc_name_converted != name)
        free (alloc_name_converted);
      if (alloc_name_converted_translit != NULL
          && alloc_name_converted_translit != name)
        free (alloc_name_converted_translit);
      return name;
    }
}

#ifdef TEST1
# include <locale.h>
int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "");
  if (mbsstr_trimmed_wordbounded (argv[1], argv[2]))
    printf("found\n");
  return 0;
}
#endif

#ifdef TEST2
# include <locale.h>
# include <stdio.h>
int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "");
  printf ("%s\n", proper_name_utf8 ("Franc,ois Pinard", "Fran\303\247ois Pinard"));
  return 0;
}
#endif
