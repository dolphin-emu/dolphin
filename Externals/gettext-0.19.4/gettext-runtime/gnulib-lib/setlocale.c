/* Set the current locale.
   Copyright (C) 2009, 2011-2014 Free Software Foundation, Inc.

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

/* Written by Bruno Haible <bruno@clisp.org>, 2009.  */

#include <config.h>

/* Override setlocale() so that when the default locale is requested
   (locale = ""), the environment variables LC_ALL, LC_*, and LANG are
   considered.
   Also include all the functionality from libintl's setlocale() override.  */

/* Please keep this file in sync with
   gettext/gettext-runtime/intl/setlocale.c !  */

/* Specification.  */
#include <locale.h>

#include <stdlib.h>
#include <string.h>

#include "localename.h"

#if 1

# undef setlocale

/* Return string representation of locale category CATEGORY.  */
static const char *
category_to_name (int category)
{
  const char *retval;

  switch (category)
  {
  case LC_COLLATE:
    retval = "LC_COLLATE";
    break;
  case LC_CTYPE:
    retval = "LC_CTYPE";
    break;
  case LC_MONETARY:
    retval = "LC_MONETARY";
    break;
  case LC_NUMERIC:
    retval = "LC_NUMERIC";
    break;
  case LC_TIME:
    retval = "LC_TIME";
    break;
  case LC_MESSAGES:
    retval = "LC_MESSAGES";
    break;
  default:
    /* If you have a better idea for a default value let me know.  */
    retval = "LC_XXX";
  }

  return retval;
}

# if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__

