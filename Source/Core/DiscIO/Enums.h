// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include "Common/CommonTypes.h"

namespace DiscIO
{
// Increment CACHE_REVISION (GameFileCache.cpp) if these enums are modified

enum class Platform
{
  GameCubeDisc = 0,
  WiiDisc,
  WiiWAD,
  ELFOrDOL,
  NumberOfPlatforms
};

enum class Country
{
  Europe = 0,
  Japan,
  USA,
  Australia,
  France,
  Germany,
  Italy,
  Korea,
  Netherlands,
  Russia,
  Spain,
  Taiwan,
  World,
  Unknown,
  NumberOfCountries
};

// This numbering matches Nintendo's GameCube/Wii region numbering.
enum class Region
{
  NTSC_J = 0,   // Japan and Taiwan (and South Korea for GameCube only)
  NTSC_U = 1,   // Mainly North America
  PAL = 2,      // Mainly Europe and Oceania
  Unknown = 3,  // Nintendo uses this to mean region free, but we also use it for unknown regions
  NTSC_K = 4    // South Korea (Wii only)
};

// Languages 0 - 9 match Nintendo's Wii language numbering.
// Languages 1 - 6 match Nintendo's PAL GameCube languages 0 - 5.
// NTSC GameCubes only support one language and thus don't number languages.
enum class Language
{
  Japanese = 0,
  English = 1,
  German = 2,
  French = 3,
  Spanish = 4,
  Italian = 5,
  Dutch = 6,
  SimplifiedChinese = 7,   // Not selectable on any unmodded retail Wii
  TraditionalChinese = 8,  // Not selectable on any unmodded retail Wii
  Korean = 9,
  Unknown
};

std::string GetName(Country country, bool translate);
std::string GetName(Language language, bool translate);

bool IsDisc(Platform volume_type);
bool IsWii(Platform volume_type);
bool IsNTSC(Region region);

int ToGameCubeLanguage(Language language);
Language FromGameCubeLanguage(int language);

Country TypicalCountryForRegion(Region region);
Region SysConfCountryToRegion(u8 country_code);
// Avoid using this function if you can. Country codes aren't always reliable region indicators.
Region CountryCodeToRegion(u8 country_code, Platform platform,
                           Region expected_region = Region::Unknown,
                           std::optional<u16> revision = {});
Country CountryCodeToCountry(u8 country_code, Platform platform, Region region = Region::Unknown,
                             std::optional<u16> revision = {});

Region GetSysMenuRegion(u16 title_version);
std::string GetSysMenuVersionString(u16 title_version, bool is_vwii);

const std::string& GetCompanyFromID(const std::string& company_id);
}  // namespace DiscIO
