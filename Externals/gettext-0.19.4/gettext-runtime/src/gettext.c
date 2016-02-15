/* gettext - retrieve text string from message catalog and print it.
   Copyright (C) 1995-1997, 2000-2007, 2012 Free Software Foundation, Inc.
   Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>, May 1995.

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "closeout.h"
#include "error.h"
#include "progname.h"
#include "relocatable.h"
#include "basename.h"
#include "xalloc.h"
#include "propername.h"
#include "gettext.h"

#define _(str) gettext (str)

/* If true, add newline after last string.  This makes only sense in
   the 'echo' emulation mode.  */
static bool add_newline;

/* If true, expand escape sequences in strings before looking in the
   message catalog.  */
static bool do_expand;

/* Long options.  */
static const struct option long_options[] =
{
  { "domain", required_argument, NULL, 'd' },
  { "help", no_argument, NULL, 'h' },
  { "shell-script", no_argument, NULL, 's' },
  { "version", no_argument, NULL, 'V' },
  { NULL, 0, NULL, 0 }
};

/* Forward declaration of local functions.  */
static void usage (int status)
#if defined __GNUC__ && ((__GNUC__ == 2 && __GNUC_MINOR__ >= 5) || __GNUC__ > 2)
     __attribute__ ((noreturn))
#endif
;
static const char *expand_escape (const char *str);

int
main (int argc, char *argv[])
{
  int optchar;
  const char *msgid;

  /* Default values for command line options.  */
  bool do_help = false;
  bool do_shell = false;
  bool do_version = false;
  const char *domain = getenv ("TEXTDOMAIN");
  const char *domaindir = getenv ("TEXTDOMAINDIR");
  add_newline = true;
  do_expand = false;

  /* Set program name for message texts.  */
  set_program_name (argv[0]);

#ifdef HAVE_SETLOCALE
  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");
#endif

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, relocate (LOCALEDIR));
  textdomain (PACKAGE);

  /* Ensure that write errors on stdout are detected.  */
  atexit (close_stdout);

  /* Parse command line options.  */
  while ((optchar = getopt_long (argc, argv, "+d:eEhnsV", long_options, NULL))
         != EOF)
    switch (optchar)
    {
    case '\0':          /* Long option.  */
      break;
    case 'd':
      domain = optarg;
      break;
    case 'e':
      do_expand = true;
      break;
    case 'E':
      /* Ignore.  Just for compatibility.  */
      break;
    case 'h':
      do_help = true;
      break;
    case 'n':
      add_newline = false;
      break;
    case 's':
      do_shell = true;
      break;
    case 'V':
      do_version = true;
      break;
    default:
      usage (EXIT_FAILURE);
    }

  /* Version information is requested.  */
  if (do_version)
    {
      printf ("%s (GNU %s) %s\n", basename (program_name), PACKAGE, VERSION);
      /* xgettext: no-wrap */
      printf (_("Copyright (C) %s Free Software Foundation, Inc.\n\
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>\n\
This is free software: you are free to change and redistribute it.\n\
There is NO WARRANTY, to the extent permitted by law.\n\
"),
              "1995-1997, 2000-2007");
      printf (_("Written by %s.\n"), proper_name ("Ulrich Drepper"));
      exit (EXIT_SUCCESS);
    }

  /* Help is requested.  */
  if (do_help)
    usage (EXIT_SUCCESS);

  /* We have two major modes: use following Uniforum spec and as
     internationalized 'echo' program.  */
  if (!do_shell)
    {
      /* We have to write a single strings translation to stdout.  */

      /* Get arguments.  */
      switch (argc - optind)
        {
          default:
            error (EXIT_FAILURE, 0, _("too many arguments"));

          case 2:
            domain = argv[optind++];
            /* FALLTHROUGH */

          case 1:
            break;

          case 0:
            error (EXIT_FAILURE, 0, _("missing arguments"));
        }

      msgid = argv[optind++];

      /* Expand escape sequences if enabled.  */
      if (do_expand)
        msgid = expand_escape (msgid);

      /* If no domain name is given we don't translate.  */
      if (domain == NULL || domain[0] == '\0')
        {
          fputs (msgid, stdout);
        }
      else
        {
          /* Bind domain to appropriate directory.  */
          if (domaindir != NULL && domaindir[0] != '\0')
            bindtextdomain (domain, domaindir);

          /* Write out the result.  */
          fputs (dgettext (domain, msgid), stdout);
        }
    }
  else
    {
      if (optind < argc)
        {
          /* If no domain name is given we print the original string.
             We mark this assigning NULL to domain.  */
          if (domain == NULL || domain[0] == '\0')
            domain = NULL;
          else
            /* Bind domain to appropriate directory.  */
            if (domaindir != NULL && domaindir[0] != '\0')
              bindtextdomain (domain, domaindir);

          /* We have to simulate 'echo'.  All arguments are strings.  */
          do
            {
              msgid = argv[optind++];

              /* Expand escape sequences if enabled.  */
              if (do_expand)
                msgid = expand_escape (msgid);

              /* Write out the result.  */
              fputs (domain == NULL ? msgid : dgettext (domain, msgid),
                     stdout);

              /* We separate the arguments by a single ' '.  */
              if (optind < argc)
                fputc (' ', stdout);
            }
          while (optind < argc);
        }

      /* If not otherwise told: add trailing newline.  */
      if (add_newline)
        fputc ('\n', stdout);
    }

  exit (EXIT_SUCCESS);
}


