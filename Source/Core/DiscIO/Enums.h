// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/CommonTypes.h"

namespace DiscIO
{
// Increment CACHE_REVISION (ISOFile.cpp & GameFile.cpp) if these enums are modified

enum class Platform
{
  GAMECUBE_DISC = 0,
  WII_DISC,
  WII_WAD,
  ELF_DOL,
  NUMBER_OF_PLATFORMS
};

enum class Country
{
  COUNTRY_EUROPE = 0,
  COUNTRY_JAPAN,
  COUNTRY_USA,
  COUNTRY_AUSTRALIA,
  COUNTRY_FRANCE,
  COUNTRY_GERMANY,
  COUNTRY_ITALY,
  COUNTRY_KOREA,
  COUNTRY_NETHERLANDS,
  COUNTRY_RUSSIA,
  COUNTRY_SPAIN,
  COUNTRY_TAIWAN,
  COUNTRY_WORLD,
  COUNTRY_UNKNOWN,
  NUMBER_OF_COUNTRIES
};

// Languages 0 - 9 match Nintendo's Wii language numbering.
// Languages 1 - 6 match Nintendo's PAL GameCube languages 0 - 5.
// NTSC GameCubes only support one language and thus don't number languages.
enum class Language
{
  LANGUAGE_JAPANESE = 0,
  LANGUAGE_ENGLISH = 1,
  LANGUAGE_GERMAN = 2,
  LANGUAGE_FRENCH = 3,
  LANGUAGE_SPANISH = 4,
  LANGUAGE_ITALIAN = 5,
  LANGUAGE_DUTCH = 6,
  LANGUAGE_SIMPLIFIED_CHINESE = 7,
  LANGUAGE_TRADITIONAL_CHINESE = 8,
  LANGUAGE_KOREAN = 9,
  LANGUAGE_UNKNOWN
};

Country CountrySwitch(u8 country_code);
u8 GetSysMenuRegion(u16 title_version);
std::string GetCompanyFromID(const std::string& company_id);
}
