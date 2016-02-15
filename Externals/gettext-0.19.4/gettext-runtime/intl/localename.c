/* Determine name of the currently selected locale.
   Copyright (C) 1995-2014 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* Written by Ulrich Drepper <drepper@gnu.org>, 1995.  */
/* Native Windows code written by Tor Lillqvist <tml@iki.fi>.  */
/* Mac OS X code written by Bruno Haible <bruno@clisp.org>.  */

#include <config.h>

/* Specification.  */
#ifdef IN_LIBINTL
# include "gettextP.h"
#else
# include "localename.h"
#endif

#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>

#if HAVE_USELOCALE
/* Mac OS X 10.5 defines the locale_t type in <xlocale.h>.  */
# if defined __APPLE__ && defined __MACH__
#  include <xlocale.h>
# endif
# include <langinfo.h>
# if !defined IN_LIBINTL
#  include "glthread/lock.h"
# endif
#endif

#if HAVE_CFLOCALECOPYCURRENT || HAVE_CFPREFERENCESCOPYAPPVALUE
# include <CoreFoundation/CFString.h>
# if HAVE_CFLOCALECOPYCURRENT
#  include <CoreFoundation/CFLocale.h>
# elif HAVE_CFPREFERENCESCOPYAPPVALUE
#  include <CoreFoundation/CFPreferences.h>
# endif
#endif

#if defined _WIN32 || defined __WIN32__
# define WINDOWS_NATIVE
# if !defined IN_LIBINTL
#  include "glthread/lock.h"
# endif
#endif

