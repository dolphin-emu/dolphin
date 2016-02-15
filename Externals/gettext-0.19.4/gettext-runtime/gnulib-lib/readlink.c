/* Stub for readlink().
   Copyright (C) 2003-2007, 2009-2014 Free Software Foundation, Inc.

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
#include <unistd.h>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#if !HAVE_READLINK

/* readlink() substitute for systems that don't have a readlink() function,
   such as DJGPP 2.03 and mingw32.  */

ssize_t
readlink (const char *name, char *buf _GL_UNUSED,
          size_t bufsize _GL_UNUSED)
{
  struct stat statbuf;

  /* In general we should use lstat() here, not stat().  But on platforms
     without symbolic links, lstat() - if it exists - would be equivalent to
     stat(), therefore we can use stat().  This saves us a configure check.  */
  if (stat (name, &statbuf) >= 0)
    errno = EINVAL;
  return -1;
}

#else /* HAVE_READLINK */

# undef readlink

/* readlink() wrapper that uses correct types, for systems like cygwin
   1.5.x where readlink returns int, and which rejects trailing slash,
   for Solaris 9.  */

ssize_t
rpl_readlink (const char *name, char *buf, size_t bufsize)
{
# if READLINK_TRAILING_SLASH_BUG
  size_t len = strlen (name);
  if (len && name[len - 1] == '/')
    {
      /* Even if name without the slash is a symlink to a directory,
         both lstat() and stat() must resolve the trailing slash to
         the directory rather than the symlink.  We can therefore
         safely use stat() to distinguish between EINVAL and
         ENOTDIR/ENOENT, avoiding extra overhead of rpl_lstat().  */
      struct stat st;
      if (stat (name, &st) == 0)
        errno = EINVAL;
      return -1;
    }
# endif /* READLINK_TRAILING_SLASH_BUG */
  return readlink (name, buf, bufsize);
}

#endif /* HAVE_READLINK */
