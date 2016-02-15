/* Charset conversion.
   Copyright (C) 2001-2007, 2010-2014 Free Software Foundation, Inc.
   Written by Bruno Haible and Simon Josefsson.

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

#include <config.h>

/* Specification.  */
#include "striconv.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_ICONV
# include <iconv.h>
/* Get MB_LEN_MAX, CHAR_BIT.  */
# include <limits.h>
#endif

#include "c-strcase.h"

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif


#if HAVE_ICONV

int
mem_cd_iconv (const char *src, size_t srclen, iconv_t cd,
              char **resultp, size_t *lengthp)
{
# define tmpbufsize 4096
  size_t length;
  char *result;

  /* Avoid glibc-2.1 bug and Solaris 2.7-2.9 bug.  */
# if defined _LIBICONV_VERSION \
     || !(((__GLIBC__ == 2 && __GLIBC_MINOR__ <= 1) && !defined __UCLIBC__) \
          || defined __sun)
  /* Set to the initial state.  */
  iconv (cd, NULL, NULL, NULL, NULL);
# endif

  /* Determine the length we need.  */
  {
    size_t count = 0;
    /* The alignment is needed when converting e.g. to glibc's WCHAR_T or
       libiconv's UCS-4-INTERNAL encoding.  */
    union { unsigned int align; char buf[tmpbufsize]; } tmp;
# define tmpbuf tmp.buf
    const char *inptr = src;
    size_t insize = srclen;

    while (insize > 0)
      {
        char *outptr = tmpbuf;
        size_t outsize = tmpbufsize;
        size_t res = iconv (cd,
                            (ICONV_CONST char **) &inptr, &insize,
                            &outptr, &outsize);

        if (res == (size_t)(-1))
          {
            if (errno == E2BIG)
              ;
            else if (errno == EINVAL)
              break;
            else
              return -1;
          }
# if !defined _LIBICONV_VERSION && !(defined __GLIBC__ && !defined __UCLIBC__)
        /* Irix iconv() inserts a NUL byte if it cannot convert.
           NetBSD iconv() inserts a question mark if it cannot convert.
           Only GNU libiconv and GNU libc are known to prefer to fail rather
           than doing a lossy conversion.  */
        else if (res > 0)
          {
            errno = EILSEQ;
            return -1;
          }
# endif
        count += outptr - tmpbuf;
      }
    /* Avoid glibc-2.1 bug and Solaris 2.7 bug.  */
# if defined _LIBICONV_VERSION \
     || !(((__GLIBC__ == 2 && __GLIBC_MINOR__ <= 1) && !defined __UCLIBC__) \
          || defined __sun)
    {
      char *outptr = tmpbuf;
      size_t outsize = tmpbufsize;
      size_t res = iconv (cd, NULL, NULL, &outptr, &outsize);

      if (res == (size_t)(-1))
        return -1;
      count += outptr - tmpbuf;
    }
# endif
    length = count;
# undef tmpbuf
  }

  if (length == 0)
    {
      *lengthp = 0;
      return 0;
    }
  if (*resultp != NULL && *lengthp >= length)
    result = *resultp;
  else
    {
      result = (char *) malloc (length);
      if (result == NULL)
        {
          errno = ENOMEM;
          return -1;
        }
    }

  /* Avoid glibc-2.1 bug and Solaris 2.7-2.9 bug.  */
# if defined _LIBICONV_VERSION \
     || !(((__GLIBC__ == 2 && __GLIBC_MINOR__ <= 1) && !defined __UCLIBC__) \
          || defined __sun)
  /* Return to the initial state.  */
  iconv (cd, NULL, NULL, NULL, NULL);
# endif

  /* Do the conversion for real.  */
  {
    const char *inptr = src;
    size_t insize = srclen;
    char *outptr = result;
    size_t outsize = length;

    while (insize > 0)
      {
        size_t res = iconv (cd,
                            (ICONV_CONST char **) &inptr, &insize,
                            &outptr, &outsize);

        if (res == (size_t)(-1))
          {
            if (errno == EINVAL)
              break;
            else
              goto fail;
          }
# if !defined _LIBICONV_VERSION && !(defined __GLIBC__ && !defined __UCLIBC__)
        /* Irix iconv() inserts a NUL byte if it cannot convert.
           NetBSD iconv() inserts a question mark if it cannot convert.
           Only GNU libiconv and GNU libc are known to prefer to fail rather
           than doing a lossy conversion.  */
        else if (res > 0)
          {
            errno = EILSEQ;
            goto fail;
          }
# endif
      }
    /* Avoid glibc-2.1 bug and Solaris 2.7 bug.  */
# if defined _LIBICONV_VERSION \
     || !(((__GLIBC__ == 2 && __GLIBC_MINOR__ <= 1) && !defined __UCLIBC__) \
          || defined __sun)
    {
      size_t res = iconv (cd, NULL, NULL, &outptr, &outsize);

      if (res == (size_t)(-1))
        goto fail;
    }
# endif
    if (outsize != 0)
      abort ();
  }

  *resultp = result;
  *lengthp = length;

  return 0;

 fail:
  {
    if (result != *resultp)
      {
        int saved_errno = errno;
        free (result);
        errno = saved_errno;
      }
    return -1;
  }
# undef tmpbufsize
}

