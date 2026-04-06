/* Copyright (C) 1999-2003, 2005, 2008, 2012, 2022 Free Software Foundation, Inc.
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

/* Creates the aliases2.h table. */

#include <stdio.h>
#include <stdlib.h>

static unsigned int counter = 0;

static void emit_alias (FILE* out1, const char* tag, const char* alias, const char* c_name)
{
  fprintf(out1,"  S(%s_%u, \"",tag,counter);
  /* Output alias in upper case. */
  {
    const char* s = alias;
    for (; *s; s++) {
      unsigned char c = * (unsigned char *) s;
      if (c >= 0x80)
        exit(1);
      if (c >= 'a' && c <= 'z')
        c -= 'a'-'A';
      putc(c, out1);
    }
  }
  fprintf(out1,"\", ei_%s )\n", c_name);
  counter++;
}

static void emit_encoding (FILE* out1, FILE* out2, const char* tag, const char* const* names, size_t n, const char* c_name)
{
  fprintf(out2,"  (int)(long)&((struct stringpool2_t *)0)->stringpool_%s_%u,\n",tag,counter);
  for (; n > 0; names++, n--)
    emit_alias(out1, tag, *names, c_name);
}

int main (int argc, char* argv[])
{
  const char* tag;
  char* aliases_file_name;
  char* canonical_file_name;
  FILE* aliases_file;
  FILE* canonical_file;

  if (argc != 4) {
    fprintf(stderr, "Usage: genaliases2 tag aliases.h canonical.h\n");
    exit(1);
  }

  tag = argv[1];
  aliases_file_name = argv[2];
  canonical_file_name = argv[3];

  aliases_file = fopen(aliases_file_name, "w");
  if (aliases_file == NULL) {
    fprintf(stderr, "Could not open '%s' for writing\n", aliases_file_name);
    exit(1);
  }

  canonical_file = fopen(canonical_file_name, "w");
  if (canonical_file == NULL) {
    fprintf(stderr, "Could not open '%s' for writing\n", canonical_file_name);
    exit(1);
  }

#define DEFENCODING(xxx_names,xxx,xxx_ifuncs1,xxx_ifuncs2,xxx_ofuncs1,xxx_ofuncs2) \
  {                                                           \
    static const char* const names[] = BRACIFY xxx_names;     \
    emit_encoding(aliases_file,canonical_file,tag,names,sizeof(names)/sizeof(names[0]),#xxx); \
  }
#define BRACIFY(...) { __VA_ARGS__ }
#define DEFALIAS(xxx_alias,xxx) emit_alias(aliases_file,tag,xxx_alias,#xxx);
#ifdef USE_AIX
#include "encodings_aix.def"
#endif
#ifdef USE_OSF1
#include "encodings_osf1.def"
#endif
#ifdef USE_DOS
#include "encodings_dos.def"
#endif
#ifdef USE_ZOS
#include "encodings_zos.def"
#endif
#ifdef USE_EXTRA
#include "encodings_extra.def"
#endif
#undef DEFALIAS
#undef BRACIFY
#undef DEFENCODING
  if (ferror(aliases_file) || fclose(aliases_file))
    exit(1);
  if (ferror(canonical_file) || fclose(canonical_file))
    exit(1);
  exit(0);
}