/* The native Windows setlocale() function expects locale names of the form
   "German" or "German_Germany" or "DEU", but not "de" or "de_DE".  We need
   to convert the names from the form with ISO 639 language code and ISO 3166
   country code to the form with English names or with three-letter identifier.
   The three-letter identifiers known by a Windows XP SP2 or SP3 are:
     AFK  Afrikaans_South Africa.1252
     ARA  Arabic_Saudi Arabia.1256
     ARB  Arabic_Lebanon.1256
     ARE  Arabic_Egypt.1256
     ARG  Arabic_Algeria.1256
     ARH  Arabic_Bahrain.1256
     ARI  Arabic_Iraq.1256
     ARJ  Arabic_Jordan.1256
     ARK  Arabic_Kuwait.1256
     ARL  Arabic_Libya.1256
     ARM  Arabic_Morocco.1256
     ARO  Arabic_Oman.1256
     ARQ  Arabic_Qatar.1256
     ARS  Arabic_Syria.1256
     ART  Arabic_Tunisia.1256
     ARU  Arabic_U.A.E..1256
     ARY  Arabic_Yemen.1256
     AZE  Azeri (Latin)_Azerbaijan.1254
     BEL  Belarusian_Belarus.1251
     BGR  Bulgarian_Bulgaria.1251
     BSB  Bosnian_Bosnia and Herzegovina.1250
     BSC  Bosnian (Cyrillic)_Bosnia and Herzegovina.1250  (wrong encoding!)
     CAT  Catalan_Spain.1252
     CHH  Chinese_Hong Kong S.A.R..950
     CHI  Chinese_Singapore.936
     CHS  Chinese_People's Republic of China.936
     CHT  Chinese_Taiwan.950
     CSY  Czech_Czech Republic.1250
     CYM  Welsh_United Kingdom.1252
     DAN  Danish_Denmark.1252
     DEA  German_Austria.1252
     DEC  German_Liechtenstein.1252
     DEL  German_Luxembourg.1252
     DES  German_Switzerland.1252
     DEU  German_Germany.1252
     ELL  Greek_Greece.1253
     ENA  English_Australia.1252
     ENB  English_Caribbean.1252
     ENC  English_Canada.1252
     ENG  English_United Kingdom.1252
     ENI  English_Ireland.1252
     ENJ  English_Jamaica.1252
     ENL  English_Belize.1252
     ENP  English_Republic of the Philippines.1252
     ENS  English_South Africa.1252
     ENT  English_Trinidad and Tobago.1252
     ENU  English_United States.1252
     ENW  English_Zimbabwe.1252
     ENZ  English_New Zealand.1252
     ESA  Spanish_Panama.1252
     ESB  Spanish_Bolivia.1252
     ESC  Spanish_Costa Rica.1252
     ESD  Spanish_Dominican Republic.1252
     ESE  Spanish_El Salvador.1252
     ESF  Spanish_Ecuador.1252
     ESG  Spanish_Guatemala.1252
     ESH  Spanish_Honduras.1252
     ESI  Spanish_Nicaragua.1252
     ESL  Spanish_Chile.1252
     ESM  Spanish_Mexico.1252
     ESN  Spanish_Spain.1252
     ESO  Spanish_Colombia.1252
     ESP  Spanish_Spain.1252
     ESR  Spanish_Peru.1252
     ESS  Spanish_Argentina.1252
     ESU  Spanish_Puerto Rico.1252
     ESV  Spanish_Venezuela.1252
     ESY  Spanish_Uruguay.1252
     ESZ  Spanish_Paraguay.1252
     ETI  Estonian_Estonia.1257
     EUQ  Basque_Spain.1252
     FAR  Farsi_Iran.1256
     FIN  Finnish_Finland.1252
     FOS  Faroese_Faroe Islands.1252
     FPO  Filipino_Philippines.1252
     FRA  French_France.1252
     FRB  French_Belgium.1252
     FRC  French_Canada.1252
     FRL  French_Luxembourg.1252
     FRM  French_Principality of Monaco.1252
     FRS  French_Switzerland.1252
     FYN  Frisian_Netherlands.1252
     GLC  Galician_Spain.1252
     HEB  Hebrew_Israel.1255
     HRB  Croatian_Bosnia and Herzegovina.1250
     HRV  Croatian_Croatia.1250
     HUN  Hungarian_Hungary.1250
     IND  Indonesian_Indonesia.1252
     IRE  Irish_Ireland.1252
     ISL  Icelandic_Iceland.1252
     ITA  Italian_Italy.1252
     ITS  Italian_Switzerland.1252
     IUK  Inuktitut (Latin)_Canada.1252
     JPN  Japanese_Japan.932
     KKZ  Kazakh_Kazakhstan.1251
     KOR  Korean_Korea.949
     KYR  Kyrgyz_Kyrgyzstan.1251
     LBX  Luxembourgish_Luxembourg.1252
     LTH  Lithuanian_Lithuania.1257
     LVI  Latvian_Latvia.1257
     MKI  FYRO Macedonian_Former Yugoslav Republic of Macedonia.1251
     MON  Mongolian_Mongolia.1251
     MPD  Mapudungun_Chile.1252
     MSB  Malay_Brunei Darussalam.1252
     MSL  Malay_Malaysia.1252
     MWK  Mohawk_Canada.1252
     NLB  Dutch_Belgium.1252
     NLD  Dutch_Netherlands.1252
     NON  Norwegian-Nynorsk_Norway.1252
     NOR  Norwegian (Bokmål)_Norway.1252
     NSO  Northern Sotho_South Africa.1252
     PLK  Polish_Poland.1250
     PTB  Portuguese_Brazil.1252
     PTG  Portuguese_Portugal.1252
     QUB  Quechua_Bolivia.1252
     QUE  Quechua_Ecuador.1252
     QUP  Quechua_Peru.1252
     RMC  Romansh_Switzerland.1252
     ROM  Romanian_Romania.1250
     RUS  Russian_Russia.1251
     SKY  Slovak_Slovakia.1250
     SLV  Slovenian_Slovenia.1250
     SMA  Sami (Southern)_Norway.1252
     SMB  Sami (Southern)_Sweden.1252
     SME  Sami (Northern)_Norway.1252
     SMF  Sami (Northern)_Sweden.1252
     SMG  Sami (Northern)_Finland.1252
     SMJ  Sami (Lule)_Norway.1252
     SMK  Sami (Lule)_Sweden.1252
     SMN  Sami (Inari)_Finland.1252
     SMS  Sami (Skolt)_Finland.1252
     SQI  Albanian_Albania.1250
     SRB  Serbian (Cyrillic)_Serbia and Montenegro.1251
     SRL  Serbian (Latin)_Serbia and Montenegro.1250
     SRN  Serbian (Cyrillic)_Bosnia and Herzegovina.1251
     SRS  Serbian (Latin)_Bosnia and Herzegovina.1250
     SVE  Swedish_Sweden.1252
     SVF  Swedish_Finland.1252
     SWK  Swahili_Kenya.1252
     THA  Thai_Thailand.874
     TRK  Turkish_Turkey.1254
     TSN  Tswana_South Africa.1252
     TTT  Tatar_Russia.1251
     UKR  Ukrainian_Ukraine.1251
     URD  Urdu_Islamic Republic of Pakistan.1256
     USA  English_United States.1252
     UZB  Uzbek (Latin)_Uzbekistan.1254
     VIT  Vietnamese_Viet Nam.1258
     XHO  Xhosa_South Africa.1252
     ZHH  Chinese_Hong Kong S.A.R..950
     ZHI  Chinese_Singapore.936
     ZHM  Chinese_Macau S.A.R..950
     ZUL  Zulu_South Africa.1252
 */