#if defined WINDOWS_NATIVE || defined __CYGWIN__ /* Native Windows or Cygwin */
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <winnls.h>
/* List of language codes, sorted by value:
   0x01 LANG_ARABIC
   0x02 LANG_BULGARIAN
   0x03 LANG_CATALAN
   0x04 LANG_CHINESE
   0x05 LANG_CZECH
   0x06 LANG_DANISH
   0x07 LANG_GERMAN
   0x08 LANG_GREEK
   0x09 LANG_ENGLISH
   0x0a LANG_SPANISH
   0x0b LANG_FINNISH
   0x0c LANG_FRENCH
   0x0d LANG_HEBREW
   0x0e LANG_HUNGARIAN
   0x0f LANG_ICELANDIC
   0x10 LANG_ITALIAN
   0x11 LANG_JAPANESE
   0x12 LANG_KOREAN
   0x13 LANG_DUTCH
   0x14 LANG_NORWEGIAN
   0x15 LANG_POLISH
   0x16 LANG_PORTUGUESE
   0x17 LANG_ROMANSH
   0x18 LANG_ROMANIAN
   0x19 LANG_RUSSIAN
   0x1a LANG_CROATIAN == LANG_SERBIAN
   0x1b LANG_SLOVAK
   0x1c LANG_ALBANIAN
   0x1d LANG_SWEDISH
   0x1e LANG_THAI
   0x1f LANG_TURKISH
   0x20 LANG_URDU
   0x21 LANG_INDONESIAN
   0x22 LANG_UKRAINIAN
   0x23 LANG_BELARUSIAN
   0x24 LANG_SLOVENIAN
   0x25 LANG_ESTONIAN
   0x26 LANG_LATVIAN
   0x27 LANG_LITHUANIAN
   0x28 LANG_TAJIK
   0x29 LANG_FARSI
   0x2a LANG_VIETNAMESE
   0x2b LANG_ARMENIAN
   0x2c LANG_AZERI
   0x2d LANG_BASQUE
   0x2e LANG_SORBIAN
   0x2f LANG_MACEDONIAN
   0x30 LANG_SUTU
   0x31 LANG_TSONGA
   0x32 LANG_TSWANA
   0x33 LANG_VENDA
   0x34 LANG_XHOSA
   0x35 LANG_ZULU
   0x36 LANG_AFRIKAANS
   0x37 LANG_GEORGIAN
   0x38 LANG_FAEROESE
   0x39 LANG_HINDI
   0x3a LANG_MALTESE
   0x3b LANG_SAMI
   0x3c LANG_GAELIC
   0x3d LANG_YIDDISH
   0x3e LANG_MALAY
   0x3f LANG_KAZAK
   0x40 LANG_KYRGYZ
   0x41 LANG_SWAHILI
   0x42 LANG_TURKMEN
   0x43 LANG_UZBEK
   0x44 LANG_TATAR
   0x45 LANG_BENGALI
   0x46 LANG_PUNJABI
   0x47 LANG_GUJARATI
   0x48 LANG_ORIYA
   0x49 LANG_TAMIL
   0x4a LANG_TELUGU
   0x4b LANG_KANNADA
   0x4c LANG_MALAYALAM
   0x4d LANG_ASSAMESE
   0x4e LANG_MARATHI
   0x4f LANG_SANSKRIT
   0x50 LANG_MONGOLIAN
   0x51 LANG_TIBETAN
   0x52 LANG_WELSH
   0x53 LANG_CAMBODIAN
   0x54 LANG_LAO
   0x55 LANG_BURMESE
   0x56 LANG_GALICIAN
   0x57 LANG_KONKANI
   0x58 LANG_MANIPURI
   0x59 LANG_SINDHI
   0x5a LANG_SYRIAC
   0x5b LANG_SINHALESE
   0x5c LANG_CHEROKEE
   0x5d LANG_INUKTITUT
   0x5e LANG_AMHARIC
   0x5f LANG_TAMAZIGHT
   0x60 LANG_KASHMIRI
   0x61 LANG_NEPALI
   0x62 LANG_FRISIAN
   0x63 LANG_PASHTO
   0x64 LANG_TAGALOG
   0x65 LANG_DIVEHI
   0x66 LANG_EDO
   0x67 LANG_FULFULDE
   0x68 LANG_HAUSA
   0x69 LANG_IBIBIO
   0x6a LANG_YORUBA
   0x6d LANG_BASHKIR
   0x6e LANG_LUXEMBOURGISH
   0x6f LANG_GREENLANDIC
   0x70 LANG_IGBO
   0x71 LANG_KANURI
   0x72 LANG_OROMO
   0x73 LANG_TIGRINYA
   0x74 LANG_GUARANI
   0x75 LANG_HAWAIIAN
   0x76 LANG_LATIN
   0x77 LANG_SOMALI
   0x78 LANG_YI
   0x79 LANG_PAPIAMENTU
   0x7a LANG_MAPUDUNGUN
   0x7c LANG_MOHAWK
   0x7e LANG_BRETON
   0x82 LANG_OCCITAN
   0x83 LANG_CORSICAN
   0x84 LANG_ALSATIAN
   0x85 LANG_YAKUT
   0x86 LANG_KICHE
   0x87 LANG_KINYARWANDA
   0x88 LANG_WOLOF
   0x8c LANG_DARI
   0x91 LANG_SCOTTISH_GAELIC
*/
/* Mingw headers don't have latest language and sublanguage codes.  */
# ifndef LANG_AFRIKAANS
# define LANG_AFRIKAANS 0x36
# endif
# ifndef LANG_ALBANIAN
# define LANG_ALBANIAN 0x1c
# endif
# ifndef LANG_ALSATIAN
# define LANG_ALSATIAN 0x84
# endif
# ifndef LANG_AMHARIC
# define LANG_AMHARIC 0x5e
# endif
# ifndef LANG_ARABIC
# define LANG_ARABIC 0x01
# endif
# ifndef LANG_ARMENIAN
# define LANG_ARMENIAN 0x2b
# endif
# ifndef LANG_ASSAMESE
# define LANG_ASSAMESE 0x4d
# endif
# ifndef LANG_AZERI
# define LANG_AZERI 0x2c
# endif
# ifndef LANG_BASHKIR
# define LANG_BASHKIR 0x6d
# endif
# ifndef LANG_BASQUE
# define LANG_BASQUE 0x2d
# endif
# ifndef LANG_BELARUSIAN
# define LANG_BELARUSIAN 0x23
# endif
# ifndef LANG_BENGALI
# define LANG_BENGALI 0x45
# endif
# ifndef LANG_BRETON
# define LANG_BRETON 0x7e
# endif
# ifndef LANG_BURMESE
# define LANG_BURMESE 0x55
# endif
# ifndef LANG_CAMBODIAN
# define LANG_CAMBODIAN 0x53
# endif
# ifndef LANG_CATALAN
# define LANG_CATALAN 0x03
# endif
# ifndef LANG_CHEROKEE
# define LANG_CHEROKEE 0x5c
# endif
# ifndef LANG_CORSICAN
# define LANG_CORSICAN 0x83
# endif
# ifndef LANG_DARI
# define LANG_DARI 0x8c
# endif
# ifndef LANG_DIVEHI
# define LANG_DIVEHI 0x65
# endif
# ifndef LANG_EDO
# define LANG_EDO 0x66
# endif
# ifndef LANG_ESTONIAN
# define LANG_ESTONIAN 0x25
# endif
# ifndef LANG_FAEROESE
# define LANG_FAEROESE 0x38
# endif
# ifndef LANG_FARSI
# define LANG_FARSI 0x29
# endif
# ifndef LANG_FRISIAN
# define LANG_FRISIAN 0x62
# endif
# ifndef LANG_FULFULDE
# define LANG_FULFULDE 0x67
# endif
# ifndef LANG_GAELIC
# define LANG_GAELIC 0x3c
# endif
# ifndef LANG_GALICIAN
# define LANG_GALICIAN 0x56
# endif
# ifndef LANG_GEORGIAN
# define LANG_GEORGIAN 0x37
# endif
# ifndef LANG_GREENLANDIC
# define LANG_GREENLANDIC 0x6f
# endif
# ifndef LANG_GUARANI
# define LANG_GUARANI 0x74
# endif
# ifndef LANG_GUJARATI
# define LANG_GUJARATI 0x47
# endif
# ifndef LANG_HAUSA
# define LANG_HAUSA 0x68
# endif
# ifndef LANG_HAWAIIAN
# define LANG_HAWAIIAN 0x75
# endif
# ifndef LANG_HEBREW
# define LANG_HEBREW 0x0d
# endif
# ifndef LANG_HINDI
# define LANG_HINDI 0x39
# endif
# ifndef LANG_IBIBIO
# define LANG_IBIBIO 0x69
# endif
# ifndef LANG_IGBO
# define LANG_IGBO 0x70
# endif
# ifndef LANG_INDONESIAN
# define LANG_INDONESIAN 0x21
# endif
# ifndef LANG_INUKTITUT
# define LANG_INUKTITUT 0x5d
# endif
# ifndef LANG_KANNADA
# define LANG_KANNADA 0x4b
# endif
# ifndef LANG_KANURI
# define LANG_KANURI 0x71
# endif
# ifndef LANG_KASHMIRI
# define LANG_KASHMIRI 0x60
# endif
# ifndef LANG_KAZAK
# define LANG_KAZAK 0x3f
# endif
# ifndef LANG_KICHE
# define LANG_KICHE 0x86
# endif
# ifndef LANG_KINYARWANDA
# define LANG_KINYARWANDA 0x87
# endif
# ifndef LANG_KONKANI
# define LANG_KONKANI 0x57
# endif
# ifndef LANG_KYRGYZ
# define LANG_KYRGYZ 0x40
# endif
# ifndef LANG_LAO
# define LANG_LAO 0x54
# endif
# ifndef LANG_LATIN
# define LANG_LATIN 0x76
# endif
# ifndef LANG_LATVIAN
# define LANG_LATVIAN 0x26
# endif
# ifndef LANG_LITHUANIAN
# define LANG_LITHUANIAN 0x27
# endif
# ifndef LANG_LUXEMBOURGISH
# define LANG_LUXEMBOURGISH 0x6e
# endif
# ifndef LANG_MACEDONIAN
# define LANG_MACEDONIAN 0x2f
# endif
# ifndef LANG_MALAY
# define LANG_MALAY 0x3e
# endif
# ifndef LANG_MALAYALAM
# define LANG_MALAYALAM 0x4c
# endif
# ifndef LANG_MALTESE
# define LANG_MALTESE 0x3a
# endif
# ifndef LANG_MANIPURI
# define LANG_MANIPURI 0x58
# endif
# ifndef LANG_MAORI
# define LANG_MAORI 0x81
# endif
# ifndef LANG_MAPUDUNGUN
# define LANG_MAPUDUNGUN 0x7a
# endif
# ifndef LANG_MARATHI
# define LANG_MARATHI 0x4e
# endif
# ifndef LANG_MOHAWK
# define LANG_MOHAWK 0x7c
# endif
# ifndef LANG_MONGOLIAN
# define LANG_MONGOLIAN 0x50
# endif
# ifndef LANG_NEPALI
# define LANG_NEPALI 0x61
# endif
# ifndef LANG_OCCITAN
# define LANG_OCCITAN 0x82
# endif
# ifndef LANG_ORIYA
# define LANG_ORIYA 0x48
# endif
# ifndef LANG_OROMO
# define LANG_OROMO 0x72
# endif
# ifndef LANG_PAPIAMENTU
# define LANG_PAPIAMENTU 0x79
# endif
# ifndef LANG_PASHTO
# define LANG_PASHTO 0x63
# endif
# ifndef LANG_PUNJABI
# define LANG_PUNJABI 0x46
# endif
# ifndef LANG_QUECHUA
# define LANG_QUECHUA 0x6b
# endif
# ifndef LANG_ROMANSH
# define LANG_ROMANSH 0x17
# endif
# ifndef LANG_SAMI
# define LANG_SAMI 0x3b
# endif
# ifndef LANG_SANSKRIT
# define LANG_SANSKRIT 0x4f
# endif
# ifndef LANG_SCOTTISH_GAELIC
# define LANG_SCOTTISH_GAELIC 0x91
# endif
# ifndef LANG_SERBIAN
# define LANG_SERBIAN 0x1a
# endif
# ifndef LANG_SINDHI
# define LANG_SINDHI 0x59
# endif
# ifndef LANG_SINHALESE
# define LANG_SINHALESE 0x5b
# endif
# ifndef LANG_SLOVAK
# define LANG_SLOVAK 0x1b
# endif
# ifndef LANG_SOMALI
# define LANG_SOMALI 0x77
# endif
# ifndef LANG_SORBIAN
# define LANG_SORBIAN 0x2e
# endif
# ifndef LANG_SOTHO
# define LANG_SOTHO 0x6c
# endif
# ifndef LANG_SUTU
# define LANG_SUTU 0x30
# endif
# ifndef LANG_SWAHILI
# define LANG_SWAHILI 0x41
# endif
# ifndef LANG_SYRIAC
# define LANG_SYRIAC 0x5a
# endif
# ifndef LANG_TAGALOG
# define LANG_TAGALOG 0x64
# endif
# ifndef LANG_TAJIK
# define LANG_TAJIK 0x28
# endif
# ifndef LANG_TAMAZIGHT
# define LANG_TAMAZIGHT 0x5f
# endif
# ifndef LANG_TAMIL
# define LANG_TAMIL 0x49
# endif
# ifndef LANG_TATAR
# define LANG_TATAR 0x44
# endif
# ifndef LANG_TELUGU
# define LANG_TELUGU 0x4a
# endif
# ifndef LANG_THAI
# define LANG_THAI 0x1e
# endif
# ifndef LANG_TIBETAN
# define LANG_TIBETAN 0x51
# endif
# ifndef LANG_TIGRINYA
# define LANG_TIGRINYA 0x73
# endif
# ifndef LANG_TSONGA
# define LANG_TSONGA 0x31
# endif
# ifndef LANG_TSWANA
# define LANG_TSWANA 0x32
# endif
# ifndef LANG_TURKMEN
# define LANG_TURKMEN 0x42
# endif
# ifndef LANG_UIGHUR
# define LANG_UIGHUR 0x80
# endif
# ifndef LANG_UKRAINIAN
# define LANG_UKRAINIAN 0x22
# endif
# ifndef LANG_URDU
# define LANG_URDU 0x20
# endif
# ifndef LANG_UZBEK
# define LANG_UZBEK 0x43
# endif
# ifndef LANG_VENDA
# define LANG_VENDA 0x33
# endif
# ifndef LANG_VIETNAMESE
# define LANG_VIETNAMESE 0x2a
# endif
# ifndef LANG_WELSH
# define LANG_WELSH 0x52
# endif
# ifndef LANG_WOLOF
# define LANG_WOLOF 0x88
# endif
# ifndef LANG_XHOSA
# define LANG_XHOSA 0x34
# endif
# ifndef LANG_YAKUT
# define LANG_YAKUT 0x85
# endif
# ifndef LANG_YI
# define LANG_YI 0x78
# endif
# ifndef LANG_YIDDISH
# define LANG_YIDDISH 0x3d
# endif
# ifndef LANG_YORUBA
# define LANG_YORUBA 0x6a
# endif
# ifndef LANG_ZULU
# define LANG_ZULU 0x35
# endif
# ifndef SUBLANG_AFRIKAANS_SOUTH_AFRICA
# define SUBLANG_AFRIKAANS_SOUTH_AFRICA 0x01
# endif
# ifndef SUBLANG_ALBANIAN_ALBANIA
# define SUBLANG_ALBANIAN_ALBANIA 0x01
# endif
# ifndef SUBLANG_ALSATIAN_FRANCE
# define SUBLANG_ALSATIAN_FRANCE 0x01
# endif
# ifndef SUBLANG_AMHARIC_ETHIOPIA
# define SUBLANG_AMHARIC_ETHIOPIA 0x01
# endif
# ifndef SUBLANG_ARABIC_SAUDI_ARABIA
# define SUBLANG_ARABIC_SAUDI_ARABIA 0x01
# endif
# ifndef SUBLANG_ARABIC_IRAQ
# define SUBLANG_ARABIC_IRAQ 0x02
# endif
# ifndef SUBLANG_ARABIC_EGYPT
# define SUBLANG_ARABIC_EGYPT 0x03
# endif
# ifndef SUBLANG_ARABIC_LIBYA
# define SUBLANG_ARABIC_LIBYA 0x04
# endif
# ifndef SUBLANG_ARABIC_ALGERIA
# define SUBLANG_ARABIC_ALGERIA 0x05
# endif
# ifndef SUBLANG_ARABIC_MOROCCO
# define SUBLANG_ARABIC_MOROCCO 0x06
# endif
# ifndef SUBLANG_ARABIC_TUNISIA
# define SUBLANG_ARABIC_TUNISIA 0x07
# endif
# ifndef SUBLANG_ARABIC_OMAN
# define SUBLANG_ARABIC_OMAN 0x08
# endif
# ifndef SUBLANG_ARABIC_YEMEN
# define SUBLANG_ARABIC_YEMEN 0x09
# endif
# ifndef SUBLANG_ARABIC_SYRIA
# define SUBLANG_ARABIC_SYRIA 0x0a
# endif
# ifndef SUBLANG_ARABIC_JORDAN
# define SUBLANG_ARABIC_JORDAN 0x0b
# endif
# ifndef SUBLANG_ARABIC_LEBANON
# define SUBLANG_ARABIC_LEBANON 0x0c
# endif
# ifndef SUBLANG_ARABIC_KUWAIT
# define SUBLANG_ARABIC_KUWAIT 0x0d
# endif
# ifndef SUBLANG_ARABIC_UAE
# define SUBLANG_ARABIC_UAE 0x0e
# endif
# ifndef SUBLANG_ARABIC_BAHRAIN
# define SUBLANG_ARABIC_BAHRAIN 0x0f
# endif
# ifndef SUBLANG_ARABIC_QATAR
# define SUBLANG_ARABIC_QATAR 0x10
# endif
# ifndef SUBLANG_ARMENIAN_ARMENIA
# define SUBLANG_ARMENIAN_ARMENIA 0x01
# endif
# ifndef SUBLANG_ASSAMESE_INDIA
# define SUBLANG_ASSAMESE_INDIA 0x01
# endif
# ifndef SUBLANG_AZERI_LATIN
# define SUBLANG_AZERI_LATIN 0x01
# endif
# ifndef SUBLANG_AZERI_CYRILLIC
# define SUBLANG_AZERI_CYRILLIC 0x02
# endif
# ifndef SUBLANG_BASHKIR_RUSSIA
# define SUBLANG_BASHKIR_RUSSIA 0x01
# endif
# ifndef SUBLANG_BASQUE_BASQUE
# define SUBLANG_BASQUE_BASQUE 0x01
# endif
# ifndef SUBLANG_BELARUSIAN_BELARUS
# define SUBLANG_BELARUSIAN_BELARUS 0x01
# endif
# ifndef SUBLANG_BENGALI_INDIA
# define SUBLANG_BENGALI_INDIA 0x01
# endif
# ifndef SUBLANG_BENGALI_BANGLADESH
# define SUBLANG_BENGALI_BANGLADESH 0x02
# endif
# ifndef SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN
# define SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN 0x05
# endif
# ifndef SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC
# define SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC 0x08
# endif
# ifndef SUBLANG_BRETON_FRANCE
# define SUBLANG_BRETON_FRANCE 0x01
# endif
# ifndef SUBLANG_BULGARIAN_BULGARIA
# define SUBLANG_BULGARIAN_BULGARIA 0x01
# endif
# ifndef SUBLANG_CAMBODIAN_CAMBODIA
# define SUBLANG_CAMBODIAN_CAMBODIA 0x01
# endif
# ifndef SUBLANG_CATALAN_SPAIN
# define SUBLANG_CATALAN_SPAIN 0x01
# endif
# ifndef SUBLANG_CORSICAN_FRANCE
# define SUBLANG_CORSICAN_FRANCE 0x01
# endif
# ifndef SUBLANG_CROATIAN_CROATIA
# define SUBLANG_CROATIAN_CROATIA 0x01
# endif
# ifndef SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN
# define SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN 0x04
# endif
# ifndef SUBLANG_CHINESE_MACAU
# define SUBLANG_CHINESE_MACAU 0x05
# endif
# ifndef SUBLANG_CZECH_CZECH_REPUBLIC
# define SUBLANG_CZECH_CZECH_REPUBLIC 0x01
# endif
# ifndef SUBLANG_DANISH_DENMARK
# define SUBLANG_DANISH_DENMARK 0x01
# endif
# ifndef SUBLANG_DARI_AFGHANISTAN
# define SUBLANG_DARI_AFGHANISTAN 0x01
# endif
# ifndef SUBLANG_DIVEHI_MALDIVES
# define SUBLANG_DIVEHI_MALDIVES 0x01
# endif
# ifndef SUBLANG_DUTCH_SURINAM
# define SUBLANG_DUTCH_SURINAM 0x03
# endif
# ifndef SUBLANG_ENGLISH_SOUTH_AFRICA
# define SUBLANG_ENGLISH_SOUTH_AFRICA 0x07
# endif
# ifndef SUBLANG_ENGLISH_JAMAICA
# define SUBLANG_ENGLISH_JAMAICA 0x08
# endif
# ifndef SUBLANG_ENGLISH_CARIBBEAN
# define SUBLANG_ENGLISH_CARIBBEAN 0x09
# endif
# ifndef SUBLANG_ENGLISH_BELIZE
# define SUBLANG_ENGLISH_BELIZE 0x0a
# endif
# ifndef SUBLANG_ENGLISH_TRINIDAD
# define SUBLANG_ENGLISH_TRINIDAD 0x0b
# endif
# ifndef SUBLANG_ENGLISH_ZIMBABWE
# define SUBLANG_ENGLISH_ZIMBABWE 0x0c
# endif
# ifndef SUBLANG_ENGLISH_PHILIPPINES
# define SUBLANG_ENGLISH_PHILIPPINES 0x0d
# endif
# ifndef SUBLANG_ENGLISH_INDONESIA
# define SUBLANG_ENGLISH_INDONESIA 0x0e
# endif
# ifndef SUBLANG_ENGLISH_HONGKONG
# define SUBLANG_ENGLISH_HONGKONG 0x0f
# endif
# ifndef SUBLANG_ENGLISH_INDIA
# define SUBLANG_ENGLISH_INDIA 0x10
# endif
# ifndef SUBLANG_ENGLISH_MALAYSIA
# define SUBLANG_ENGLISH_MALAYSIA 0x11
# endif
# ifndef SUBLANG_ENGLISH_SINGAPORE
# define SUBLANG_ENGLISH_SINGAPORE 0x12
# endif
# ifndef SUBLANG_ESTONIAN_ESTONIA
# define SUBLANG_ESTONIAN_ESTONIA 0x01
# endif
# ifndef SUBLANG_FAEROESE_FAROE_ISLANDS
# define SUBLANG_FAEROESE_FAROE_ISLANDS 0x01
# endif
# ifndef SUBLANG_FARSI_IRAN
# define SUBLANG_FARSI_IRAN 0x01
# endif
# ifndef SUBLANG_FINNISH_FINLAND
# define SUBLANG_FINNISH_FINLAND 0x01
# endif
# ifndef SUBLANG_FRENCH_LUXEMBOURG
# define SUBLANG_FRENCH_LUXEMBOURG 0x05
# endif
# ifndef SUBLANG_FRENCH_MONACO
# define SUBLANG_FRENCH_MONACO 0x06
# endif
# ifndef SUBLANG_FRENCH_WESTINDIES
# define SUBLANG_FRENCH_WESTINDIES 0x07
# endif
# ifndef SUBLANG_FRENCH_REUNION
# define SUBLANG_FRENCH_REUNION 0x08
# endif
# ifndef SUBLANG_FRENCH_CONGO
# define SUBLANG_FRENCH_CONGO 0x09
# endif
# ifndef SUBLANG_FRENCH_SENEGAL
# define SUBLANG_FRENCH_SENEGAL 0x0a
# endif
# ifndef SUBLANG_FRENCH_CAMEROON
# define SUBLANG_FRENCH_CAMEROON 0x0b
# endif
# ifndef SUBLANG_FRENCH_COTEDIVOIRE
# define SUBLANG_FRENCH_COTEDIVOIRE 0x0c
# endif
# ifndef SUBLANG_FRENCH_MALI
# define SUBLANG_FRENCH_MALI 0x0d
# endif
# ifndef SUBLANG_FRENCH_MOROCCO
# define SUBLANG_FRENCH_MOROCCO 0x0e
# endif
# ifndef SUBLANG_FRENCH_HAITI
# define SUBLANG_FRENCH_HAITI 0x0f
# endif
# ifndef SUBLANG_FRISIAN_NETHERLANDS
# define SUBLANG_FRISIAN_NETHERLANDS 0x01
# endif
# ifndef SUBLANG_GALICIAN_SPAIN
# define SUBLANG_GALICIAN_SPAIN 0x01
# endif
# ifndef SUBLANG_GEORGIAN_GEORGIA
# define SUBLANG_GEORGIAN_GEORGIA 0x01
# endif
# ifndef SUBLANG_GERMAN_LUXEMBOURG
# define SUBLANG_GERMAN_LUXEMBOURG 0x04
# endif
# ifndef SUBLANG_GERMAN_LIECHTENSTEIN
# define SUBLANG_GERMAN_LIECHTENSTEIN 0x05
# endif
# ifndef SUBLANG_GREEK_GREECE
# define SUBLANG_GREEK_GREECE 0x01
# endif
# ifndef SUBLANG_GREENLANDIC_GREENLAND
# define SUBLANG_GREENLANDIC_GREENLAND 0x01
# endif
# ifndef SUBLANG_GUJARATI_INDIA
# define SUBLANG_GUJARATI_INDIA 0x01
# endif
# ifndef SUBLANG_HAUSA_NIGERIA_LATIN
# define SUBLANG_HAUSA_NIGERIA_LATIN 0x01
# endif
# ifndef SUBLANG_HEBREW_ISRAEL
# define SUBLANG_HEBREW_ISRAEL 0x01
# endif
# ifndef SUBLANG_HINDI_INDIA
# define SUBLANG_HINDI_INDIA 0x01
# endif
# ifndef SUBLANG_HUNGARIAN_HUNGARY
# define SUBLANG_HUNGARIAN_HUNGARY 0x01
# endif
# ifndef SUBLANG_ICELANDIC_ICELAND
# define SUBLANG_ICELANDIC_ICELAND 0x01
# endif
# ifndef SUBLANG_IGBO_NIGERIA
# define SUBLANG_IGBO_NIGERIA 0x01
# endif
# ifndef SUBLANG_INDONESIAN_INDONESIA
# define SUBLANG_INDONESIAN_INDONESIA 0x01
# endif
# ifndef SUBLANG_INUKTITUT_CANADA
# define SUBLANG_INUKTITUT_CANADA 0x01
# endif
# undef SUBLANG_INUKTITUT_CANADA_LATIN
# define SUBLANG_INUKTITUT_CANADA_LATIN 0x02
# undef SUBLANG_IRISH_IRELAND
# define SUBLANG_IRISH_IRELAND 0x02
# ifndef SUBLANG_JAPANESE_JAPAN
# define SUBLANG_JAPANESE_JAPAN 0x01
# endif
# ifndef SUBLANG_KANNADA_INDIA
# define SUBLANG_KANNADA_INDIA 0x01
# endif
# ifndef SUBLANG_KASHMIRI_INDIA
# define SUBLANG_KASHMIRI_INDIA 0x02
# endif
# ifndef SUBLANG_KAZAK_KAZAKHSTAN
# define SUBLANG_KAZAK_KAZAKHSTAN 0x01
# endif
# ifndef SUBLANG_KICHE_GUATEMALA
# define SUBLANG_KICHE_GUATEMALA 0x01
# endif
# ifndef SUBLANG_KINYARWANDA_RWANDA
# define SUBLANG_KINYARWANDA_RWANDA 0x01
# endif
# ifndef SUBLANG_KONKANI_INDIA
# define SUBLANG_KONKANI_INDIA 0x01
# endif
# ifndef SUBLANG_KYRGYZ_KYRGYZSTAN
# define SUBLANG_KYRGYZ_KYRGYZSTAN 0x01
# endif
# ifndef SUBLANG_LAO_LAOS
# define SUBLANG_LAO_LAOS 0x01
# endif
# ifndef SUBLANG_LATVIAN_LATVIA
# define SUBLANG_LATVIAN_LATVIA 0x01
# endif
# ifndef SUBLANG_LITHUANIAN_LITHUANIA
# define SUBLANG_LITHUANIAN_LITHUANIA 0x01
# endif
# undef SUBLANG_LOWER_SORBIAN_GERMANY
# define SUBLANG_LOWER_SORBIAN_GERMANY 0x02
# ifndef SUBLANG_LUXEMBOURGISH_LUXEMBOURG
# define SUBLANG_LUXEMBOURGISH_LUXEMBOURG 0x01
# endif
# ifndef SUBLANG_MACEDONIAN_MACEDONIA
# define SUBLANG_MACEDONIAN_MACEDONIA 0x01
# endif
# ifndef SUBLANG_MALAY_MALAYSIA
# define SUBLANG_MALAY_MALAYSIA 0x01
# endif
# ifndef SUBLANG_MALAY_BRUNEI_DARUSSALAM
# define SUBLANG_MALAY_BRUNEI_DARUSSALAM 0x02
# endif
# ifndef SUBLANG_MALAYALAM_INDIA
# define SUBLANG_MALAYALAM_INDIA 0x01
# endif
# ifndef SUBLANG_MALTESE_MALTA
# define SUBLANG_MALTESE_MALTA 0x01
# endif
# ifndef SUBLANG_MAORI_NEW_ZEALAND
# define SUBLANG_MAORI_NEW_ZEALAND 0x01
# endif
# ifndef SUBLANG_MAPUDUNGUN_CHILE
# define SUBLANG_MAPUDUNGUN_CHILE 0x01
# endif
# ifndef SUBLANG_MARATHI_INDIA
# define SUBLANG_MARATHI_INDIA 0x01
# endif
# ifndef SUBLANG_MOHAWK_CANADA
# define SUBLANG_MOHAWK_CANADA 0x01
# endif
# ifndef SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA
# define SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA 0x01
# endif
# ifndef SUBLANG_MONGOLIAN_PRC
# define SUBLANG_MONGOLIAN_PRC 0x02
# endif
# ifndef SUBLANG_NEPALI_NEPAL
# define SUBLANG_NEPALI_NEPAL 0x01
# endif
# ifndef SUBLANG_NEPALI_INDIA
# define SUBLANG_NEPALI_INDIA 0x02
# endif
# ifndef SUBLANG_OCCITAN_FRANCE
# define SUBLANG_OCCITAN_FRANCE 0x01
# endif
# ifndef SUBLANG_ORIYA_INDIA
# define SUBLANG_ORIYA_INDIA 0x01
# endif
# ifndef SUBLANG_PASHTO_AFGHANISTAN
# define SUBLANG_PASHTO_AFGHANISTAN 0x01
# endif
# ifndef SUBLANG_POLISH_POLAND
# define SUBLANG_POLISH_POLAND 0x01
# endif
# ifndef SUBLANG_PUNJABI_INDIA
# define SUBLANG_PUNJABI_INDIA 0x01
# endif
# ifndef SUBLANG_PUNJABI_PAKISTAN
# define SUBLANG_PUNJABI_PAKISTAN 0x02
# endif
# ifndef SUBLANG_QUECHUA_BOLIVIA
# define SUBLANG_QUECHUA_BOLIVIA 0x01
# endif
# ifndef SUBLANG_QUECHUA_ECUADOR
# define SUBLANG_QUECHUA_ECUADOR 0x02
# endif
# ifndef SUBLANG_QUECHUA_PERU
# define SUBLANG_QUECHUA_PERU 0x03
# endif
# ifndef SUBLANG_ROMANIAN_ROMANIA
# define SUBLANG_ROMANIAN_ROMANIA 0x01
# endif
# ifndef SUBLANG_ROMANIAN_MOLDOVA
# define SUBLANG_ROMANIAN_MOLDOVA 0x02
# endif
# ifndef SUBLANG_ROMANSH_SWITZERLAND
# define SUBLANG_ROMANSH_SWITZERLAND 0x01
# endif
# ifndef SUBLANG_RUSSIAN_RUSSIA
# define SUBLANG_RUSSIAN_RUSSIA 0x01
# endif
# ifndef SUBLANG_RUSSIAN_MOLDAVIA
# define SUBLANG_RUSSIAN_MOLDAVIA 0x02
# endif
# ifndef SUBLANG_SAMI_NORTHERN_NORWAY
# define SUBLANG_SAMI_NORTHERN_NORWAY 0x01
# endif
# ifndef SUBLANG_SAMI_NORTHERN_SWEDEN
# define SUBLANG_SAMI_NORTHERN_SWEDEN 0x02
# endif
# ifndef SUBLANG_SAMI_NORTHERN_FINLAND
# define SUBLANG_SAMI_NORTHERN_FINLAND 0x03
# endif
# ifndef SUBLANG_SAMI_LULE_NORWAY
# define SUBLANG_SAMI_LULE_NORWAY 0x04
# endif
# ifndef SUBLANG_SAMI_LULE_SWEDEN
# define SUBLANG_SAMI_LULE_SWEDEN 0x05
# endif
# ifndef SUBLANG_SAMI_SOUTHERN_NORWAY
# define SUBLANG_SAMI_SOUTHERN_NORWAY 0x06
# endif
# ifndef SUBLANG_SAMI_SOUTHERN_SWEDEN
# define SUBLANG_SAMI_SOUTHERN_SWEDEN 0x07
# endif
# undef SUBLANG_SAMI_SKOLT_FINLAND
# define SUBLANG_SAMI_SKOLT_FINLAND 0x08
# undef SUBLANG_SAMI_INARI_FINLAND
# define SUBLANG_SAMI_INARI_FINLAND 0x09
# ifndef SUBLANG_SANSKRIT_INDIA
# define SUBLANG_SANSKRIT_INDIA 0x01
# endif
# ifndef SUBLANG_SERBIAN_LATIN
# define SUBLANG_SERBIAN_LATIN 0x02
# endif
# ifndef SUBLANG_SERBIAN_CYRILLIC
# define SUBLANG_SERBIAN_CYRILLIC 0x03
# endif
# ifndef SUBLANG_SINDHI_INDIA
# define SUBLANG_SINDHI_INDIA 0x01
# endif
# undef SUBLANG_SINDHI_PAKISTAN
# define SUBLANG_SINDHI_PAKISTAN 0x02
# ifndef SUBLANG_SINDHI_AFGHANISTAN
# define SUBLANG_SINDHI_AFGHANISTAN 0x02
# endif
# ifndef SUBLANG_SINHALESE_SRI_LANKA
# define SUBLANG_SINHALESE_SRI_LANKA 0x01
# endif
# ifndef SUBLANG_SLOVAK_SLOVAKIA
# define SUBLANG_SLOVAK_SLOVAKIA 0x01
# endif
# ifndef SUBLANG_SLOVENIAN_SLOVENIA
# define SUBLANG_SLOVENIAN_SLOVENIA 0x01
# endif
# ifndef SUBLANG_SOTHO_SOUTH_AFRICA
# define SUBLANG_SOTHO_SOUTH_AFRICA 0x01
# endif
# ifndef SUBLANG_SPANISH_GUATEMALA
# define SUBLANG_SPANISH_GUATEMALA 0x04
# endif
# ifndef SUBLANG_SPANISH_COSTA_RICA
# define SUBLANG_SPANISH_COSTA_RICA 0x05
# endif
# ifndef SUBLANG_SPANISH_PANAMA
# define SUBLANG_SPANISH_PANAMA 0x06
# endif
# ifndef SUBLANG_SPANISH_DOMINICAN_REPUBLIC
# define SUBLANG_SPANISH_DOMINICAN_REPUBLIC 0x07
# endif
# ifndef SUBLANG_SPANISH_VENEZUELA
# define SUBLANG_SPANISH_VENEZUELA 0x08
# endif
# ifndef SUBLANG_SPANISH_COLOMBIA
# define SUBLANG_SPANISH_COLOMBIA 0x09
# endif
# ifndef SUBLANG_SPANISH_PERU
# define SUBLANG_SPANISH_PERU 0x0a
# endif
# ifndef SUBLANG_SPANISH_ARGENTINA
# define SUBLANG_SPANISH_ARGENTINA 0x0b
# endif
# ifndef SUBLANG_SPANISH_ECUADOR
# define SUBLANG_SPANISH_ECUADOR 0x0c
# endif
# ifndef SUBLANG_SPANISH_CHILE
# define SUBLANG_SPANISH_CHILE 0x0d
# endif
# ifndef SUBLANG_SPANISH_URUGUAY
# define SUBLANG_SPANISH_URUGUAY 0x0e
# endif
# ifndef SUBLANG_SPANISH_PARAGUAY
# define SUBLANG_SPANISH_PARAGUAY 0x0f
# endif
# ifndef SUBLANG_SPANISH_BOLIVIA
# define SUBLANG_SPANISH_BOLIVIA 0x10
# endif
# ifndef SUBLANG_SPANISH_EL_SALVADOR
# define SUBLANG_SPANISH_EL_SALVADOR 0x11
# endif
# ifndef SUBLANG_SPANISH_HONDURAS
# define SUBLANG_SPANISH_HONDURAS 0x12
# endif
# ifndef SUBLANG_SPANISH_NICARAGUA
# define SUBLANG_SPANISH_NICARAGUA 0x13
# endif
# ifndef SUBLANG_SPANISH_PUERTO_RICO
# define SUBLANG_SPANISH_PUERTO_RICO 0x14
# endif
# ifndef SUBLANG_SPANISH_US
# define SUBLANG_SPANISH_US 0x15
# endif
# ifndef SUBLANG_SWAHILI_KENYA
# define SUBLANG_SWAHILI_KENYA 0x01
# endif
# ifndef SUBLANG_SWEDISH_SWEDEN
# define SUBLANG_SWEDISH_SWEDEN 0x01
# endif
# ifndef SUBLANG_SWEDISH_FINLAND
# define SUBLANG_SWEDISH_FINLAND 0x02
# endif
# ifndef SUBLANG_SYRIAC_SYRIA
# define SUBLANG_SYRIAC_SYRIA 0x01
# endif
# ifndef SUBLANG_TAGALOG_PHILIPPINES
# define SUBLANG_TAGALOG_PHILIPPINES 0x01
# endif
# ifndef SUBLANG_TAJIK_TAJIKISTAN
# define SUBLANG_TAJIK_TAJIKISTAN 0x01
# endif
# ifndef SUBLANG_TAMAZIGHT_ARABIC
# define SUBLANG_TAMAZIGHT_ARABIC 0x01
# endif
# ifndef SUBLANG_TAMAZIGHT_ALGERIA_LATIN
# define SUBLANG_TAMAZIGHT_ALGERIA_LATIN 0x02
# endif
# ifndef SUBLANG_TAMIL_INDIA
# define SUBLANG_TAMIL_INDIA 0x01
# endif
# ifndef SUBLANG_TATAR_RUSSIA
# define SUBLANG_TATAR_RUSSIA 0x01
# endif
# ifndef SUBLANG_TELUGU_INDIA
# define SUBLANG_TELUGU_INDIA 0x01
# endif
# ifndef SUBLANG_THAI_THAILAND
# define SUBLANG_THAI_THAILAND 0x01
# endif
# ifndef SUBLANG_TIBETAN_PRC
# define SUBLANG_TIBETAN_PRC 0x01
# endif
# undef SUBLANG_TIBETAN_BHUTAN
# define SUBLANG_TIBETAN_BHUTAN 0x02
# ifndef SUBLANG_TIGRINYA_ETHIOPIA
# define SUBLANG_TIGRINYA_ETHIOPIA 0x01
# endif
# ifndef SUBLANG_TIGRINYA_ERITREA
# define SUBLANG_TIGRINYA_ERITREA 0x02
# endif
# ifndef SUBLANG_TSWANA_SOUTH_AFRICA
# define SUBLANG_TSWANA_SOUTH_AFRICA 0x01
# endif
# ifndef SUBLANG_TURKISH_TURKEY
# define SUBLANG_TURKISH_TURKEY 0x01
# endif
# ifndef SUBLANG_TURKMEN_TURKMENISTAN
# define SUBLANG_TURKMEN_TURKMENISTAN 0x01
# endif
# ifndef SUBLANG_UIGHUR_PRC
# define SUBLANG_UIGHUR_PRC 0x01
# endif
# ifndef SUBLANG_UKRAINIAN_UKRAINE
# define SUBLANG_UKRAINIAN_UKRAINE 0x01
# endif
# ifndef SUBLANG_UPPER_SORBIAN_GERMANY
# define SUBLANG_UPPER_SORBIAN_GERMANY 0x01
# endif
# ifndef SUBLANG_URDU_PAKISTAN
# define SUBLANG_URDU_PAKISTAN 0x01
# endif
# ifndef SUBLANG_URDU_INDIA
# define SUBLANG_URDU_INDIA 0x02
# endif
# ifndef SUBLANG_UZBEK_LATIN
# define SUBLANG_UZBEK_LATIN 0x01
# endif
# ifndef SUBLANG_UZBEK_CYRILLIC
# define SUBLANG_UZBEK_CYRILLIC 0x02
# endif
# ifndef SUBLANG_VIETNAMESE_VIETNAM
# define SUBLANG_VIETNAMESE_VIETNAM 0x01
# endif
# ifndef SUBLANG_WELSH_UNITED_KINGDOM
# define SUBLANG_WELSH_UNITED_KINGDOM 0x01
# endif
# ifndef SUBLANG_WOLOF_SENEGAL
# define SUBLANG_WOLOF_SENEGAL 0x01
# endif
# ifndef SUBLANG_XHOSA_SOUTH_AFRICA
# define SUBLANG_XHOSA_SOUTH_AFRICA 0x01
# endif
# ifndef SUBLANG_YAKUT_RUSSIA
# define SUBLANG_YAKUT_RUSSIA 0x01
# endif
# ifndef SUBLANG_YI_PRC
# define SUBLANG_YI_PRC 0x01
# endif
# ifndef SUBLANG_YORUBA_NIGERIA
# define SUBLANG_YORUBA_NIGERIA 0x01
# endif
# ifndef SUBLANG_ZULU_SOUTH_AFRICA
# define SUBLANG_ZULU_SOUTH_AFRICA 0x01
# endif
/* GetLocaleInfoA operations.  */
# ifndef LOCALE_SNAME
# define LOCALE_SNAME 0x5c
# endif
# ifndef LOCALE_NAME_MAX_LENGTH
# define LOCALE_NAME_MAX_LENGTH 85
# endif
#endif


