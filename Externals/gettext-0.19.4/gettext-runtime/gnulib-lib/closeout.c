/* closeout.c - close standard output and standard error
   Copyright (C) 1998-2007, 2012 Free Software Foundation, Inc.

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
#include "closeout.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "error.h"
#include "fwriteerror.h"
#include "gettext.h"

#define _(msgid) gettext (msgid)

/* Close standard output, exiting with status STATUS on failure.
   If a program writes *anything* to stdout, that program should close
   stdout and make sure that it succeeds before exiting.  Otherwise,
   suppose that you go to the extreme of checking the return status
   of every function that does an explicit write to stdout.  The last
   printf can succeed in writing to the internal stream buffer, and yet
   the fclose(stdout) could still fail (due e.g., to a disk full error)
   when it tries to write out that buffered data.  Thus, you would be
   left with an incomplete output file and the offending program would
   exit successfully.  Even calling fflush is not always sufficient,
   since some file systems (NFS and CODA) buffer written/flushed data
   until an actual close call.

   Besides, it's wasteful to check the return value from every call
   that writes to stdout -- just let the internal stream state record
   the failure.  That's what the ferror test is checking below.

   If the stdout file descriptor was initially closed (such as when executing
   a program through "program 1>&-"), it is a failure if and only if some
   output was made to stdout.

   Likewise for standard error.

   It's important to detect such failures and exit nonzero because many
   tools (most notably 'make' and other build-management systems) depend
   on being able to detect failure in other tools via their exit status.  */

/* Close standard output and standard error, exiting with status EXIT_FAILURE
   on failure.  */
void
close_stdout (void)
{
  /* Close standard output.  */
  if (fwriteerror_no_ebadf (stdout))
    error (EXIT_FAILURE, errno, "%s", _("write error"));

  /* Close standard error.  This is simpler than fwriteerror_no_ebadf, because
     upon failure we don't need an errno - all we can do at this point is to
     set an exit status.  */
  errno = 0;
  if (ferror (stderr) || fflush (stderr))
    {
      fclose (stderr);
      exit (EXIT_FAILURE);
    }
  if (fclose (stderr) && errno != EBADF)
    exit (EXIT_FAILURE);
}

/* Note: When exit (...) calls the atexit-registered
              close_stdout (), which calls
              error (status, ...), which calls
              exit (status),
   we have undefined behaviour according to ISO C 99 section 7.20.4.3.(2).
   But in practice there is no problem: The second exit call is executed
   at a moment when the atexit handlers are no longer active.  */