/* Table from ISO 639 language code, optionally with country or script suffix,
   to English name.
   Keep in sync with the gl_locale_name_from_win32_LANGID function in
   localename.c!  */
struct table_entry
{
  const char *code;
  const char *english;
};
static const struct table_entry language_table[] =
  {
    { "af", "Afrikaans" },
    { "am", "Amharic" },
    { "ar", "Arabic" },
    { "arn", "Mapudungun" },
    { "as", "Assamese" },
    { "az@cyrillic", "Azeri (Cyrillic)" },
    { "az@latin", "Azeri (Latin)" },
    { "ba", "Bashkir" },
    { "be", "Belarusian" },
    { "ber", "Tamazight" },
    { "ber@arabic", "Tamazight (Arabic)" },
    { "ber@latin", "Tamazight (Latin)" },
    { "bg", "Bulgarian" },
    { "bin", "Edo" },
    { "bn", "Bengali" },
    { "bn_BD", "Bengali (Bangladesh)" },
    { "bn_IN", "Bengali (India)" },
    { "bnt", "Sutu" },
    { "bo", "Tibetan" },
    { "br", "Breton" },
    { "bs", "BSB" }, /* "Bosnian (Latin)" */
    { "bs@cyrillic", "BSC" }, /* Bosnian (Cyrillic) */
    { "ca", "Catalan" },
    { "chr", "Cherokee" },
    { "co", "Corsican" },
    { "cpe", "Hawaiian" },
    { "cs", "Czech" },
    { "cy", "Welsh" },
    { "da", "Danish" },
    { "de", "German" },
    { "dsb", "Lower Sorbian" },
    { "dv", "Divehi" },
    { "el", "Greek" },
    { "en", "English" },
    { "es", "Spanish" },
    { "et", "Estonian" },
    { "eu", "Basque" },
    { "fa", "Farsi" },
    { "ff", "Fulfulde" },
    { "fi", "Finnish" },
    { "fo", "Faroese" }, /* "Faeroese" does not work */
    { "fr", "French" },
    { "fy", "Frisian" },
    { "ga", "IRE" }, /* Gaelic (Ireland) */
    { "gd", "Gaelic (Scotland)" },
    { "gd", "Scottish Gaelic" },
    { "gl", "Galician" },
    { "gn", "Guarani" },
    { "gsw", "Alsatian" },
    { "gu", "Gujarati" },
    { "ha", "Hausa" },
    { "he", "Hebrew" },
    { "hi", "Hindi" },
    { "hr", "Croatian" },
    { "hsb", "Upper Sorbian" },
    { "hu", "Hungarian" },
    { "hy", "Armenian" },
    { "id", "Indonesian" },
    { "ig", "Igbo" },
    { "ii", "Yi" },
    { "is", "Icelandic" },
    { "it", "Italian" },
    { "iu", "IUK" }, /* Inuktitut */
    { "ja", "Japanese" },
    { "ka", "Georgian" },
    { "kk", "Kazakh" },
    { "kl", "Greenlandic" },
    { "km", "Cambodian" },
    { "km", "Khmer" },
    { "kn", "Kannada" },
    { "ko", "Korean" },
    { "kok", "Konkani" },
    { "kr", "Kanuri" },
    { "ks", "Kashmiri" },
    { "ks_IN", "Kashmiri_India" },
    { "ks_PK", "Kashmiri (Arabic)_Pakistan" },
    { "ky", "Kyrgyz" },
    { "la", "Latin" },
    { "lb", "Luxembourgish" },
    { "lo", "Lao" },
    { "lt", "Lithuanian" },
    { "lv", "Latvian" },
    { "mi", "Maori" },
    { "mk", "FYRO Macedonian" },
    { "mk", "Macedonian" },
    { "ml", "Malayalam" },
    { "mn", "Mongolian" },
    { "mni", "Manipuri" },
    { "moh", "Mohawk" },
    { "mr", "Marathi" },
    { "ms", "Malay" },
    { "mt", "Maltese" },
    { "my", "Burmese" },
    { "nb", "NOR" }, /* Norwegian Bokmål */
    { "ne", "Nepali" },
    { "nic", "Ibibio" },
    { "nl", "Dutch" },
    { "nn", "NON" }, /* Norwegian Nynorsk */
    { "no", "Norwegian" },
    { "nso", "Northern Sotho" },
    { "nso", "Sepedi" },
    { "oc", "Occitan" },
    { "om", "Oromo" },
    { "or", "Oriya" },
    { "pa", "Punjabi" },
    { "pap", "Papiamentu" },
    { "pl", "Polish" },
    { "prs", "Dari" },
    { "ps", "Pashto" },
    { "pt", "Portuguese" },
    { "qu", "Quechua" },
    { "qut", "K'iche'" },
    { "rm", "Romansh" },
    { "ro", "Romanian" },
    { "ru", "Russian" },
    { "rw", "Kinyarwanda" },
    { "sa", "Sanskrit" },
    { "sah", "Yakut" },
    { "sd", "Sindhi" },
    { "se", "Sami (Northern)" },
    { "se", "Northern Sami" },
    { "si", "Sinhalese" },
    { "sk", "Slovak" },
    { "sl", "Slovenian" },
    { "sma", "Sami (Southern)" },
    { "sma", "Southern Sami" },
    { "smj", "Sami (Lule)" },
    { "smj", "Lule Sami" },
    { "smn", "Sami (Inari)" },
    { "smn", "Inari Sami" },
    { "sms", "Sami (Skolt)" },
    { "sms", "Skolt Sami" },
    { "so", "Somali" },
    { "sq", "Albanian" },
    { "sr", "Serbian (Latin)" },
    { "sr@cyrillic", "SRB" }, /* Serbian (Cyrillic) */
    { "sw", "Swahili" },
    { "syr", "Syriac" },
    { "ta", "Tamil" },
    { "te", "Telugu" },
    { "tg", "Tajik" },
    { "th", "Thai" },
    { "ti", "Tigrinya" },
    { "tk", "Turkmen" },
    { "tl", "Filipino" },
    { "tn", "Tswana" },
    { "tr", "Turkish" },
    { "ts", "Tsonga" },
    { "tt", "Tatar" },
    { "ug", "Uighur" },
    { "uk", "Ukrainian" },
    { "ur", "Urdu" },
    { "uz", "Uzbek" },
    { "uz", "Uzbek (Latin)" },
    { "uz@cyrillic", "Uzbek (Cyrillic)" },
    { "ve", "Venda" },
    { "vi", "Vietnamese" },
    { "wen", "Sorbian" },
    { "wo", "Wolof" },
    { "xh", "Xhosa" },
    { "yi", "Yiddish" },
    { "yo", "Yoruba" },
    { "zh", "Chinese" },
    { "zu", "Zulu" }
  };