char *
str_cd_iconv (const char *src, iconv_t cd)
{
  /* For most encodings, a trailing NUL byte in the input will be converted
     to a trailing NUL byte in the output.  But not for UTF-7.  So that this
     function is usable for UTF-7, we have to exclude the NUL byte from the
     conversion and add it by hand afterwards.  */
# if !defined _LIBICONV_VERSION && !(defined __GLIBC__ && !defined __UCLIBC__)
  /* Irix iconv() inserts a NUL byte if it cannot convert.
     NetBSD iconv() inserts a question mark if it cannot convert.
     Only GNU libiconv and GNU libc are known to prefer to fail rather
     than doing a lossy conversion.  For other iconv() implementations,
     we have to look at the number of irreversible conversions returned;
     but this information is lost when iconv() returns for an E2BIG reason.
     Therefore we cannot use the second, faster algorithm.  */

  char *result = NULL;
  size_t length = 0;
  int retval = mem_cd_iconv (src, strlen (src), cd, &result, &length);
  char *final_result;

  if (retval < 0)
    {
      if (result != NULL)
        abort ();
      return NULL;
    }

  /* Add the terminating NUL byte.  */
  final_result =
    (result != NULL ? realloc (result, length + 1) : malloc (length + 1));
  if (final_result == NULL)
    {
      free (result);
      errno = ENOMEM;
      return NULL;
    }
  final_result[length] = '\0';

  return final_result;

# else
  /* This algorithm is likely faster than the one above.  But it may produce
     iconv() returns for an E2BIG reason, when the output size guess is too
     small.  Therefore it can only be used when we don't need the number of
     irreversible conversions performed.  */
  char *result;
  size_t result_size;
  size_t length;
  const char *inptr = src;
  size_t inbytes_remaining = strlen (src);

  /* Make a guess for the worst-case output size, in order to avoid a
     realloc.  It's OK if the guess is wrong as long as it is not zero and
     doesn't lead to an integer overflow.  */
  result_size = inbytes_remaining;
  {
    size_t approx_sqrt_SIZE_MAX = SIZE_MAX >> (sizeof (size_t) * CHAR_BIT / 2);
    if (result_size <= approx_sqrt_SIZE_MAX / MB_LEN_MAX)
      result_size *= MB_LEN_MAX;
  }
  result_size += 1; /* for the terminating NUL */

  result = (char *) malloc (result_size);
  if (result == NULL)
    {
      errno = ENOMEM;
      return NULL;
    }

  /* Avoid glibc-2.1 bug and Solaris 2.7-2.9 bug.  */
# if defined _LIBICONV_VERSION \
     || !(((__GLIBC__ == 2 && __GLIBC_MINOR__ <= 1) && !defined __UCLIBC__) \
          || defined __sun)
  /* Set to the initial state.  */
  iconv (cd, NULL, NULL, NULL, NULL);
# endif

  /* Do the conversion.  */
  {
    char *outptr = result;
    size_t outbytes_remaining = result_size - 1;

    for (;;)
      {
        /* Here inptr + inbytes_remaining = src + strlen (src),
                outptr + outbytes_remaining = result + result_size - 1.  */
        size_t res = iconv (cd,
                            (ICONV_CONST char **) &inptr, &inbytes_remaining,
                            &outptr, &outbytes_remaining);

        if (res == (size_t)(-1))
          {
            if (errno == EINVAL)
              break;
            else if (errno == E2BIG)
              {
                size_t used = outptr - result;
                size_t newsize = result_size * 2;
                char *newresult;

                if (!(newsize > result_size))
                  {
                    errno = ENOMEM;
                    goto failed;
                  }
                newresult = (char *) realloc (result, newsize);
                if (newresult == NULL)
                  {
                    errno = ENOMEM;
                    goto failed;
                  }
                result = newresult;
                result_size = newsize;
                outptr = result + used;
                outbytes_remaining = result_size - 1 - used;
              }
            else
              goto failed;
          }
        else
          break;
      }
    /* Avoid glibc-2.1 bug and Solaris 2.7 bug.  */
# if defined _LIBICONV_VERSION \
     || !(((__GLIBC__ == 2 && __GLIBC_MINOR__ <= 1) && !defined __UCLIBC__) \
          || defined __sun)
    for (;;)
      {
        /* Here outptr + outbytes_remaining = result + result_size - 1.  */
        size_t res = iconv (cd, NULL, NULL, &outptr, &outbytes_remaining);

        if (res == (size_t)(-1))
          {
            if (errno == E2BIG)
              {
                size_t used = outptr - result;
                size_t newsize = result_size * 2;
                char *newresult;

                if (!(newsize > result_size))
                  {
                    errno = ENOMEM;
                    goto failed;
                  }
                newresult = (char *) realloc (result, newsize);
                if (newresult == NULL)
                  {
                    errno = ENOMEM;
                    goto failed;
                  }
                result = newresult;
                result_size = newsize;
                outptr = result + used;
                outbytes_remaining = result_size - 1 - used;
              }
            else
              goto failed;
          }
        else
          break;
      }
# endif

    /* Add the terminating NUL byte.  */
    *outptr++ = '\0';

    length = outptr - result;
  }

  /* Give away unused memory.  */
  if (length < result_size)
    {
      char *smaller_result = (char *) realloc (result, length);

      if (smaller_result != NULL)
        result = smaller_result;
    }

  return result;

 failed:
  {
    int saved_errno = errno;
    free (result);
    errno = saved_errno;
    return NULL;
  }

# endif
}