#if HAVE_CFLOCALECOPYCURRENT || HAVE_CFPREFERENCESCOPYAPPVALUE
/* Mac OS X 10.2 or newer */

/* Canonicalize a Mac OS X locale name to a Unix locale name.
   NAME is a sufficiently large buffer.
   On input, it contains the Mac OS X locale name.
   On output, it contains the Unix locale name.  */
# if !defined IN_LIBINTL
static
# endif
void
gl_locale_name_canonicalize (char *name)
{
  /* This conversion is based on a posting by
     Deborah GoldSmith <goldsmit@apple.com> on 2005-03-08,
     http://lists.apple.com/archives/carbon-dev/2005/Mar/msg00293.html */

  /* Convert legacy (NeXTstep inherited) English names to Unix (ISO 639 and
     ISO 3166) names.  Prior to Mac OS X 10.3, there is no API for doing this.
     Therefore we do it ourselves, using a table based on the results of the
     Mac OS X 10.3.8 function
     CFLocaleCreateCanonicalLocaleIdentifierFromString().  */
  typedef struct { const char legacy[21+1]; const char unixy[5+1]; }
          legacy_entry;
  static const legacy_entry legacy_table[] = {
    { "Afrikaans",             "af" },
    { "Albanian",              "sq" },
    { "Amharic",               "am" },
    { "Arabic",                "ar" },
    { "Armenian",              "hy" },
    { "Assamese",              "as" },
    { "Aymara",                "ay" },
    { "Azerbaijani",           "az" },
    { "Basque",                "eu" },
    { "Belarusian",            "be" },
    { "Belorussian",           "be" },
    { "Bengali",               "bn" },
    { "Brazilian Portugese",   "pt_BR" },
    { "Brazilian Portuguese",  "pt_BR" },
    { "Breton",                "br" },
    { "Bulgarian",             "bg" },
    { "Burmese",               "my" },
    { "Byelorussian",          "be" },
    { "Catalan",               "ca" },
    { "Chewa",                 "ny" },
    { "Chichewa",              "ny" },
    { "Chinese",               "zh" },
    { "Chinese, Simplified",   "zh_CN" },
    { "Chinese, Traditional",  "zh_TW" },
    { "Chinese, Tradtional",   "zh_TW" },
    { "Croatian",              "hr" },
    { "Czech",                 "cs" },
    { "Danish",                "da" },
    { "Dutch",                 "nl" },
    { "Dzongkha",              "dz" },
    { "English",               "en" },
    { "Esperanto",             "eo" },
    { "Estonian",              "et" },
    { "Faroese",               "fo" },
    { "Farsi",                 "fa" },
    { "Finnish",               "fi" },
    { "Flemish",               "nl_BE" },
    { "French",                "fr" },
    { "Galician",              "gl" },
    { "Gallegan",              "gl" },
    { "Georgian",              "ka" },
    { "German",                "de" },
    { "Greek",                 "el" },
    { "Greenlandic",           "kl" },
    { "Guarani",               "gn" },
    { "Gujarati",              "gu" },
    { "Hawaiian",              "haw" }, /* Yes, "haw", not "cpe".  */
    { "Hebrew",                "he" },
    { "Hindi",                 "hi" },
    { "Hungarian",             "hu" },
    { "Icelandic",             "is" },
    { "Indonesian",            "id" },
    { "Inuktitut",             "iu" },
    { "Irish",                 "ga" },
    { "Italian",               "it" },
    { "Japanese",              "ja" },
    { "Javanese",              "jv" },
    { "Kalaallisut",           "kl" },
    { "Kannada",               "kn" },
    { "Kashmiri",              "ks" },
    { "Kazakh",                "kk" },
    { "Khmer",                 "km" },
    { "Kinyarwanda",           "rw" },
    { "Kirghiz",               "ky" },
    { "Korean",                "ko" },
    { "Kurdish",               "ku" },
    { "Latin",                 "la" },
    { "Latvian",               "lv" },
    { "Lithuanian",            "lt" },
    { "Macedonian",            "mk" },
    { "Malagasy",              "mg" },
    { "Malay",                 "ms" },
    { "Malayalam",             "ml" },
    { "Maltese",               "mt" },
    { "Manx",                  "gv" },
    { "Marathi",               "mr" },
    { "Moldavian",             "mo" },
    { "Mongolian",             "mn" },
    { "Nepali",                "ne" },
    { "Norwegian",             "nb" }, /* Yes, "nb", not the obsolete "no".  */
    { "Nyanja",                "ny" },
    { "Nynorsk",               "nn" },
    { "Oriya",                 "or" },
    { "Oromo",                 "om" },
    { "Panjabi",               "pa" },
    { "Pashto",                "ps" },
    { "Persian",               "fa" },
    { "Polish",                "pl" },
    { "Portuguese",            "pt" },
    { "Portuguese, Brazilian", "pt_BR" },
    { "Punjabi",               "pa" },
    { "Pushto",                "ps" },
    { "Quechua",               "qu" },
    { "Romanian",              "ro" },
    { "Ruanda",                "rw" },
    { "Rundi",                 "rn" },
    { "Russian",               "ru" },
    { "Sami",                  "se_NO" }, /* Not just "se".  */
    { "Sanskrit",              "sa" },
    { "Scottish",              "gd" },
    { "Serbian",               "sr" },
    { "Simplified Chinese",    "zh_CN" },
    { "Sindhi",                "sd" },
    { "Sinhalese",             "si" },
    { "Slovak",                "sk" },
    { "Slovenian",             "sl" },
    { "Somali",                "so" },
    { "Spanish",               "es" },
    { "Sundanese",             "su" },
    { "Swahili",               "sw" },
    { "Swedish",               "sv" },
    { "Tagalog",               "tl" },
    { "Tajik",                 "tg" },
    { "Tajiki",                "tg" },
    { "Tamil",                 "ta" },
    { "Tatar",                 "tt" },
    { "Telugu",                "te" },
    { "Thai",                  "th" },
    { "Tibetan",               "bo" },
    { "Tigrinya",              "ti" },
    { "Tongan",                "to" },
    { "Traditional Chinese",   "zh_TW" },
    { "Turkish",               "tr" },
    { "Turkmen",               "tk" },
    { "Uighur",                "ug" },
    { "Ukrainian",             "uk" },
    { "Urdu",                  "ur" },
    { "Uzbek",                 "uz" },
    { "Vietnamese",            "vi" },
    { "Welsh",                 "cy" },
    { "Yiddish",               "yi" }
  };

  /* Convert new-style locale names with language tags (ISO 639 and ISO 15924)
     to Unix (ISO 639 and ISO 3166) names.  */
  typedef struct { const char langtag[7+1]; const char unixy[12+1]; }
          langtag_entry;
  static const langtag_entry langtag_table[] = {
    /* Mac OS X has "az-Arab", "az-Cyrl", "az-Latn".
       The default script for az on Unix is Latin.  */
    { "az-Latn", "az" },
    /* Mac OS X has "ga-dots".  Does not yet exist on Unix.  */
    { "ga-dots", "ga" },
    /* Mac OS X has "kk-Cyrl".  Does not yet exist on Unix.  */
    /* Mac OS X has "mn-Cyrl", "mn-Mong".
       The default script for mn on Unix is Cyrillic.  */
    { "mn-Cyrl", "mn" },
    /* Mac OS X has "ms-Arab", "ms-Latn".
       The default script for ms on Unix is Latin.  */
    { "ms-Latn", "ms" },
    /* Mac OS X has "tg-Cyrl".
       The default script for tg on Unix is Cyrillic.  */
    { "tg-Cyrl", "tg" },
    /* Mac OS X has "tk-Cyrl".  Does not yet exist on Unix.  */
    /* Mac OS X has "tt-Cyrl".
       The default script for tt on Unix is Cyrillic.  */
    { "tt-Cyrl", "tt" },
    /* Mac OS X has "zh-Hans", "zh-Hant".
       Country codes are used to distinguish these on Unix.  */
    { "zh-Hans", "zh_CN" },
    { "zh-Hant", "zh_TW" }
  };

  /* Convert script names (ISO 15924) to Unix conventions.
     See http://www.unicode.org/iso15924/iso15924-codes.html  */
  typedef struct { const char script[4+1]; const char unixy[9+1]; }
          script_entry;
  static const script_entry script_table[] = {
    { "Arab", "arabic" },
    { "Cyrl", "cyrillic" },
    { "Mong", "mongolian" }
  };

  /* Step 1: Convert using legacy_table.  */
  if (name[0] >= 'A' && name[0] <= 'Z')
    {
      unsigned int i1, i2;
      i1 = 0;
      i2 = sizeof (legacy_table) / sizeof (legacy_entry);
      while (i2 - i1 > 1)
        {
          /* At this point we know that if name occurs in legacy_table,
             its index must be >= i1 and < i2.  */
          unsigned int i = (i1 + i2) >> 1;
          const legacy_entry *p = &legacy_table[i];
          if (strcmp (name, p->legacy) < 0)
            i2 = i;
          else
            i1 = i;
        }
      if (strcmp (name, legacy_table[i1].legacy) == 0)
        {
          strcpy (name, legacy_table[i1].unixy);
          return;
        }
    }

  /* Step 2: Convert using langtag_table and script_table.  */
  if (strlen (name) == 7 && name[2] == '-')
    {
      unsigned int i1, i2;
      i1 = 0;
      i2 = sizeof (langtag_table) / sizeof (langtag_entry);
      while (i2 - i1 > 1)
        {
          /* At this point we know that if name occurs in langtag_table,
             its index must be >= i1 and < i2.  */
          unsigned int i = (i1 + i2) >> 1;
          const langtag_entry *p = &langtag_table[i];
          if (strcmp (name, p->langtag) < 0)
            i2 = i;
          else
            i1 = i;
        }
      if (strcmp (name, langtag_table[i1].langtag) == 0)
        {
          strcpy (name, langtag_table[i1].unixy);
          return;
        }

      i1 = 0;
      i2 = sizeof (script_table) / sizeof (script_entry);
      while (i2 - i1 > 1)
        {
          /* At this point we know that if (name + 3) occurs in script_table,
             its index must be >= i1 and < i2.  */
          unsigned int i = (i1 + i2) >> 1;
          const script_entry *p = &script_table[i];
          if (strcmp (name + 3, p->script) < 0)
            i2 = i;
          else
            i1 = i;
        }
      if (strcmp (name + 3, script_table[i1].script) == 0)
        {
          name[2] = '@';
          strcpy (name + 3, script_table[i1].unixy);
          return;
        }
    }

  /* Step 3: Convert new-style dash to Unix underscore. */
  {
    char *p;
    for (p = name; *p != '\0'; p++)
      if (*p == '-')
        *p = '_';
  }
}