/* Table from ISO 3166 country code to English name.
   Keep in sync with the gl_locale_name_from_win32_LANGID function in
   localename.c!  */
static const struct table_entry country_table[] =
  {
    { "AE", "U.A.E." },
    { "AF", "Afghanistan" },
    { "AL", "Albania" },
    { "AM", "Armenia" },
    { "AN", "Netherlands Antilles" },
    { "AR", "Argentina" },
    { "AT", "Austria" },
    { "AU", "Australia" },
    { "AZ", "Azerbaijan" },
    { "BA", "Bosnia and Herzegovina" },
    { "BD", "Bangladesh" },
    { "BE", "Belgium" },
    { "BG", "Bulgaria" },
    { "BH", "Bahrain" },
    { "BN", "Brunei Darussalam" },
    { "BO", "Bolivia" },
    { "BR", "Brazil" },
    { "BT", "Bhutan" },
    { "BY", "Belarus" },
    { "BZ", "Belize" },
    { "CA", "Canada" },
    { "CG", "Congo" },
    { "CH", "Switzerland" },
    { "CI", "Cote d'Ivoire" },
    { "CL", "Chile" },
    { "CM", "Cameroon" },
    { "CN", "People's Republic of China" },
    { "CO", "Colombia" },
    { "CR", "Costa Rica" },
    { "CS", "Serbia and Montenegro" },
    { "CZ", "Czech Republic" },
    { "DE", "Germany" },
    { "DK", "Denmark" },
    { "DO", "Dominican Republic" },
    { "DZ", "Algeria" },
    { "EC", "Ecuador" },
    { "EE", "Estonia" },
    { "EG", "Egypt" },
    { "ER", "Eritrea" },
    { "ES", "Spain" },
    { "ET", "Ethiopia" },
    { "FI", "Finland" },
    { "FO", "Faroe Islands" },
    { "FR", "France" },
    { "GB", "United Kingdom" },
    { "GD", "Caribbean" },
    { "GE", "Georgia" },
    { "GL", "Greenland" },
    { "GR", "Greece" },
    { "GT", "Guatemala" },
    { "HK", "Hong Kong" },
    { "HK", "Hong Kong S.A.R." },
    { "HN", "Honduras" },
    { "HR", "Croatia" },
    { "HT", "Haiti" },
    { "HU", "Hungary" },
    { "ID", "Indonesia" },
    { "IE", "Ireland" },
    { "IL", "Israel" },
    { "IN", "India" },
    { "IQ", "Iraq" },
    { "IR", "Iran" },
    { "IS", "Iceland" },
    { "IT", "Italy" },
    { "JM", "Jamaica" },
    { "JO", "Jordan" },
    { "JP", "Japan" },
    { "KE", "Kenya" },
    { "KG", "Kyrgyzstan" },
    { "KH", "Cambodia" },
    { "KR", "South Korea" },
    { "KW", "Kuwait" },
    { "KZ", "Kazakhstan" },
    { "LA", "Laos" },
    { "LB", "Lebanon" },
    { "LI", "Liechtenstein" },
    { "LK", "Sri Lanka" },
    { "LT", "Lithuania" },
    { "LU", "Luxembourg" },
    { "LV", "Latvia" },
    { "LY", "Libya" },
    { "MA", "Morocco" },
    { "MC", "Principality of Monaco" },
    { "MD", "Moldava" },
    { "MD", "Moldova" },
    { "ME", "Montenegro" },
    { "MK", "Former Yugoslav Republic of Macedonia" },
    { "ML", "Mali" },
    { "MM", "Myanmar" },
    { "MN", "Mongolia" },
    { "MO", "Macau S.A.R." },
    { "MT", "Malta" },
    { "MV", "Maldives" },
    { "MX", "Mexico" },
    { "MY", "Malaysia" },
    { "NG", "Nigeria" },
    { "NI", "Nicaragua" },
    { "NL", "Netherlands" },
    { "NO", "Norway" },
    { "NP", "Nepal" },
    { "NZ", "New Zealand" },
    { "OM", "Oman" },
    { "PA", "Panama" },
    { "PE", "Peru" },
    { "PH", "Philippines" },
    { "PK", "Islamic Republic of Pakistan" },
    { "PL", "Poland" },
    { "PR", "Puerto Rico" },
    { "PT", "Portugal" },
    { "PY", "Paraguay" },
    { "QA", "Qatar" },
    { "RE", "Reunion" },
    { "RO", "Romania" },
    { "RS", "Serbia" },
    { "RU", "Russia" },
    { "RW", "Rwanda" },
    { "SA", "Saudi Arabia" },
    { "SE", "Sweden" },
    { "SG", "Singapore" },
    { "SI", "Slovenia" },
    { "SK", "Slovak" },
    { "SN", "Senegal" },
    { "SO", "Somalia" },
    { "SR", "Suriname" },
    { "SV", "El Salvador" },
    { "SY", "Syria" },
    { "TH", "Thailand" },
    { "TJ", "Tajikistan" },
    { "TM", "Turkmenistan" },
    { "TN", "Tunisia" },
    { "TR", "Turkey" },
    { "TT", "Trinidad and Tobago" },
    { "TW", "Taiwan" },
    { "TZ", "Tanzania" },
    { "UA", "Ukraine" },
    { "US", "United States" },
    { "UY", "Uruguay" },
    { "VA", "Vatican" },
    { "VE", "Venezuela" },
    { "VN", "Viet Nam" },
    { "YE", "Yemen" },
    { "ZA", "South Africa" },
    { "ZW", "Zimbabwe" }
  };