/* Display usage information and exit.  */
static void
usage (int status)
{
  if (status != EXIT_SUCCESS)
    fprintf (stderr, _("Try '%s --help' for more information.\n"),
             program_name);
  else
    {
      /* xgettext: no-wrap */
      printf (_("\
Usage: %s [OPTION] [[TEXTDOMAIN] MSGID]\n\
or:    %s [OPTION] -s [MSGID]...\n\
"), program_name, program_name);
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Display native language translation of a textual message.\n"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
  -d, --domain=TEXTDOMAIN   retrieve translated messages from TEXTDOMAIN\n\
  -e                        enable expansion of some escape sequences\n\
  -E                        (ignored for compatibility)\n\
  -h, --help                display this help and exit\n\
  -n                        suppress trailing newline\n\
  -V, --version             display version information and exit\n\
  [TEXTDOMAIN] MSGID        retrieve translated message corresponding\n\
                            to MSGID from TEXTDOMAIN\n"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
If the TEXTDOMAIN parameter is not given, the domain is determined from the\n\
environment variable TEXTDOMAIN.  If the message catalog is not found in the\n\
regular directory, another location can be specified with the environment\n\
variable TEXTDOMAINDIR.\n\
When used with the -s option the program behaves like the 'echo' command.\n\
But it does not simply copy its arguments to stdout.  Instead those messages\n\
found in the selected catalog are translated.\n\
Standard search directory: %s\n"),
              getenv ("IN_HELP2MAN") == NULL ? LOCALEDIR : "@localedir@");
      printf ("\n");
      /* TRANSLATORS: The placeholder indicates the bug-reporting address
         for this package.  Please add _another line_ saying
         "Report translation bugs to <...>\n" with the address for translation
         bugs (typically your translation team's web or email address).  */
      fputs (_("Report bugs to <bug-gnu-gettext@gnu.org>.\n"), stdout);
    }

  exit (status);
}


/* Expand some escape sequences found in the argument string.  */
static const char *
expand_escape (const char *str)
{
  char *retval, *rp;
  const char *cp = str;

  for (;;)
    {
      while (cp[0] != '\0' && cp[0] != '\\')
        ++cp;
      if (cp[0] == '\0')
        return str;
      /* Found a backslash.  */
      if (cp[1] == '\0')
        return str;
      if (strchr ("abcfnrtv\\01234567", cp[1]) != NULL)
        break;
      ++cp;
    }

  retval = XNMALLOC (strlen (str), char);

  rp = retval + (cp - str);
  memcpy (retval, str, cp - str);

  do
    {
      /* Here cp[0] == '\\'.  */
      switch (*++cp)
        {
        case 'a':               /* alert */
          *rp++ = '\a';
          ++cp;
          break;
        case 'b':               /* backspace */
          *rp++ = '\b';
          ++cp;
          break;
        case 'c':               /* suppress trailing newline */
          add_newline = false;
          ++cp;
          break;
        case 'f':               /* form feed */
          *rp++ = '\f';
          ++cp;
          break;
        case 'n':               /* new line */
          *rp++ = '\n';
          ++cp;
          break;
        case 'r':               /* carriage return */
          *rp++ = '\r';
          ++cp;
          break;
        case 't':               /* horizontal tab */
          *rp++ = '\t';
          ++cp;
          break;
        case 'v':               /* vertical tab */
          *rp++ = '\v';
          ++cp;
          break;
        case '\\':
          *rp = '\\';
          ++cp;
          break;
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
          {
            int ch = *cp++ - '0';

            if (*cp >= '0' && *cp <= '7')
              {
                ch *= 8;
                ch += *cp++ - '0';

                if (*cp >= '0' && *cp <= '7')
                  {
                    ch *= 8;
                    ch += *cp++ - '0';
                  }
              }
            *rp = ch;
          }
          break;
        default:
          *rp = '\\';
          break;
        }

      while (cp[0] != '\0' && cp[0] != '\\')
        *rp++ = *cp++;
    }
  while (cp[0] != '\0');

  /* Terminate string.  */
  *rp = '\0';

  return (const char *) retval;
}
