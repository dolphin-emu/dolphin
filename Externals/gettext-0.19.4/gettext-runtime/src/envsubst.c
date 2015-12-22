/* Substitution of environment variables in shell format strings.
   Copyright (C) 2003-2007, 2012 Free Software Foundation, Inc.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
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

/* If true, substitution shall be performed on all variables.  */
static bool all_variables;

/* Long options.  */
static const struct option long_options[] =
{
  { "help", no_argument, NULL, 'h' },
  { "variables", no_argument, NULL, 'v' },
  { "version", no_argument, NULL, 'V' },
  { NULL, 0, NULL, 0 }
};

/* Forward declaration of local functions.  */
static void usage (int status)
#if defined __GNUC__ && ((__GNUC__ == 2 && __GNUC_MINOR__ >= 5) || __GNUC__ > 2)
     __attribute__ ((noreturn))
#endif
;
static void print_variables (const char *string);
static void note_variables (const char *string);
static void subst_from_stdin (void);

int
main (int argc, char *argv[])
{
  /* Default values for command line options.  */
  bool show_variables = false;
  bool do_help = false;
  bool do_version = false;

  int opt;

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
  while ((opt = getopt_long (argc, argv, "hvV", long_options, NULL)) != EOF)
    switch (opt)
    {
    case '\0':          /* Long option.  */
      break;
    case 'h':
      do_help = true;
      break;
    case 'v':
      show_variables = true;
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
              "2003-2007");
      printf (_("Written by %s.\n"), proper_name ("Bruno Haible"));
      exit (EXIT_SUCCESS);
    }

  /* Help is requested.  */
  if (do_help)
    usage (EXIT_SUCCESS);

  if (argc - optind > 1)
    error (EXIT_FAILURE, 0, _("too many arguments"));

  /* Distinguish the two main operation modes.  */
  if (show_variables)
    {
      /* Output only the variables.  */
      switch (argc - optind)
        {
        case 1:
          break;
        case 0:
          error (EXIT_FAILURE, 0, _("missing arguments"));
        default:
          abort ();
        }
      print_variables (argv[optind++]);
    }
  else
    {
      /* Actually perform the substitutions.  */
      switch (argc - optind)
        {
        case 1:
          all_variables = false;
          note_variables (argv[optind++]);
          break;
        case 0:
          all_variables = true;
          break;
        default:
          abort ();
        }
      subst_from_stdin ();
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
Usage: %s [OPTION] [SHELL-FORMAT]\n\
"), program_name);
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Substitutes the values of environment variables.\n"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Operation mode:\n"));
      /* xgettext: no-wrap */
      printf (_("\
  -v, --variables             output the variables occurring in SHELL-FORMAT\n"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
Informative output:\n"));
      /* xgettext: no-wrap */
      printf (_("\
  -h, --help                  display this help and exit\n"));
      /* xgettext: no-wrap */
      printf (_("\
  -V, --version               output version information and exit\n"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
In normal operation mode, standard input is copied to standard output,\n\
with references to environment variables of the form $VARIABLE or ${VARIABLE}\n\
being replaced with the corresponding values.  If a SHELL-FORMAT is given,\n\
only those environment variables that are referenced in SHELL-FORMAT are\n\
substituted; otherwise all environment variables references occurring in\n\
standard input are substituted.\n"));
      printf ("\n");
      /* xgettext: no-wrap */
      printf (_("\
When --variables is used, standard input is ignored, and the output consists\n\
of the environment variables that are referenced in SHELL-FORMAT, one per line.\n"));
      printf ("\n");
      /* TRANSLATORS: The placeholder indicates the bug-reporting address
         for this package.  Please add _another line_ saying
         "Report translation bugs to <...>\n" with the address for translation
         bugs (typically your translation team's web or email address).  */
      fputs (_("Report bugs to <bug-gnu-gettext@gnu.org>.\n"), stdout);
    }

  exit (status);
}


/* Parse the string and invoke the callback each time a $VARIABLE or
   ${VARIABLE} construct is seen, where VARIABLE is a nonempty sequence
   of ASCII alphanumeric/underscore characters, starting with an ASCII
   alphabetic/underscore character.
   We allow only ASCII characters, to avoid dependencies w.r.t. the current
   encoding: While "${\xe0}" looks like a variable access in ISO-8859-1
   encoding, it doesn't look like one in the BIG5, BIG5-HKSCS, GBK, GB18030,
   SHIFT_JIS, JOHAB encodings, because \xe0\x7d is a single character in these
   encodings.  */
static void
find_variables (const char *string,
                void (*callback) (const char *var_ptr, size_t var_len))
{
  for (; *string != '\0';)
    if (*string++ == '$')
      {
        const char *variable_start;
        const char *variable_end;
        bool valid;
        char c;

        if (*string == '{')
          string++;

        variable_start = string;
        c = *string;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')
          {
            do
              c = *++string;
            while ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                   || (c >= '0' && c <= '9') || c == '_');
            variable_end = string;

            if (variable_start[-1] == '{')
              {
                if (*string == '}')
                  {
                    string++;
                    valid = true;
                  }
                else
                  valid = false;
              }
            else
              valid = true;

            if (valid)
              callback (variable_start, variable_end - variable_start);
          }
      }
}


/* Print a variable to stdout, followed by a newline.  */
static void
print_variable (const char *var_ptr, size_t var_len)
{
  fwrite (var_ptr, var_len, 1, stdout);
  putchar ('\n');
}

/* Print the variables contained in STRING to stdout, each one followed by a
   newline.  */
static void
print_variables (const char *string)
{
  find_variables (string, &print_variable);
}


/* Type describing list of immutable strings,
   implemented using a dynamic array.  */
typedef struct string_list_ty string_list_ty;
struct string_list_ty
{
  const char **item;
  size_t nitems;
  size_t nitems_max;
};

/* Initialize an empty list of strings.  */
static inline void
string_list_init (string_list_ty *slp)
{
  slp->item = NULL;
  slp->nitems = 0;
  slp->nitems_max = 0;
}

/* Append a single string to the end of a list of strings.  */
static inline void
string_list_append (string_list_ty *slp, const char *s)
{
  /* Grow the list.  */
  if (slp->nitems >= slp->nitems_max)
    {
      size_t nbytes;

      slp->nitems_max = slp->nitems_max * 2 + 4;
      nbytes = slp->nitems_max * sizeof (slp->item[0]);
      slp->item = (const char **) xrealloc (slp->item, nbytes);
    }

  /* Add the string to the end of the list.  */
  slp->item[slp->nitems++] = s;
}

/* Compare two strings given by reference.  */
static int
cmp_string (const void *pstr1, const void *pstr2)
{
  const char *str1 = *(const char **)pstr1;
  const char *str2 = *(const char **)pstr2;

  return strcmp (str1, str2);
}

/* Sort a list of strings.  */
static inline void
string_list_sort (string_list_ty *slp)
{
  if (slp->nitems > 0)
    qsort (slp->item, slp->nitems, sizeof (slp->item[0]), cmp_string);
}

/* Test whether a string list contains a given string.  */
static inline int
string_list_member (const string_list_ty *slp, const char *s)
{
  size_t j;

  for (j = 0; j < slp->nitems; ++j)
    if (strcmp (slp->item[j], s) == 0)
      return 1;
  return 0;
}

/* Test whether a sorted string list contains a given string.  */
static int
sorted_string_list_member (const string_list_ty *slp, const char *s)
{
  size_t j1, j2;

  j1 = 0;
  j2 = slp->nitems;
  if (j2 > 0)
    {
      /* Binary search.  */
      while (j2 - j1 > 1)
        {
          /* Here we know that if s is in the list, it is at an index j
             with j1 <= j < j2.  */
          size_t j = (j1 + j2) >> 1;
          int result = strcmp (slp->item[j], s);

          if (result > 0)
            j2 = j;
          else if (result == 0)
            return 1;
          else
            j1 = j + 1;
        }
      if (j2 > j1)
        if (strcmp (slp->item[j1], s) == 0)
          return 1;
    }
  return 0;
}

/* Destroy a list of strings.  */
static inline void
string_list_destroy (string_list_ty *slp)
{
  size_t j;

  for (j = 0; j < slp->nitems; ++j)
    free ((char *) slp->item[j]);
  if (slp->item != NULL)
    free (slp->item);
}


/* Set of variables on which to perform substitution.
   Used only if !all_variables.  */
static string_list_ty variables_set;

/* Adds a variable to variables_set.  */
static void
note_variable (const char *var_ptr, size_t var_len)
{
  char *string = XNMALLOC (var_len + 1, char);
  memcpy (string, var_ptr, var_len);
  string[var_len] = '\0';

  string_list_append (&variables_set, string);
}

/* Stores the variables occurring in the string in variables_set.  */
static void
note_variables (const char *string)
{
  string_list_init (&variables_set);
  find_variables (string, &note_variable);
  string_list_sort (&variables_set);
}


static int
do_getc ()
{
  int c = getc (stdin);

  if (c == EOF)
    {
      if (ferror (stdin))
        error (EXIT_FAILURE, errno, _("\
error while reading \"%s\""), _("standard input"));
    }

  return c;
}

static inline void
do_ungetc (int c)
{
  if (c != EOF)
    ungetc (c, stdin);
}

/* Copies stdin to stdout, performing substitutions.  */
static void
subst_from_stdin ()
{
  static char *buffer;
  static size_t bufmax;
  static size_t buflen;
  int c;

  for (;;)
    {
      c = do_getc ();
      if (c == EOF)
        break;
      /* Look for $VARIABLE or ${VARIABLE}.  */
      if (c == '$')
        {
          bool opening_brace = false;
          bool closing_brace = false;

          c = do_getc ();
          if (c == '{')
            {
              opening_brace = true;
              c = do_getc ();
            }
          if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')
            {
              bool valid;

              /* Accumulate the VARIABLE in buffer.  */
              buflen = 0;
              do
                {
                  if (buflen >= bufmax)
                    {
                      bufmax = 2 * bufmax + 10;
                      buffer = xrealloc (buffer, bufmax);
                    }
                  buffer[buflen++] = c;

                  c = do_getc ();
                }
              while ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                     || (c >= '0' && c <= '9') || c == '_');

              if (opening_brace)
                {
                  if (c == '}')
                    {
                      closing_brace = true;
                      valid = true;
                    }
                  else
                    {
                      valid = false;
                      do_ungetc (c);
                    }
                }
              else
                {
                  valid = true;
                  do_ungetc (c);
                }

              if (valid)
                {
                  /* Terminate the variable in the buffer.  */
                  if (buflen >= bufmax)
                    {
                      bufmax = 2 * bufmax + 10;
                      buffer = xrealloc (buffer, bufmax);
                    }
                  buffer[buflen] = '\0';

                  /* Test whether the variable shall be substituted.  */
                  if (!all_variables
                      && !sorted_string_list_member (&variables_set, buffer))
                    valid = false;
                }

              if (valid)
                {
                  /* Substitute the variable's value from the environment.  */
                  const char *env_value = getenv (buffer);

                  if (env_value != NULL)
                    fputs (env_value, stdout);
                }
              else
                {
                  /* Perform no substitution at all.  Since the buffered input
                     contains no other '$' than at the start, we can just
                     output all the buffered contents.  */
                  putchar ('$');
                  if (opening_brace)
                    putchar ('{');
                  fwrite (buffer, buflen, 1, stdout);
                  if (closing_brace)
                    putchar ('}');
                }
            }
          else
            {
              do_ungetc (c);
              putchar ('$');
              if (opening_brace)
                putchar ('{');
            }
        }
      else
        putchar (c);
    }
}
