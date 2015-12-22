/* Detect write error on a stream.
   Copyright (C) 2003, 2005-2006, 2009-2014 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2003.

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

/* There are two approaches for detecting a write error on a stream opened
   for writing:

     (a) Test the return value of every fwrite() or fprintf() call, and react
         immediately.
     (b) Just before fclose(), test the error indicator in the stream and
         the return value of the final fclose() call.

   The benefit of (a) is that non file related errors (such that ENOMEM during
   fprintf) and temporary error conditions can be diagnosed accurately.

   A theoretical benefit of (a) is also that, on POSIX systems, in the case of
   an ENOSPC error, errno is set and can be used by error() to provide a more
   accurate error message. But in practice, this benefit is not big because
   users can easily figure out by themselves why a file cannot be written to,
   and furthermore the function fwriteerror() can provide errno as well.

   The big drawback of (a) is extensive error checking code: Every function
   which does stream output must return an error indicator.

   This file provides support for (b).  */

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Write out the not yet written buffered contents of the stream FP, close
   the stream FP, and test whether some error occurred on the stream FP.
   FP must be a stream opened for writing.
   Return 0 if no error occurred and fclose (fp) succeeded.
   Return -1 and set errno if there was an error.  The errno value will be 0
   if the cause of the error cannot be determined.
   For any given stream FP other than stdout, fwriteerror (FP) may only be
   called once.  */
extern int fwriteerror (FILE *fp);

/* Likewise, but don't consider it an error if FP has an invalid file
   descriptor and no output was done to FP.  */
extern int fwriteerror_no_ebadf (FILE *fp);

#ifdef __cplusplus
}
#endif