/* Given a string STRING, find the set of indices i such that TABLE[i].code is
   the given STRING.  It is a range [lo,hi-1].  */
typedef struct { size_t lo; size_t hi; } range_t;
static void
search (const struct table_entry *table, size_t table_size, const char *string,
        range_t *result)
{
  /* The table is sorted.  Perform a binary search.  */
  size_t hi = table_size;
  size_t lo = 0;
  while (lo < hi)
    {
      /* Invariant:
         for i < lo, strcmp (table[i].code, string) < 0,
         for i >= hi, strcmp (table[i].code, string) > 0.  */
      size_t mid = (hi + lo) >> 1; /* >= lo, < hi */
      int cmp = strcmp (table[mid].code, string);
      if (cmp < 0)
        lo = mid + 1;
      else if (cmp > 0)
        hi = mid;
      else
        {
          /* Found an i with
               strcmp (language_table[i].code, string) == 0.
             Find the entire interval of such i.  */
          {
            size_t i;

            for (i = mid; i > lo; )
              {
                i--;
                if (strcmp (table[i].code, string) < 0)
                  {
                    lo = i + 1;
                    break;
                  }
              }
          }
          {
            size_t i;

            for (i = mid; i < hi; i++)
              {
                if (strcmp (table[i].code, string) > 0)
                  {
                    hi = i;
                    break;
                  }
              }
          }
          /* The set of i with
               strcmp (language_table[i].code, string) == 0
             is the interval [lo, hi-1].  */
          break;
        }
    }
  result->lo = lo;
  result->hi = hi;
}

