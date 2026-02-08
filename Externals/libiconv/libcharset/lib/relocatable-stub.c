/* Provide relocatable packages.
   Copyright (C) 2018-2023 Free Software Foundation, Inc.
   Written by Bruno Haible <bruno@clisp.org>, 2018.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <https://www.gnu.org/licenses/>.  */

#include <config.h>

#include "libcharset.h"

extern LIBCHARSET_SHLIB_EXPORTED void
libcharset_set_relocation_prefix (const char *orig_prefix, const char *curr_prefix);

/* This is a stub for binary backward-compatibility.  */
void
libcharset_set_relocation_prefix (const char *orig_prefix, const char *curr_prefix)
{
}