#endif


#if defined WINDOWS_NATIVE || defined __CYGWIN__ /* Native Windows or Cygwin */

/* Canonicalize a Windows native locale name to a Unix locale name.
   NAME is a sufficiently large buffer.
   On input, it contains the Windows locale name.
   On output, it contains the Unix locale name.  */
# if !defined IN_LIBINTL
static
# endif
void
gl_locale_name_canonicalize (char *name)
{
  /* FIXME: This is probably incomplete: it does not handle "zh-Hans" and
     "zh-Hant".  */
  char *p;

  for (p = name; *p != '\0'; p++)
    if (*p == '-')
      {
        *p = '_';
        p++;
        for (; *p != '\0'; p++)
          {
            if (*p >= 'a' && *p <= 'z')
              *p += 'A' - 'a';
            if (*p == '-')
              {
                *p = '\0';
                return;
              }
          }
        return;
      }
}

# if !defined IN_LIBINTL
static
# endif
const char *
gl_locale_name_from_win32_LANGID (LANGID langid)
{
  /* Activate the new code only when the GETTEXT_MUI environment variable is
     set, for the time being, since the new code is not well tested.  */
  if (getenv ("GETTEXT_MUI") != NULL)
    {
      static char namebuf[256];

      /* Query the system's notion of locale name.
         On Windows95/98/ME, GetLocaleInfoA returns some incorrect results.
         But we don't need to support systems that are so old.  */
      if (GetLocaleInfoA (MAKELCID (langid, SORT_DEFAULT), LOCALE_SNAME,
                          namebuf, sizeof (namebuf) - 1))
        {
          /* Convert it to a Unix locale name.  */
          gl_locale_name_canonicalize (namebuf);
          return namebuf;
        }
    }
  /* Internet Explorer has an LCID to RFC3066 name mapping stored in
     HKEY_CLASSES_ROOT\Mime\Database\Rfc1766.  But we better don't use that
     since IE's i18n subsystem is known to be inconsistent with the native
     Windows base (e.g. they have different character conversion facilities
     that produce different results).  */
  /* Use our own table.  */
  {
    int primary, sub;

    /* Split into language and territory part.  */
    primary = PRIMARYLANGID (langid);
    sub = SUBLANGID (langid);

    /* Dispatch on language.
       See also http://www.unicode.org/unicode/onlinedat/languages.html .
       For details about languages, see http://www.ethnologue.com/ .  */
    switch (primary)
      {
      case LANG_AFRIKAANS:
        switch (sub)
          {
          case SUBLANG_AFRIKAANS_SOUTH_AFRICA: return "af_ZA";
          }
        return "af";
      case LANG_ALBANIAN:
        switch (sub)
          {
          case SUBLANG_ALBANIAN_ALBANIA: return "sq_AL";
          }
        return "sq";
      case LANG_ALSATIAN:
        switch (sub)
          {
          case SUBLANG_ALSATIAN_FRANCE: return "gsw_FR";
          }
        return "gsw";
      case LANG_AMHARIC:
        switch (sub)
          {
          case SUBLANG_AMHARIC_ETHIOPIA: return "am_ET";
          }
        return "am";
      case LANG_ARABIC:
        switch (sub)
          {
          case SUBLANG_ARABIC_SAUDI_ARABIA: return "ar_SA";
          case SUBLANG_ARABIC_IRAQ: return "ar_IQ";
          case SUBLANG_ARABIC_EGYPT: return "ar_EG";
          case SUBLANG_ARABIC_LIBYA: return "ar_LY";
          case SUBLANG_ARABIC_ALGERIA: return "ar_DZ";
          case SUBLANG_ARABIC_MOROCCO: return "ar_MA";
          case SUBLANG_ARABIC_TUNISIA: return "ar_TN";
          case SUBLANG_ARABIC_OMAN: return "ar_OM";
          case SUBLANG_ARABIC_YEMEN: return "ar_YE";
          case SUBLANG_ARABIC_SYRIA: return "ar_SY";
          case SUBLANG_ARABIC_JORDAN: return "ar_JO";
          case SUBLANG_ARABIC_LEBANON: return "ar_LB";
          case SUBLANG_ARABIC_KUWAIT: return "ar_KW";
          case SUBLANG_ARABIC_UAE: return "ar_AE";
          case SUBLANG_ARABIC_BAHRAIN: return "ar_BH";
          case SUBLANG_ARABIC_QATAR: return "ar_QA";
          }
        return "ar";
      case LANG_ARMENIAN:
        switch (sub)
          {
          case SUBLANG_ARMENIAN_ARMENIA: return "hy_AM";
          }
        return "hy";
      case LANG_ASSAMESE:
        switch (sub)
          {
          case SUBLANG_ASSAMESE_INDIA: return "as_IN";
          }
        return "as";
      case LANG_AZERI:
        switch (sub)
          {
          /* FIXME: Adjust this when Azerbaijani locales appear on Unix.  */
          case 0x1e: return "az@latin";
          case SUBLANG_AZERI_LATIN: return "az_AZ@latin";
          case 0x1d: return "az@cyrillic";
          case SUBLANG_AZERI_CYRILLIC: return "az_AZ@cyrillic";
          }
        return "az";
      case LANG_BASHKIR:
        switch (sub)
          {
          case SUBLANG_BASHKIR_RUSSIA: return "ba_RU";
          }
        return "ba";
      case LANG_BASQUE:
        switch (sub)
          {
          case SUBLANG_BASQUE_BASQUE: return "eu_ES";
          }
        return "eu"; /* Ambiguous: could be "eu_ES" or "eu_FR".  */
      case LANG_BELARUSIAN:
        switch (sub)
          {
          case SUBLANG_BELARUSIAN_BELARUS: return "be_BY";
          }
        return "be";
      case LANG_BENGALI:
        switch (sub)
          {
          case SUBLANG_BENGALI_INDIA: return "bn_IN";
          case SUBLANG_BENGALI_BANGLADESH: return "bn_BD";
          }
        return "bn";
      case LANG_BRETON:
        switch (sub)
          {
          case SUBLANG_BRETON_FRANCE: return "br_FR";
          }
        return "br";
      case LANG_BULGARIAN:
        switch (sub)
          {
          case SUBLANG_BULGARIAN_BULGARIA: return "bg_BG";
          }
        return "bg";
      case LANG_BURMESE:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "my_MM";
          }
        return "my";
      case LANG_CAMBODIAN:
        switch (sub)
          {
          case SUBLANG_CAMBODIAN_CAMBODIA: return "km_KH";
          }
        return "km";
      case LANG_CATALAN:
        switch (sub)
          {
          case SUBLANG_CATALAN_SPAIN: return "ca_ES";
          }
        return "ca";
      case LANG_CHEROKEE:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "chr_US";
          }
        return "chr";
      case LANG_CHINESE:
        switch (sub)
          {
          case SUBLANG_CHINESE_TRADITIONAL: case 0x1f: return "zh_TW";
          case SUBLANG_CHINESE_SIMPLIFIED: case 0x00: return "zh_CN";
          case SUBLANG_CHINESE_HONGKONG: return "zh_HK"; /* traditional */
          case SUBLANG_CHINESE_SINGAPORE: return "zh_SG"; /* simplified */
          case SUBLANG_CHINESE_MACAU: return "zh_MO"; /* traditional */
          }
        return "zh";
      case LANG_CORSICAN:
        switch (sub)
          {
          case SUBLANG_CORSICAN_FRANCE: return "co_FR";
          }
        return "co";
      case LANG_CROATIAN:      /* LANG_CROATIAN == LANG_SERBIAN == LANG_BOSNIAN
                                * What used to be called Serbo-Croatian
                                * should really now be two separate
                                * languages because of political reasons.
                                * (Says tml, who knows nothing about Serbian
                                * or Croatian.)
                                * (I can feel those flames coming already.)
                                */
        switch (sub)
          {
          /* Croatian */
          case 0x00: return "hr";
          case SUBLANG_CROATIAN_CROATIA: return "hr_HR";
          case SUBLANG_CROATIAN_BOSNIA_HERZEGOVINA_LATIN: return "hr_BA";
          /* Serbian */
          case 0x1f: return "sr";
          case 0x1c: return "sr"; /* latin */
          case SUBLANG_SERBIAN_LATIN: return "sr_CS"; /* latin */
          case 0x09: return "sr_RS"; /* latin */
          case 0x0b: return "sr_ME"; /* latin */
          case 0x06: return "sr_BA"; /* latin */
          case 0x1b: return "sr@cyrillic";
          case SUBLANG_SERBIAN_CYRILLIC: return "sr_CS@cyrillic";
          case 0x0a: return "sr_RS@cyrillic";
          case 0x0c: return "sr_ME@cyrillic";
          case 0x07: return "sr_BA@cyrillic";
          /* Bosnian */
          case 0x1e: return "bs";
          case 0x1a: return "bs"; /* latin */
          case SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN: return "bs_BA"; /* latin */
          case 0x19: return "bs@cyrillic";
          case SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_CYRILLIC: return "bs_BA@cyrillic";
          }
        return "hr";
      case LANG_CZECH:
        switch (sub)
          {
          case SUBLANG_CZECH_CZECH_REPUBLIC: return "cs_CZ";
          }
        return "cs";
      case LANG_DANISH:
        switch (sub)
          {
          case SUBLANG_DANISH_DENMARK: return "da_DK";
          }
        return "da";
      case LANG_DARI:
        /* FIXME: Adjust this when such locales appear on Unix.  */
        switch (sub)
          {
          case SUBLANG_DARI_AFGHANISTAN: return "prs_AF";
          }
        return "prs";
      case LANG_DIVEHI:
        switch (sub)
          {
          case SUBLANG_DIVEHI_MALDIVES: return "dv_MV";
          }
        return "dv";
      case LANG_DUTCH:
        switch (sub)
          {
          case SUBLANG_DUTCH: return "nl_NL";
          case SUBLANG_DUTCH_BELGIAN: /* FLEMISH, VLAAMS */ return "nl_BE";
          case SUBLANG_DUTCH_SURINAM: return "nl_SR";
          }
        return "nl";
      case LANG_EDO:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "bin_NG";
          }
        return "bin";
      case LANG_ENGLISH:
        switch (sub)
          {
          /* SUBLANG_ENGLISH_US == SUBLANG_DEFAULT. Heh. I thought
           * English was the language spoken in England.
           * Oh well.
           */
          case SUBLANG_ENGLISH_US: return "en_US";
          case SUBLANG_ENGLISH_UK: return "en_GB";
          case SUBLANG_ENGLISH_AUS: return "en_AU";
          case SUBLANG_ENGLISH_CAN: return "en_CA";
          case SUBLANG_ENGLISH_NZ: return "en_NZ";
          case SUBLANG_ENGLISH_EIRE: return "en_IE";
          case SUBLANG_ENGLISH_SOUTH_AFRICA: return "en_ZA";
          case SUBLANG_ENGLISH_JAMAICA: return "en_JM";
          case SUBLANG_ENGLISH_CARIBBEAN: return "en_GD"; /* Grenada? */
          case SUBLANG_ENGLISH_BELIZE: return "en_BZ";
          case SUBLANG_ENGLISH_TRINIDAD: return "en_TT";
          case SUBLANG_ENGLISH_ZIMBABWE: return "en_ZW";
          case SUBLANG_ENGLISH_PHILIPPINES: return "en_PH";
          case SUBLANG_ENGLISH_INDONESIA: return "en_ID";
          case SUBLANG_ENGLISH_HONGKONG: return "en_HK";
          case SUBLANG_ENGLISH_INDIA: return "en_IN";
          case SUBLANG_ENGLISH_MALAYSIA: return "en_MY";
          case SUBLANG_ENGLISH_SINGAPORE: return "en_SG";
          }
        return "en";
      case LANG_ESTONIAN:
        switch (sub)
          {
          case SUBLANG_ESTONIAN_ESTONIA: return "et_EE";
          }
        return "et";
      case LANG_FAEROESE:
        switch (sub)
          {
          case SUBLANG_FAEROESE_FAROE_ISLANDS: return "fo_FO";
          }
        return "fo";
      case LANG_FARSI:
        switch (sub)
          {
          case SUBLANG_FARSI_IRAN: return "fa_IR";
          }
        return "fa";
      case LANG_FINNISH:
        switch (sub)
          {
          case SUBLANG_FINNISH_FINLAND: return "fi_FI";
          }
        return "fi";
      case LANG_FRENCH:
        switch (sub)
          {
          case SUBLANG_FRENCH: return "fr_FR";
          case SUBLANG_FRENCH_BELGIAN: /* WALLOON */ return "fr_BE";
          case SUBLANG_FRENCH_CANADIAN: return "fr_CA";
          case SUBLANG_FRENCH_SWISS: return "fr_CH";
          case SUBLANG_FRENCH_LUXEMBOURG: return "fr_LU";
          case SUBLANG_FRENCH_MONACO: return "fr_MC";
          case SUBLANG_FRENCH_WESTINDIES: return "fr"; /* Caribbean? */
          case SUBLANG_FRENCH_REUNION: return "fr_RE";
          case SUBLANG_FRENCH_CONGO: return "fr_CG";
          case SUBLANG_FRENCH_SENEGAL: return "fr_SN";
          case SUBLANG_FRENCH_CAMEROON: return "fr_CM";
          case SUBLANG_FRENCH_COTEDIVOIRE: return "fr_CI";
          case SUBLANG_FRENCH_MALI: return "fr_ML";
          case SUBLANG_FRENCH_MOROCCO: return "fr_MA";
          case SUBLANG_FRENCH_HAITI: return "fr_HT";
          }
        return "fr";
      case LANG_FRISIAN:
        switch (sub)
          {
          case SUBLANG_FRISIAN_NETHERLANDS: return "fy_NL";
          }
        return "fy";
      case LANG_FULFULDE:
        /* Spoken in Nigeria, Guinea, Senegal, Mali, Niger, Cameroon, Benin.  */
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "ff_NG";
          }
        return "ff";
      case LANG_GAELIC:
        switch (sub)
          {
          case 0x01: /* SCOTTISH */
            /* old, superseded by LANG_SCOTTISH_GAELIC */
            return "gd_GB";
          case SUBLANG_IRISH_IRELAND: return "ga_IE";
          }
        return "ga";
      case LANG_GALICIAN:
        switch (sub)
          {
          case SUBLANG_GALICIAN_SPAIN: return "gl_ES";
          }
        return "gl";
      case LANG_GEORGIAN:
        switch (sub)
          {
          case SUBLANG_GEORGIAN_GEORGIA: return "ka_GE";
          }
        return "ka";
      case LANG_GERMAN:
        switch (sub)
          {
          case SUBLANG_GERMAN: return "de_DE";
          case SUBLANG_GERMAN_SWISS: return "de_CH";
          case SUBLANG_GERMAN_AUSTRIAN: return "de_AT";
          case SUBLANG_GERMAN_LUXEMBOURG: return "de_LU";
          case SUBLANG_GERMAN_LIECHTENSTEIN: return "de_LI";
          }
        return "de";
      case LANG_GREEK:
        switch (sub)
          {
          case SUBLANG_GREEK_GREECE: return "el_GR";
          }
        return "el";
      case LANG_GREENLANDIC:
        switch (sub)
          {
          case SUBLANG_GREENLANDIC_GREENLAND: return "kl_GL";
          }
        return "kl";
      case LANG_GUARANI:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "gn_PY";
          }
        return "gn";
      case LANG_GUJARATI:
        switch (sub)
          {
          case SUBLANG_GUJARATI_INDIA: return "gu_IN";
          }
        return "gu";
      case LANG_HAUSA:
        switch (sub)
          {
          case 0x1f: return "ha";
          case SUBLANG_HAUSA_NIGERIA_LATIN: return "ha_NG";
          }
        return "ha";
      case LANG_HAWAIIAN:
        /* FIXME: Do they mean Hawaiian ("haw_US", 1000 speakers)
           or Hawaii Creole English ("cpe_US", 600000 speakers)?  */
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "cpe_US";
          }
        return "cpe";
      case LANG_HEBREW:
        switch (sub)
          {
          case SUBLANG_HEBREW_ISRAEL: return "he_IL";
          }
        return "he";
      case LANG_HINDI:
        switch (sub)
          {
          case SUBLANG_HINDI_INDIA: return "hi_IN";
          }
        return "hi";
      case LANG_HUNGARIAN:
        switch (sub)
          {
          case SUBLANG_HUNGARIAN_HUNGARY: return "hu_HU";
          }
        return "hu";
      case LANG_IBIBIO:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "nic_NG";
          }
        return "nic";
      case LANG_ICELANDIC:
        switch (sub)
          {
          case SUBLANG_ICELANDIC_ICELAND: return "is_IS";
          }
        return "is";
      case LANG_IGBO:
        switch (sub)
          {
          case SUBLANG_IGBO_NIGERIA: return "ig_NG";
          }
        return "ig";
      case LANG_INDONESIAN:
        switch (sub)
          {
          case SUBLANG_INDONESIAN_INDONESIA: return "id_ID";
          }
        return "id";
      case LANG_INUKTITUT:
        switch (sub)
          {
          case 0x1e: return "iu"; /* syllabic */
          case SUBLANG_INUKTITUT_CANADA: return "iu_CA"; /* syllabic */
          case 0x1f: return "iu@latin";
          case SUBLANG_INUKTITUT_CANADA_LATIN: return "iu_CA@latin";
          }
        return "iu";
      case LANG_ITALIAN:
        switch (sub)
          {
          case SUBLANG_ITALIAN: return "it_IT";
          case SUBLANG_ITALIAN_SWISS: return "it_CH";
          }
        return "it";
      case LANG_JAPANESE:
        switch (sub)
          {
          case SUBLANG_JAPANESE_JAPAN: return "ja_JP";
          }
        return "ja";
      case LANG_KANNADA:
        switch (sub)
          {
          case SUBLANG_KANNADA_INDIA: return "kn_IN";
          }
        return "kn";
      case LANG_KANURI:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "kr_NG";
          }
        return "kr";
      case LANG_KASHMIRI:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "ks_PK";
          case SUBLANG_KASHMIRI_INDIA: return "ks_IN";
          }
        return "ks";
      case LANG_KAZAK:
        switch (sub)
          {
          case SUBLANG_KAZAK_KAZAKHSTAN: return "kk_KZ";
          }
        return "kk";
      case LANG_KICHE:
        /* FIXME: Adjust this when such locales appear on Unix.  */
        switch (sub)
          {
          case SUBLANG_KICHE_GUATEMALA: return "qut_GT";
          }
        return "qut";
      case LANG_KINYARWANDA:
        switch (sub)
          {
          case SUBLANG_KINYARWANDA_RWANDA: return "rw_RW";
          }
        return "rw";
      case LANG_KONKANI:
        /* FIXME: Adjust this when such locales appear on Unix.  */
        switch (sub)
          {
          case SUBLANG_KONKANI_INDIA: return "kok_IN";
          }
        return "kok";
      case LANG_KOREAN:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "ko_KR";
          }
        return "ko";
      case LANG_KYRGYZ:
        switch (sub)
          {
          case SUBLANG_KYRGYZ_KYRGYZSTAN: return "ky_KG";
          }
        return "ky";
      case LANG_LAO:
        switch (sub)
          {
          case SUBLANG_LAO_LAOS: return "lo_LA";
          }
        return "lo";
      case LANG_LATIN:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "la_VA";
          }
        return "la";
      case LANG_LATVIAN:
        switch (sub)
          {
          case SUBLANG_LATVIAN_LATVIA: return "lv_LV";
          }
        return "lv";
      case LANG_LITHUANIAN:
        switch (sub)
          {
          case SUBLANG_LITHUANIAN_LITHUANIA: return "lt_LT";
          }
        return "lt";
      case LANG_LUXEMBOURGISH:
        switch (sub)
          {
          case SUBLANG_LUXEMBOURGISH_LUXEMBOURG: return "lb_LU";
          }
        return "lb";
      case LANG_MACEDONIAN:
        switch (sub)
          {
          case SUBLANG_MACEDONIAN_MACEDONIA: return "mk_MK";
          }
        return "mk";
      case LANG_MALAY:
        switch (sub)
          {
          case SUBLANG_MALAY_MALAYSIA: return "ms_MY";
          case SUBLANG_MALAY_BRUNEI_DARUSSALAM: return "ms_BN";
          }
        return "ms";
      case LANG_MALAYALAM:
        switch (sub)
          {
          case SUBLANG_MALAYALAM_INDIA: return "ml_IN";
          }
        return "ml";
      case LANG_MALTESE:
        switch (sub)
          {
          case SUBLANG_MALTESE_MALTA: return "mt_MT";
          }
        return "mt";
      case LANG_MANIPURI:
        /* FIXME: Adjust this when such locales appear on Unix.  */
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "mni_IN";
          }
        return "mni";
      case LANG_MAORI:
        switch (sub)
          {
          case SUBLANG_MAORI_NEW_ZEALAND: return "mi_NZ";
          }
        return "mi";
      case LANG_MAPUDUNGUN:
        switch (sub)
          {
          case SUBLANG_MAPUDUNGUN_CHILE: return "arn_CL";
          }
        return "arn";
      case LANG_MARATHI:
        switch (sub)
          {
          case SUBLANG_MARATHI_INDIA: return "mr_IN";
          }
        return "mr";
      case LANG_MOHAWK:
        switch (sub)
          {
          case SUBLANG_MOHAWK_CANADA: return "moh_CA";
          }
        return "moh";
      case LANG_MONGOLIAN:
        switch (sub)
          {
          case SUBLANG_MONGOLIAN_CYRILLIC_MONGOLIA: case 0x1e: return "mn_MN";
          case SUBLANG_MONGOLIAN_PRC: case 0x1f: return "mn_CN";
          }
        return "mn"; /* Ambiguous: could be "mn_CN" or "mn_MN".  */
      case LANG_NEPALI:
        switch (sub)
          {
          case SUBLANG_NEPALI_NEPAL: return "ne_NP";
          case SUBLANG_NEPALI_INDIA: return "ne_IN";
          }
        return "ne";
      case LANG_NORWEGIAN:
        switch (sub)
          {
          case 0x1f: return "nb";
          case SUBLANG_NORWEGIAN_BOKMAL: return "nb_NO";
          case 0x1e: return "nn";
          case SUBLANG_NORWEGIAN_NYNORSK: return "nn_NO";
          }
        return "no";
      case LANG_OCCITAN:
        switch (sub)
          {
          case SUBLANG_OCCITAN_FRANCE: return "oc_FR";
          }
        return "oc";
      case LANG_ORIYA:
        switch (sub)
          {
          case SUBLANG_ORIYA_INDIA: return "or_IN";
          }
        return "or";
      case LANG_OROMO:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "om_ET";
          }
        return "om";
      case LANG_PAPIAMENTU:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "pap_AN";
          }
        return "pap";
      case LANG_PASHTO:
        switch (sub)
          {
          case SUBLANG_PASHTO_AFGHANISTAN: return "ps_AF";
          }
        return "ps"; /* Ambiguous: could be "ps_PK" or "ps_AF".  */
      case LANG_POLISH:
        switch (sub)
          {
          case SUBLANG_POLISH_POLAND: return "pl_PL";
          }
        return "pl";
      case LANG_PORTUGUESE:
        switch (sub)
          {
          /* Hmm. SUBLANG_PORTUGUESE_BRAZILIAN == SUBLANG_DEFAULT.
             Same phenomenon as SUBLANG_ENGLISH_US == SUBLANG_DEFAULT. */
          case SUBLANG_PORTUGUESE_BRAZILIAN: return "pt_BR";
          case SUBLANG_PORTUGUESE: return "pt_PT";
          }
        return "pt";
      case LANG_PUNJABI:
        switch (sub)
          {
          case SUBLANG_PUNJABI_INDIA: return "pa_IN"; /* Gurmukhi script */
          case SUBLANG_PUNJABI_PAKISTAN: return "pa_PK"; /* Arabic script */
          }
        return "pa";
      case LANG_QUECHUA:
        /* Note: Microsoft uses the non-ISO language code "quz".  */
        switch (sub)
          {
          case SUBLANG_QUECHUA_BOLIVIA: return "qu_BO";
          case SUBLANG_QUECHUA_ECUADOR: return "qu_EC";
          case SUBLANG_QUECHUA_PERU: return "qu_PE";
          }
        return "qu";
      case LANG_ROMANIAN:
        switch (sub)
          {
          case SUBLANG_ROMANIAN_ROMANIA: return "ro_RO";
          case SUBLANG_ROMANIAN_MOLDOVA: return "ro_MD";
          }
        return "ro";
      case LANG_ROMANSH:
        switch (sub)
          {
          case SUBLANG_ROMANSH_SWITZERLAND: return "rm_CH";
          }
        return "rm";
      case LANG_RUSSIAN:
        switch (sub)
          {
          case SUBLANG_RUSSIAN_RUSSIA: return "ru_RU";
          case SUBLANG_RUSSIAN_MOLDAVIA: return "ru_MD";
          }
        return "ru"; /* Ambiguous: could be "ru_RU" or "ru_UA" or "ru_MD".  */
      case LANG_SAMI:
        switch (sub)
          {
          /* Northern Sami */
          case 0x00: return "se";
          case SUBLANG_SAMI_NORTHERN_NORWAY: return "se_NO";
          case SUBLANG_SAMI_NORTHERN_SWEDEN: return "se_SE";
          case SUBLANG_SAMI_NORTHERN_FINLAND: return "se_FI";
          /* Lule Sami */
          case 0x1f: return "smj";
          case SUBLANG_SAMI_LULE_NORWAY: return "smj_NO";
          case SUBLANG_SAMI_LULE_SWEDEN: return "smj_SE";
          /* Southern Sami */
          case 0x1e: return "sma";
          case SUBLANG_SAMI_SOUTHERN_NORWAY: return "sma_NO";
          case SUBLANG_SAMI_SOUTHERN_SWEDEN: return "sma_SE";
          /* Skolt Sami */
          case 0x1d: return "sms";
          case SUBLANG_SAMI_SKOLT_FINLAND: return "sms_FI";
          /* Inari Sami */
          case 0x1c: return "smn";
          case SUBLANG_SAMI_INARI_FINLAND: return "smn_FI";
          }
        return "se"; /* or "smi"? */
      case LANG_SANSKRIT:
        switch (sub)
          {
          case SUBLANG_SANSKRIT_INDIA: return "sa_IN";
          }
        return "sa";
      case LANG_SCOTTISH_GAELIC:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "gd_GB";
          }
        return "gd";
      case LANG_SINDHI:
        switch (sub)
          {
          case SUBLANG_SINDHI_INDIA: return "sd_IN";
          case SUBLANG_SINDHI_PAKISTAN: return "sd_PK";
          /*case SUBLANG_SINDHI_AFGHANISTAN: return "sd_AF";*/
          }
        return "sd";
      case LANG_SINHALESE:
        switch (sub)
          {
          case SUBLANG_SINHALESE_SRI_LANKA: return "si_LK";
          }
        return "si";
      case LANG_SLOVAK:
        switch (sub)
          {
          case SUBLANG_SLOVAK_SLOVAKIA: return "sk_SK";
          }
        return "sk";
      case LANG_SLOVENIAN:
        switch (sub)
          {
          case SUBLANG_SLOVENIAN_SLOVENIA: return "sl_SI";
          }
        return "sl";
      case LANG_SOMALI:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "so_SO";
          }
        return "so";
      case LANG_SORBIAN:
        /* FIXME: Adjust this when such locales appear on Unix.  */
        switch (sub)
          {
          /* Upper Sorbian */
          case 0x00: return "hsb";
          case SUBLANG_UPPER_SORBIAN_GERMANY: return "hsb_DE";
          /* Lower Sorbian */
          case 0x1f: return "dsb";
          case SUBLANG_LOWER_SORBIAN_GERMANY: return "dsb_DE";
          }
        return "wen";
      case LANG_SOTHO:
        /* <http://www.microsoft.com/globaldev/reference/lcid-all.mspx> calls
           it "Sepedi"; according to
           <http://www.ethnologue.com/show_language.asp?code=nso>
           <http://www.ethnologue.com/show_language.asp?code=sot>
           it's the same as Northern Sotho.  */
        switch (sub)
          {
          case SUBLANG_SOTHO_SOUTH_AFRICA: return "nso_ZA";
          }
        return "nso";
      case LANG_SPANISH:
        switch (sub)
          {
          case SUBLANG_SPANISH: return "es_ES";
          case SUBLANG_SPANISH_MEXICAN: return "es_MX";
          case SUBLANG_SPANISH_MODERN:
            return "es_ES@modern";      /* not seen on Unix */
          case SUBLANG_SPANISH_GUATEMALA: return "es_GT";
          case SUBLANG_SPANISH_COSTA_RICA: return "es_CR";
          case SUBLANG_SPANISH_PANAMA: return "es_PA";
          case SUBLANG_SPANISH_DOMINICAN_REPUBLIC: return "es_DO";
          case SUBLANG_SPANISH_VENEZUELA: return "es_VE";
          case SUBLANG_SPANISH_COLOMBIA: return "es_CO";
          case SUBLANG_SPANISH_PERU: return "es_PE";
          case SUBLANG_SPANISH_ARGENTINA: return "es_AR";
          case SUBLANG_SPANISH_ECUADOR: return "es_EC";
          case SUBLANG_SPANISH_CHILE: return "es_CL";
          case SUBLANG_SPANISH_URUGUAY: return "es_UY";
          case SUBLANG_SPANISH_PARAGUAY: return "es_PY";
          case SUBLANG_SPANISH_BOLIVIA: return "es_BO";
          case SUBLANG_SPANISH_EL_SALVADOR: return "es_SV";
          case SUBLANG_SPANISH_HONDURAS: return "es_HN";
          case SUBLANG_SPANISH_NICARAGUA: return "es_NI";
          case SUBLANG_SPANISH_PUERTO_RICO: return "es_PR";
          case SUBLANG_SPANISH_US: return "es_US";
          }
        return "es";
      case LANG_SUTU:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "bnt_TZ"; /* or "st_LS" or "nso_ZA"? */
          }
        return "bnt";
      case LANG_SWAHILI:
        switch (sub)
          {
          case SUBLANG_SWAHILI_KENYA: return "sw_KE";
          }
        return "sw";
      case LANG_SWEDISH:
        switch (sub)
          {
          case SUBLANG_SWEDISH_SWEDEN: return "sv_SE";
          case SUBLANG_SWEDISH_FINLAND: return "sv_FI";
          }
        return "sv";
      case LANG_SYRIAC:
        switch (sub)
          {
          case SUBLANG_SYRIAC_SYRIA: return "syr_SY"; /* An extinct language.  */
          }
        return "syr";
      case LANG_TAGALOG:
        switch (sub)
          {
          case SUBLANG_TAGALOG_PHILIPPINES: return "tl_PH"; /* or "fil_PH"? */
          }
        return "tl"; /* or "fil"? */
      case LANG_TAJIK:
        switch (sub)
          {
          case 0x1f: return "tg";
          case SUBLANG_TAJIK_TAJIKISTAN: return "tg_TJ";
          }
        return "tg";
      case LANG_TAMAZIGHT:
        /* Note: Microsoft uses the non-ISO language code "tmz".  */
        switch (sub)
          {
          /* FIXME: Adjust this when Tamazight locales appear on Unix.  */
          case SUBLANG_TAMAZIGHT_ARABIC: return "ber_MA@arabic";
          case 0x1f: return "ber@latin";
          case SUBLANG_TAMAZIGHT_ALGERIA_LATIN: return "ber_DZ@latin";
          }
        return "ber";
      case LANG_TAMIL:
        switch (sub)
          {
          case SUBLANG_TAMIL_INDIA: return "ta_IN";
          }
        return "ta"; /* Ambiguous: could be "ta_IN" or "ta_LK" or "ta_SG".  */
      case LANG_TATAR:
        switch (sub)
          {
          case SUBLANG_TATAR_RUSSIA: return "tt_RU";
          }
        return "tt";
      case LANG_TELUGU:
        switch (sub)
          {
          case SUBLANG_TELUGU_INDIA: return "te_IN";
          }
        return "te";
      case LANG_THAI:
        switch (sub)
          {
          case SUBLANG_THAI_THAILAND: return "th_TH";
          }
        return "th";
      case LANG_TIBETAN:
        switch (sub)
          {
          case SUBLANG_TIBETAN_PRC:
            /* Most Tibetans would not like "bo_CN".  But Tibet does not yet
               have a country code of its own.  */
            return "bo";
          case SUBLANG_TIBETAN_BHUTAN: return "bo_BT";
          }
        return "bo";
      case LANG_TIGRINYA:
        switch (sub)
          {
          case SUBLANG_TIGRINYA_ETHIOPIA: return "ti_ET";
          case SUBLANG_TIGRINYA_ERITREA: return "ti_ER";
          }
        return "ti";
      case LANG_TSONGA:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "ts_ZA";
          }
        return "ts";
      case LANG_TSWANA:
        /* Spoken in South Africa, Botswana.  */
        switch (sub)
          {
          case SUBLANG_TSWANA_SOUTH_AFRICA: return "tn_ZA";
          }
        return "tn";
      case LANG_TURKISH:
        switch (sub)
          {
          case SUBLANG_TURKISH_TURKEY: return "tr_TR";
          }
        return "tr";
      case LANG_TURKMEN:
        switch (sub)
          {
          case SUBLANG_TURKMEN_TURKMENISTAN: return "tk_TM";
          }
        return "tk";
      case LANG_UIGHUR:
        switch (sub)
          {
          case SUBLANG_UIGHUR_PRC: return "ug_CN";
          }
        return "ug";
      case LANG_UKRAINIAN:
        switch (sub)
          {
          case SUBLANG_UKRAINIAN_UKRAINE: return "uk_UA";
          }
        return "uk";
      case LANG_URDU:
        switch (sub)
          {
          case SUBLANG_URDU_PAKISTAN: return "ur_PK";
          case SUBLANG_URDU_INDIA: return "ur_IN";
          }
        return "ur";
      case LANG_UZBEK:
        switch (sub)
          {
          case 0x1f: return "uz";
          case SUBLANG_UZBEK_LATIN: return "uz_UZ";
          case 0x1e: return "uz@cyrillic";
          case SUBLANG_UZBEK_CYRILLIC: return "uz_UZ@cyrillic";
          }
        return "uz";
      case LANG_VENDA:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "ve_ZA";
          }
        return "ve";
      case LANG_VIETNAMESE:
        switch (sub)
          {
          case SUBLANG_VIETNAMESE_VIETNAM: return "vi_VN";
          }
        return "vi";
      case LANG_WELSH:
        switch (sub)
          {
          case SUBLANG_WELSH_UNITED_KINGDOM: return "cy_GB";
          }
        return "cy";
      case LANG_WOLOF:
        switch (sub)
          {
          case SUBLANG_WOLOF_SENEGAL: return "wo_SN";
          }
        return "wo";
      case LANG_XHOSA:
        switch (sub)
          {
          case SUBLANG_XHOSA_SOUTH_AFRICA: return "xh_ZA";
          }
        return "xh";
      case LANG_YAKUT:
        switch (sub)
          {
          case SUBLANG_YAKUT_RUSSIA: return "sah_RU";
          }
        return "sah";
      case LANG_YI:
        switch (sub)
          {
          case SUBLANG_YI_PRC: return "ii_CN";
          }
        return "ii";
      case LANG_YIDDISH:
        switch (sub)
          {
          case SUBLANG_DEFAULT: return "yi_IL";
          }
        return "yi";
      case LANG_YORUBA:
        switch (sub)
          {
          case SUBLANG_YORUBA_NIGERIA: return "yo_NG";
          }
        return "yo";
      case LANG_ZULU:
        switch (sub)
          {
          case SUBLANG_ZULU_SOUTH_AFRICA: return "zu_ZA";
          }
        return "zu";
      default: return "C";
      }
  }
}