/* Like setlocale, but accept also locale names in the form ll or ll_CC,
   where ll is an ISO 639 language code and CC is an ISO 3166 country code.  */
static char *
setlocale_unixlike (int category, const char *locale)
{
  char *result;
  char llCC_buf[64];
  char ll_buf[64];
  char CC_buf[64];

  /* First, try setlocale with the original argument unchanged.  */
  result = setlocale (category, locale);
  if (result != NULL)
    return result;

  /* Otherwise, assume the argument is in the form
       language[_territory][.codeset][@modifier]
     and try to map it using the tables.  */
  if (strlen (locale) < sizeof (llCC_buf))
    {
      /* Second try: Remove the codeset part.  */
      {
        const char *p = locale;
        char *q = llCC_buf;

        /* Copy the part before the dot.  */
        for (; *p != '\0' && *p != '.'; p++, q++)
          *q = *p;
        if (*p == '.')
          /* Skip the part up to the '@', if any.  */
          for (; *p != '\0' && *p != '@'; p++)
            ;
        /* Copy the part starting with '@', if any.  */
        for (; *p != '\0'; p++, q++)
          *q = *p;
        *q = '\0';
      }
      /* llCC_buf now contains
           language[_territory][@modifier]
       */
      if (strcmp (llCC_buf, locale) != 0)
        {
          result = setlocale (category, llCC_buf);
          if (result != NULL)
            return result;
        }
      /* Look it up in language_table.  */
      {
        range_t range;
        size_t i;

        search (language_table,
                sizeof (language_table) / sizeof (language_table[0]),
                llCC_buf,
                &range);

        for (i = range.lo; i < range.hi; i++)
          {
            /* Try the replacement in language_table[i].  */
            result = setlocale (category, language_table[i].english);
            if (result != NULL)
              return result;
          }
      }
      /* Split language[_territory][@modifier]
         into  ll_buf = language[@modifier]
         and   CC_buf = territory
       */
      {
        const char *underscore = strchr (llCC_buf, '_');
        if (underscore != NULL)
          {
            const char *territory_start = underscore + 1;
            const char *territory_end = strchr (territory_start, '@');
            if (territory_end == NULL)
              territory_end = territory_start + strlen (territory_start);

            memcpy (ll_buf, llCC_buf, underscore - llCC_buf);
            strcpy (ll_buf + (underscore - llCC_buf), territory_end);

            memcpy (CC_buf, territory_start, territory_end - territory_start);
            CC_buf[territory_end - territory_start] = '\0';

            {
              /* Look up ll_buf in language_table
                 and CC_buf in country_table.  */
              range_t language_range;

              search (language_table,
                      sizeof (language_table) / sizeof (language_table[0]),
                      ll_buf,
                      &language_range);
              if (language_range.lo < language_range.hi)
                {
                  range_t country_range;

                  search (country_table,
                          sizeof (country_table) / sizeof (country_table[0]),
                          CC_buf,
                          &country_range);
                  if (country_range.lo < country_range.hi)
                    {
                      size_t i;
                      size_t j;

                      for (i = language_range.lo; i < language_range.hi; i++)
                        for (j = country_range.lo; j < country_range.hi; j++)
                          {
                            /* Concatenate the replacements.  */
                            const char *part1 = language_table[i].english;
                            size_t part1_len = strlen (part1);
                            const char *part2 = country_table[j].english;
                            size_t part2_len = strlen (part2) + 1;
                            char buf[64+64];

                            if (!(part1_len + 1 + part2_len <= sizeof (buf)))
                              abort ();
                            memcpy (buf, part1, part1_len);
                            buf[part1_len] = '_';
                            memcpy (buf + part1_len + 1, part2, part2_len);

                            /* Try the concatenated replacements.  */
                            result = setlocale (category, buf);
                            if (result != NULL)
                              return result;
                          }
                    }

                  /* Try omitting the country entirely.  This may set a locale
                     corresponding to the wrong country, but is better than
                     failing entirely.  */
                  {
                    size_t i;

                    for (i = language_range.lo; i < language_range.hi; i++)
                      {
                        /* Try only the language replacement.  */
                        result =
                          setlocale (category, language_table[i].english);
                        if (result != NULL)
                          return result;
                      }
                  }
                }
            }
          }
      }
    }

  /* Failed.  */
  return NULL;
}