#endif

char *
str_iconv (const char *src, const char *from_codeset, const char *to_codeset)
{
  if (*src == '\0' || c_strcasecmp (from_codeset, to_codeset) == 0)
    {
      char *result = strdup (src);

      if (result == NULL)
        errno = ENOMEM;
      return result;
    }
  else
    {
#if HAVE_ICONV
      iconv_t cd;
      char *result;

      /* Avoid glibc-2.1 bug with EUC-KR.  */
# if ((__GLIBC__ == 2 && __GLIBC_MINOR__ <= 1) && !defined __UCLIBC__) \
     && !defined _LIBICONV_VERSION
      if (c_strcasecmp (from_codeset, "EUC-KR") == 0
          || c_strcasecmp (to_codeset, "EUC-KR") == 0)
        {
          errno = EINVAL;
          return NULL;
        }
# endif
      cd = iconv_open (to_codeset, from_codeset);
      if (cd == (iconv_t) -1)
        return NULL;

      result = str_cd_iconv (src, cd);

      if (result == NULL)
        {
          /* Close cd, but preserve the errno from str_cd_iconv.  */
          int saved_errno = errno;
          iconv_close (cd);
          errno = saved_errno;
        }
      else
        {
          if (iconv_close (cd) < 0)
            {
              /* Return NULL, but free the allocated memory, and while doing
                 that, preserve the errno from iconv_close.  */
              int saved_errno = errno;
              free (result);
              errno = saved_errno;
              return NULL;
            }
        }
      return result;
#else
      /* This is a different error code than if iconv_open existed but didn't
         support from_codeset and to_codeset, so that the caller can emit
         an error message such as
           "iconv() is not supported. Installing GNU libiconv and
            then reinstalling this package would fix this."  */
      errno = ENOSYS;
      return NULL;
#endif
    }
}