# if !defined IN_LIBINTL
static
# endif
const char *
gl_locale_name_from_win32_LCID (LCID lcid)
{
  LANGID langid;

  /* Strip off the sorting rules, keep only the language part.  */
  langid = LANGIDFROMLCID (lcid);

  return gl_locale_name_from_win32_LANGID (langid);
}

# ifdef WINDOWS_NATIVE

/* Two variables to interface between get_lcid and the EnumLocales
   callback function below.  */
static LCID found_lcid;
static char lname[LC_MAX * (LOCALE_NAME_MAX_LENGTH + 1) + 1];

/* Callback function for EnumLocales.  */
static BOOL CALLBACK
enum_locales_fn (LPTSTR locale_num_str)
{
  char *endp;
  char locval[2 * LOCALE_NAME_MAX_LENGTH + 1 + 1];
  LCID try_lcid = strtoul (locale_num_str, &endp, 16);

  if (GetLocaleInfo (try_lcid, LOCALE_SENGLANGUAGE,
                    locval, LOCALE_NAME_MAX_LENGTH))
    {
      strcat (locval, "_");
      if (GetLocaleInfo (try_lcid, LOCALE_SENGCOUNTRY,
                        locval + strlen (locval), LOCALE_NAME_MAX_LENGTH))
       {
         size_t locval_len = strlen (locval);

         if (strncmp (locval, lname, locval_len) == 0
             && (lname[locval_len] == '.'
                 || lname[locval_len] == '\0'))
           {
             found_lcid = try_lcid;
             return FALSE;
           }
       }
    }
  return TRUE;
}