# else
#  define setlocale_unixlike setlocale
# endif

# if LC_MESSAGES == 1729

/* The system does not store an LC_MESSAGES locale category.  Do it here.  */
static char lc_messages_name[64] = "C";

/* Like setlocale, but support also LC_MESSAGES.  */
static char *
setlocale_single (int category, const char *locale)
{
  if (category == LC_MESSAGES)
    {
      if (locale != NULL)
        {
          lc_messages_name[sizeof (lc_messages_name) - 1] = '\0';
          strncpy (lc_messages_name, locale, sizeof (lc_messages_name) - 1);
        }
      return lc_messages_name;
    }
  else
    return setlocale_unixlike (category, locale);
}

# else
#  define setlocale_single setlocale_unixlike
# endif

char *
rpl_setlocale (int category, const char *locale)
{
  if (locale != NULL && locale[0] == '\0')
    {
      /* A request to the set the current locale to the default locale.  */
      if (category == LC_ALL)
        {
          /* Set LC_CTYPE first.  Then the other categories.  */
          static int const categories[] =
            {
              LC_NUMERIC,
              LC_TIME,
              LC_COLLATE,
              LC_MONETARY,
              LC_MESSAGES
            };
          char *saved_locale;
          const char *base_name;
          unsigned int i;

          /* Back up the old locale, in case one of the steps fails.  */
          saved_locale = setlocale (LC_ALL, NULL);
          if (saved_locale == NULL)
            return NULL;
          saved_locale = strdup (saved_locale);
          if (saved_locale == NULL)
            return NULL;

          /* Set LC_CTYPE category.  Set all other categories (except possibly
             LC_MESSAGES) to the same value in the same call; this is likely to
             save calls.  */
          base_name =
            gl_locale_name_environ (LC_CTYPE, category_to_name (LC_CTYPE));
          if (base_name == NULL)
            base_name = gl_locale_name_default ();

          if (setlocale_unixlike (LC_ALL, base_name) == NULL)
            goto fail;
# if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
          /* On native Windows, setlocale(LC_ALL,...) may succeed but set the
             LC_CTYPE category to an invalid value ("C") when it does not
             support the specified encoding.  Report a failure instead.  */
          if (strchr (base_name, '.') != NULL
              && strcmp (setlocale (LC_CTYPE, NULL), "C") == 0)
            goto fail;
# endif

          for (i = 0; i < sizeof (categories) / sizeof (categories[0]); i++)
            {
              int cat = categories[i];
              const char *name;

              name = gl_locale_name_environ (cat, category_to_name (cat));
              if (name == NULL)
                name = gl_locale_name_default ();

              /* If name is the same as base_name, it has already been set
                 through the setlocale call before the loop.  */
              if (strcmp (name, base_name) != 0
# if LC_MESSAGES == 1729
                  || cat == LC_MESSAGES
# endif
                 )
                if (setlocale_single (cat, name) == NULL)
                  goto fail;
            }

          /* All steps were successful.  */
          free (saved_locale);
          return setlocale (LC_ALL, NULL);

        fail:
          if (saved_locale[0] != '\0') /* don't risk an endless recursion */
            setlocale (LC_ALL, saved_locale);
          free (saved_locale);
          return NULL;
        }
      else
        {
          const char *name =
            gl_locale_name_environ (category, category_to_name (category));
          if (name == NULL)
            name = gl_locale_name_default ();

          return setlocale_single (category, name);
        }
    }
  else
    {
# if (defined _WIN32 || defined __WIN32__) && ! defined __CYGWIN__
      if (category == LC_ALL && locale != NULL && strchr (locale, '.') != NULL)
        {
          char *saved_locale;

          /* Back up the old locale.  */
          saved_locale = setlocale (LC_ALL, NULL);
          if (saved_locale == NULL)
            return NULL;
          saved_locale = strdup (saved_locale);
          if (saved_locale == NULL)
            return NULL;

          if (setlocale_unixlike (LC_ALL, locale) == NULL)
            {
              free (saved_locale);
              return NULL;
            }

          /* On native Windows, setlocale(LC_ALL,...) may succeed but set the
             LC_CTYPE category to an invalid value ("C") when it does not
             support the specified encoding.  Report a failure instead.  */
          if (strcmp (setlocale (LC_CTYPE, NULL), "C") == 0)
            {
              if (saved_locale[0] != '\0') /* don't risk an endless recursion */
                setlocale (LC_ALL, saved_locale);
              free (saved_locale);
              return NULL;
            }

          /* It was really successful.  */
          free (saved_locale);
          return setlocale (LC_ALL, NULL);
        }
      else
# endif
        return setlocale_single (category, locale);
    }
}

#endif
