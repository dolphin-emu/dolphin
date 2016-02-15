/* Localization of proper names.
   Copyright (C) 2006, 2008-2014 Free Software Foundation, Inc.
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

/* INTRODUCTION

   What do

      Torbjörn Granlund    (coreutils)
      François Pinard      (coreutils)
      Danilo Šegan         (gettext)

   have in common?

   A non-ASCII name. This causes trouble in the --version output. The simple
   "solution", unfortunately mutilates the name.

     $ du --version| grep Granlund
     Écrit par Torbjorn Granlund, David MacKenzie, Paul Eggert et Jim Meyering.

     $ ptx --version| grep Pinard
     Écrit par F. Pinard.

   What is desirable, is to print the full name if the output character set
   allows it, and the ASCIIfied name only as a fallback.

     $ recode-sr-latin --version
     ...
     Written by Danilo Šegan and Bruno Haible.

     $ LC_ALL=C recode-sr-latin --version
     ...
     Written by Danilo Segan and Bruno Haible.

   The 'propername' module does exactly this. Plus, for languages that use
   a different writing system than the Latin alphabet, it allows a translator
   to write the name using that different writing system. In that case the
   output will look like this:
      <translated name> (<original name in English>)

   To use the 'propername' module is done in three simple steps:

     1) Add it to the list of gnulib modules to import,

     2) Change the arguments of version_etc, from

          from "Paul Eggert"
          to   proper_name ("Paul Eggert")

          from "Torbjorn Granlund"
          to   proper_name_utf8 ("Torbjorn Granlund", "Torbj\303\266rn Granlund")

          from "F. Pinard"
          to   proper_name_utf8 ("Franc,ois Pinard", "Fran\303\247ois Pinard")

        (Optionally, here you can also add / * TRANSLATORS: ... * / comments
        explaining how the name is written or pronounced.)

     3) If you are using GNU gettext version 0.16.1 or older, in po/Makevars,
        in the definition of the XGETTEXT_OPTIONS variable, add:

           --keyword='proper_name:1,"This is a proper name. See the gettext manual, section Names."'
           --keyword='proper_name_utf8:1,"This is a proper name. See the gettext manual, section Names."'

        This specifies automatic comments for the translator. (Requires
        xgettext >= 0.15. The double-quotes inside the quoted string are on
        purpose: they are part of the --keyword argument syntax.)
 */

#ifndef _PROPERNAME_H
#define _PROPERNAME_H


#ifdef __cplusplus
extern "C" {
#endif

/* Return the localization of NAME.  NAME is written in ASCII.  */
extern const char * proper_name (const char *name) /* NOT attribute const */;

/* Return the localization of a name whose original writing is not ASCII.
   NAME_UTF8 is the real name, written in UTF-8 with octal or hexadecimal
   escape sequences.  NAME_ASCII is a fallback written only with ASCII
   characters.  */
extern const char * proper_name_utf8 (const char *name_ascii,
                                      const char *name_utf8);

#ifdef __cplusplus
}
#endif


#endif /* _PROPERNAME_H */