/* This lock protects the get_lcid against multiple simultaneous calls.  */
gl_lock_define_initialized(static, get_lcid_lock)

/* Return the Locale ID (LCID) number given the locale's name, a
   string, in LOCALE_NAME.  This works by enumerating all the locales
   supported by the system, until we find one whose name matches
   LOCALE_NAME.  */
static LCID
get_lcid (const char *locale_name)
{
  /* A simple cache.  */
  static LCID last_lcid;
  static char last_locale[1000];

  /* Lock while looking for an LCID, to protect access to static
     variables: last_lcid, last_locale, found_lcid, and lname.  */
  gl_lock_lock (get_lcid_lock);
  if (last_lcid > 0 && strcmp (locale_name, last_locale) == 0)
    {
      gl_lock_unlock (get_lcid_lock);
      return last_lcid;
    }
  strncpy (lname, locale_name, sizeof (lname) - 1);
  lname[sizeof (lname) - 1] = '\0';
  found_lcid = 0;
  EnumSystemLocales (enum_locales_fn, LCID_SUPPORTED);
  if (found_lcid > 0)
    {
      last_lcid = found_lcid;
      strcpy (last_locale, locale_name);
    }
  gl_lock_unlock (get_lcid_lock);
  return found_lcid;
}

# endif
#endif


