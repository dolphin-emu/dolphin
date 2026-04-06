/* Copyright (C) 1999-2001, 2003, 2005, 2008, 2012, 2017, 2023 Free Software Foundation, Inc.
   This file is part of the GNU LIBICONV Library.

   The GNU LIBICONV Library is free software; you can redistribute it
   and/or modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either version 2.1
   of the License, or (at your option) any later version.

   The GNU LIBICONV Library is distributed in the hope that it will be
   useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU LIBICONV Library; see the file COPYING.LIB.
   If not, see <https://www.gnu.org/licenses/>.  */

/* Creates the aliases.gperf table. */

#include <stdio.h>
#include <stdlib.h>

/* When we create shell scripts, we need to make sure that on Cygwin they have
   Unix end-of-line characters, regardless of Cygwin choice of text mode vs.
   binary mode. On z/OS, however, binary mode turns off charset tagging for
   output files, which is not what we want. */
#if defined __MVS__
# define BINARY_MODE ""
#else
# define BINARY_MODE "b"
#endif

static void emit_alias (FILE* out1, const char* alias, const char* c_name)
{
  /* Output alias in upper case. */
  const char* s = alias;
  for (; *s; s++) {
    unsigned char c = * (unsigned char *) s;
    if (c >= 0x80)
      exit(1);
    if (c >= 'a' && c <= 'z')
      c -= 'a'-'A';
    putc(c, out1);
  }
  fprintf(out1,", ei_%s\n", c_name);
}

static void emit_encoding (FILE* out1, FILE* out2, const char* const* names, size_t n, const char* c_name)
{
  fprintf(out2,"grep 'sizeof(\"");
  /* Output *names in upper case. */
  {
    const char* s = *names;
    for (; *s; s++) {
      unsigned char c = * (unsigned char *) s;
      if (c >= 0x80)
        exit(1);
      if (c >= 'a' && c <= 'z')
        c -= 'a'-'A';
      putc(c, out2);
    }
  }
  fprintf(out2,"\")' tmp.h | sed -e 's|^.*\\(stringpool_str[0-9]*\\).*$|  (int)(long)\\&((struct stringpool_t *)0)->\\1,|'\n");
  for (; n > 0; names++, n--)
    emit_alias(out1, *names, c_name);
}

int main (int argc, char* argv[])
{
  char* aliases_file_name;
  char* canonical_sh_file_name;
  char* canonical_local_sh_file_name;
  FILE* aliases_file;
  FILE* canonical_sh_file;

  if (argc != 4) {
    fprintf(stderr, "Usage: genaliases aliases.gperf canonical.sh canonical_local.sh\n");
    exit(1);
  }

  aliases_file_name = argv[1];
  canonical_sh_file_name = argv[2];
  canonical_local_sh_file_name = argv[3];

  aliases_file = fopen(aliases_file_name, "w");
  if (aliases_file == NULL) {
    fprintf(stderr, "Could not open '%s' for writing\n", aliases_file_name);
    exit(1);
  }

  fprintf(aliases_file, "struct alias { int name; unsigned int encoding_index; };\n");
  fprintf(aliases_file, "%%struct-type\n");
  fprintf(aliases_file, "%%language=ANSI-C\n");
  fprintf(aliases_file, "%%define hash-function-name aliases_hash\n");
  fprintf(aliases_file, "%%define lookup-function-name aliases_lookup\n");
  fprintf(aliases_file, "%%7bit\n");
  fprintf(aliases_file, "%%readonly-tables\n");
  fprintf(aliases_file, "%%global-table\n");
  fprintf(aliases_file, "%%define word-array-name aliases\n");
  fprintf(aliases_file, "%%pic\n");
  fprintf(aliases_file, "%%%%\n");

#define DEFENCODING(xxx_names,xxx,xxx_ifuncs1,xxx_ifuncs2,xxx_ofuncs1,xxx_ofuncs2) \
  {                                                           \
    static const char* const names[] = BRACIFY xxx_names;     \
    emit_encoding(aliases_file,canonical_sh_file,names,sizeof(names)/sizeof(names[0]),#xxx); \
  }
#define BRACIFY(...) { __VA_ARGS__ }
#define DEFALIAS(xxx_alias,xxx) emit_alias(aliases_file,xxx_alias,#xxx);

  canonical_sh_file = fopen(canonical_sh_file_name, "w" BINARY_MODE);
  if (canonical_sh_file == NULL) {
    fprintf(stderr, "Could not open '%s' for writing\n", canonical_sh_file_name);
    exit(1);
  }
#include "encodings.def"
  if (ferror(canonical_sh_file) || fclose(canonical_sh_file))
    exit(1);

  canonical_sh_file = fopen(canonical_local_sh_file_name, "w" BINARY_MODE);
  if (canonical_sh_file == NULL) {
    fprintf(stderr, "Could not open '%s' for writing\n", canonical_local_sh_file_name);
    exit(1);
  }
#include "encodings_local.def"
  if (ferror(canonical_sh_file) || fclose(canonical_sh_file))
    exit(1);

#undef DEFALIAS
#undef BRACIFY
#undef DEFENCODING

  if (ferror(aliases_file) || fclose(aliases_file))
    exit(1);
  exit(0);
}