#if HAVE_USELOCALE /* glibc or Mac OS X */

/* Simple hash set of strings.  We don't want to drag in lots of hash table
   code here.  */

# define SIZE_BITS (sizeof (size_t) * CHAR_BIT)

/* A hash function for NUL-terminated char* strings using
   the method described by Bruno Haible.
   See http://www.haible.de/bruno/hashfunc.html.  */
static size_t _GL_ATTRIBUTE_PURE
string_hash (const void *x)
{
  const char *s = (const char *) x;
  size_t h = 0;

  for (; *s; s++)
    h = *s + ((h << 9) | (h >> (SIZE_BITS - 9)));

  return h;
}

/* A hash table of fixed size.  Multiple threads can access it read-only
   simultaneously, but only one thread can insert into it at the same time.  */

/* A node in a hash bucket collision list.  */
struct hash_node
  {
    struct hash_node * volatile next;
    char contents[100]; /* has variable size */
  };

# define HASH_TABLE_SIZE 257
static struct hash_node * volatile struniq_hash_table[HASH_TABLE_SIZE]
  /* = { NULL, ..., NULL } */;

/* This lock protects the struniq_hash_table against multiple simultaneous
   insertions.  */
gl_lock_define_initialized(static, struniq_lock)

/* Store a copy of the given string in a string pool with indefinite extent.
   Return a pointer to this copy.  */
static const char *
struniq (const char *string)
{
  size_t hashcode = string_hash (string);
  size_t slot = hashcode % HASH_TABLE_SIZE;
  size_t size;
  struct hash_node *new_node;
  struct hash_node *p;
  for (p = struniq_hash_table[slot]; p != NULL; p = p->next)
    if (strcmp (p->contents, string) == 0)
      return p->contents;
  size = strlen (string) + 1;
  new_node =
    (struct hash_node *)
    malloc (offsetof (struct hash_node, contents[0]) + size);
  if (new_node == NULL)
    /* Out of memory.  Return a statically allocated string.  */
    return "C";
  memcpy (new_node->contents, string, size);
  /* Lock while inserting new_node.  */
  gl_lock_lock (struniq_lock);
  /* Check whether another thread already added the string while we were
     waiting on the lock.  */
  for (p = struniq_hash_table[slot]; p != NULL; p = p->next)
    if (strcmp (p->contents, string) == 0)
      {
        free (new_node);
        new_node = p;
        goto done;
      }
  /* Really insert new_node into the hash table.  Fill new_node entirely first,
     because other threads may be iterating over the linked list.  */
  new_node->next = struniq_hash_table[slot];
  struniq_hash_table[slot] = new_node;
 done:
  /* Unlock after new_node is inserted.  */
  gl_lock_unlock (struniq_lock);
  return new_node->contents;
}

#endif


#if defined IN_LIBINTL || HAVE_USELOCALE

/* Like gl_locale_name_thread, except that the result is not in storage of
   indefinite extent.  */
# if !defined IN_LIBINTL
static
# endif
const char *
gl_locale_name_thread_unsafe (int category, const char *categoryname)
{
# if HAVE_USELOCALE
  {
    locale_t thread_locale = uselocale (NULL);
    if (thread_locale != LC_GLOBAL_LOCALE)
      {
#  if __GLIBC__ >= 2 && !defined __UCLIBC__
        /* Work around an incorrect definition of the _NL_LOCALE_NAME macro in
           glibc < 2.12.
           See <http://sourceware.org/bugzilla/show_bug.cgi?id=10968>.  */
        const char *name =
          nl_langinfo (_NL_ITEM ((category), _NL_ITEM_INDEX (-1)));
        if (name[0] == '\0')
          /* Fallback code for glibc < 2.4, which did not implement
             nl_langinfo (_NL_LOCALE_NAME (category)).  */
          name = thread_locale->__names[category];
        return name;
#  elif defined __FreeBSD__ || (defined __APPLE__ && defined __MACH__)
        /* FreeBSD, Mac OS X */
        int mask;

        switch (category)
          {
          case LC_CTYPE:
            mask = LC_CTYPE_MASK;
            break;
          case LC_NUMERIC:
            mask = LC_NUMERIC_MASK;
            break;
          case LC_TIME:
            mask = LC_TIME_MASK;
            break;
          case LC_COLLATE:
            mask = LC_COLLATE_MASK;
            break;
          case LC_MONETARY:
            mask = LC_MONETARY_MASK;
            break;
          case LC_MESSAGES:
            mask = LC_MESSAGES_MASK;
            break;
          default: /* We shouldn't get here.  */
            return "";
          }
        return querylocale (mask, thread_locale);
#  endif
      }
  }
# endif
  return NULL;
}

#endif

const char *
gl_locale_name_thread (int category, const char *categoryname)
{
#if HAVE_USELOCALE
  const char *name = gl_locale_name_thread_unsafe (category, categoryname);
  if (name != NULL)
    return struniq (name);
#elif defined WINDOWS_NATIVE
  if (LC_MIN <= category && category <= LC_MAX)
    {
      char *locname = setlocale (category, NULL);
      LCID lcid = 0;

      /* If CATEGORY is LC_ALL, the result might be a semi-colon
        separated list of locales.  We need only one, so we take the
        one corresponding to LC_CTYPE, as the most important for
        character translations.  */
      if (strchr (locname, ';'))
       locname = setlocale (LC_CTYPE, NULL);

      /* Convert locale name to LCID.  We don't want to use
         LocaleNameToLCID because (a) it is only available since Vista,
         and (b) it doesn't accept locale names returned by 'setlocale'.  */
      lcid = get_lcid (locname);

      if (lcid > 0)
        return gl_locale_name_from_win32_LCID (lcid);
    }
#endif
  return NULL;
}

/* XPG3 defines the result of 'setlocale (category, NULL)' as:
   "Directs 'setlocale()' to query 'category' and return the current
    setting of 'local'."
   However it does not specify the exact format.  Neither do SUSV2 and
   ISO C 99.  So we can use this feature only on selected systems (e.g.
   those using GNU C Library).  */
#if defined _LIBC || ((defined __GLIBC__ && __GLIBC__ >= 2) && !defined __UCLIBC__)
# define HAVE_LOCALE_NULL
#endif

const char *
gl_locale_name_posix (int category, const char *categoryname)
{
  /* Use the POSIX methods of looking to 'LC_ALL', 'LC_xxx', and 'LANG'.
     On some systems this can be done by the 'setlocale' function itself.  */
#if defined HAVE_SETLOCALE && defined HAVE_LC_MESSAGES && defined HAVE_LOCALE_NULL
  return setlocale (category, NULL);
#else
  /* On other systems we ignore what setlocale reports and instead look at the
     environment variables directly.  This is necessary
       1. on systems which have a facility for customizing the default locale
          (Mac OS X, native Windows, Cygwin) and where the system's setlocale()
          function ignores this default locale (Mac OS X, Cygwin), in two cases:
          a. when the user missed to use the setlocale() override from libintl
             (for example by not including <libintl.h>),
          b. when setlocale supports only the "C" locale, such as on Cygwin
             1.5.x.  In this case even the override from libintl cannot help.
       2. on all systems where setlocale supports only the "C" locale.  */
  /* Strictly speaking, it is a POSIX violation to look at the environment
     variables regardless whether setlocale has been called or not.  POSIX
     says:
         "For C-language programs, the POSIX locale shall be the
          default locale when the setlocale() function is not called."
     But we assume that all programs that use internationalized APIs call
     setlocale (LC_ALL, "").  */
  return gl_locale_name_environ (category, categoryname);
#endif
}

const char *
gl_locale_name_environ (int category, const char *categoryname)
{
  const char *retval;

  /* Setting of LC_ALL overrides all other.  */
  retval = getenv ("LC_ALL");
  if (retval != NULL && retval[0] != '\0')
    return retval;
  /* Next comes the name of the desired category.  */
  retval = getenv (categoryname);
  if (retval != NULL && retval[0] != '\0')
    return retval;
  /* Last possibility is the LANG environment variable.  */
  retval = getenv ("LANG");
  if (retval != NULL && retval[0] != '\0')
    {
#if HAVE_CFLOCALECOPYCURRENT || HAVE_CFPREFERENCESCOPYAPPVALUE
      /* Mac OS X 10.2 or newer.
         Ignore invalid LANG value set by the Terminal application.  */
      if (strcmp (retval, "UTF-8") != 0)
#endif
#if defined __CYGWIN__
      /* Cygwin.
         Ignore dummy LANG value set by ~/.profile.  */
      if (strcmp (retval, "C.UTF-8") != 0)
#endif
        return retval;
    }

  return NULL;
}

const char *
gl_locale_name_default (void)
{
  /* POSIX:2001 says:
     "All implementations shall define a locale as the default locale, to be
      invoked when no environment variables are set, or set to the empty
      string.  This default locale can be the POSIX locale or any other
      implementation-defined locale.  Some implementations may provide
      facilities for local installation administrators to set the default
      locale, customizing it for each location.  POSIX:2001 does not require
      such a facility.

     The systems with such a facility are Mac OS X and Windows: They provide a
     GUI that allows the user to choose a locale.
       - On Mac OS X, by default, none of LC_* or LANG are set.  Starting with
         Mac OS X 10.4 or 10.5, LANG is set for processes launched by the
         'Terminal' application (but sometimes to an incorrect value "UTF-8").
         When no environment variable is set, setlocale (LC_ALL, "") uses the
         "C" locale.
       - On native Windows, by default, none of LC_* or LANG are set.
         When no environment variable is set, setlocale (LC_ALL, "") uses the
         locale chosen by the user.
       - On Cygwin 1.5.x, by default, none of LC_* or LANG are set.
         When no environment variable is set, setlocale (LC_ALL, "") uses the
         "C" locale.
       - On Cygwin 1.7, by default, LANG is set to "C.UTF-8" when the default
         ~/.profile is executed.
         When no environment variable is set, setlocale (LC_ALL, "") uses the
         "C.UTF-8" locale, which operates in the same way as the "C" locale.
  */

#if !(HAVE_CFLOCALECOPYCURRENT || HAVE_CFPREFERENCESCOPYAPPVALUE || defined WINDOWS_NATIVE || defined __CYGWIN__)

  /* The system does not have a way of setting the locale, other than the
     POSIX specified environment variables.  We use C as default locale.  */
  return "C";

#else

  /* Return an XPG style locale name language[_territory][@modifier].
     Don't even bother determining the codeset; it's not useful in this
     context, because message catalogs are not specific to a single
     codeset.  */

# if HAVE_CFLOCALECOPYCURRENT || HAVE_CFPREFERENCESCOPYAPPVALUE
  /* Mac OS X 10.2 or newer */
  {
    /* Cache the locale name, since CoreFoundation calls are expensive.  */
    static const char *cached_localename;

    if (cached_localename == NULL)
      {
        char namebuf[256];
#  if HAVE_CFLOCALECOPYCURRENT /* Mac OS X 10.3 or newer */
        CFLocaleRef locale = CFLocaleCopyCurrent ();
        CFStringRef name = CFLocaleGetIdentifier (locale);

        if (CFStringGetCString (name, namebuf, sizeof (namebuf),
                                kCFStringEncodingASCII))
          {
            gl_locale_name_canonicalize (namebuf);
            cached_localename = strdup (namebuf);
          }
        CFRelease (locale);
#  elif HAVE_CFPREFERENCESCOPYAPPVALUE /* Mac OS X 10.2 or newer */
        CFTypeRef value =
          CFPreferencesCopyAppValue (CFSTR ("AppleLocale"),
                                     kCFPreferencesCurrentApplication);
        if (value != NULL
            && CFGetTypeID (value) == CFStringGetTypeID ()
            && CFStringGetCString ((CFStringRef)value,
                                   namebuf, sizeof (namebuf),
                                   kCFStringEncodingASCII))
          {
            gl_locale_name_canonicalize (namebuf);
            cached_localename = strdup (namebuf);
          }
#  endif
        if (cached_localename == NULL)
          cached_localename = "C";
      }
    return cached_localename;
  }

# endif

# if defined WINDOWS_NATIVE || defined __CYGWIN__ /* Native Windows or Cygwin */
  {
    LCID lcid;

    /* Use native Windows API locale ID.  */
    lcid = GetThreadLocale ();

    return gl_locale_name_from_win32_LCID (lcid);
  }
# endif
#endif
}

/* Determine the current locale's name, and canonicalize it into XPG syntax
     language[_territory][.codeset][@modifier]
   The codeset part in the result is not reliable; the locale_charset()
   should be used for codeset information instead.
   The result must not be freed; it is statically allocated.  */

const char *
gl_locale_name (int category, const char *categoryname)
{
  const char *retval;

  retval = gl_locale_name_thread (category, categoryname);
  if (retval != NULL)
    return retval;

  retval = gl_locale_name_posix (category, categoryname);
  if (retval != NULL)
    return retval;

  return gl_locale_name_default ();
}
