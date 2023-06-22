// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/DefaultLocale.h"

#include <algorithm>
#include <array>
#include <locale>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Core/Host.h"
#include "DiscIO/Enums.h"

namespace Config
{
static std::vector<std::string> GetPreferredLocales()
{
  static const std::vector<std::string> locales = Host_GetPreferredLocales();
  return locales;
}

static std::optional<DiscIO::Language> TryParseLanguage(const std::string& locale)
{
  const std::vector<std::string> split_locale = SplitString(locale, '-');
  if (split_locale.empty())
    return std::nullopt;

  // Special handling of Chinese due to its two writing systems
  if (split_locale[0] == "zh")
  {
    const auto locale_contains = [&split_locale](std::string_view str) {
      return std::find(split_locale.cbegin(), split_locale.cend(), str) != split_locale.cend();
    };

    if (locale_contains("Hans"))
      return DiscIO::Language::SimplifiedChinese;
    if (locale_contains("Hant"))
      return DiscIO::Language::TraditionalChinese;

    // Mainland China and Singapore use simplified characters
    if (locale_contains("CN") || locale_contains("SG"))
      return DiscIO::Language::SimplifiedChinese;
    else
      return DiscIO::Language::TraditionalChinese;
  }

  // Same order as in Wii SYSCONF
  constexpr std::array<std::string_view, 10> LANGUAGES = {
      "ja", "en", "de", "fr", "es", "it", "nl", "zh", "zh", "ko",
  };

  const auto it = std::find(LANGUAGES.cbegin(), LANGUAGES.cend(), split_locale[0]);
  if (it == LANGUAGES.cend())
    return std::nullopt;

  return static_cast<DiscIO::Language>(it - LANGUAGES.cbegin());
}

static DiscIO::Language ComputeDefaultLanguage()
{
  for (const std::string& locale : GetPreferredLocales())
  {
    if (const std::optional<DiscIO::Language> language = TryParseLanguage(locale))
      return *language;
  }

  return DiscIO::Language::English;
}

static std::optional<std::string> TryParseCountryCode(const std::string& locale)
{
  const auto is_upper = [](char c) { return std::isupper(c, std::locale::classic()); };

  for (const std::string& part : SplitString(locale, '-'))
  {
    if (part.size() == 2 && is_upper(part[0]) && is_upper(part[1]))
      return part;
  }

  return std::nullopt;
}

static std::string ComputeDefaultCountryCode()
{
#ifdef _WIN32
  // Windows codepath: Check the regional information.
  // More likely to match the user's physical location than locales are.
  // TODO: Can we use GetUserDefaultGeoName? (It was added in a Windows 10 update)
  GEOID geo = GetUserGeoID(GEOCLASS_NATION);
  const int buffer_size = GetGeoInfoW(geo, GEO_ISO2, nullptr, 0, 0);
  std::vector<wchar_t> buffer(buffer_size);
  const int result = GetGeoInfoW(geo, GEO_ISO2, buffer.data(), buffer_size, 0);
  if (result != 0)
    return TStrToUTF8(buffer.data());
#endif

  // Generic codepath: Check the locales.
  // Might be entirely unrelated to someone's actual country (for instance someone in a
  // non-English-speaking country using en-US), but could also be a perfect match.
  for (const std::string& locale : GetPreferredLocales())
  {
    if (const std::optional<std::string> country_code = TryParseCountryCode(locale))
      return *country_code;
  }

  return "";
}

static std::optional<u8> ComputeDefaultCountry()
{
  // clang-format off
  // Same order as in Wii SYSCONF
  static constexpr std::array<std::string_view, 178> COUNTRIES = {
    "--", "JP", "--", "--", "--", "--", "--", "--", "AI", "AG", "AR", "AW", "BS", "BB", "BZ", "BO",
    "BR", "VG", "CA", "KY", "CL", "CO", "CR", "DM", "DO", "EC", "SV", "GF", "GD", "GP", "GT", "GY",
    "HT", "HN", "JM", "MQ", "MX", "MS", "AN", "NI", "PA", "PY", "PE", "KN", "LC", "VC", "SR", "TT",
    "TC", "US", "UY", "VI", "VE", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--", "--",
    "AL", "AU", "AT", "BE", "BA", "BW", "BG", "HR", "CY", "CZ", "DK", "EE", "FI", "FR", "DE", "GR",
    "HU", "IS", "IE", "IT", "LV", "LS", "LI", "LT", "LU", "MK", "MT", "ME", "MZ", "NA", "NL", "NZ",
    "NO", "PL", "PT", "RO", "RU", "RS", "SK", "SI", "ZA", "ES", "SZ", "SE", "CH", "TR", "GB", "ZM",
    "ZW", "AZ", "MR", "ML", "NE", "TD", "SD", "ER", "DJ", "SO", "--", "--", "--", "--", "--", "--",
    "TW", "--", "--", "--", "--", "--", "--", "--", "KR", "--", "--", "--", "--", "--", "--", "--",
    "HK", "MO", "--", "--", "--", "--", "--", "--", "ID", "SG", "TH", "PH", "MY", "--", "--", "--",
    "CN", "--", "--", "--", "--", "--", "--", "--", "AE", "IN", "EG", "OM", "QA", "KW", "SA", "SY",
    "BH", "JO",
  };
  // clang-format on

  std::string country = ComputeDefaultCountryCode();

  // Netherlands Antilles was split into three new codes after the release of the Wii
  if (country == "BQ" || country == "CW" || country == "SX")
    country = "AN";

  const auto it = std::find(COUNTRIES.cbegin(), COUNTRIES.cend(), country);
  if (it == COUNTRIES.cend())
    return std::nullopt;

  return static_cast<u8>(it - COUNTRIES.cbegin());
}

static DiscIO::Region ComputeDefaultRegion()
{
  const std::optional<u8> country = GetDefaultCountry();

  if (country)
  {
    const DiscIO::Region region = DiscIO::SysConfCountryToRegion(GetDefaultCountry());
    ASSERT(region != DiscIO::Region::Unknown);
    return region;
  }

  switch (GetDefaultLanguage())
  {
  case DiscIO::Language::Japanese:
    return DiscIO::Region::NTSC_J;
  case DiscIO::Language::Korean:
    return DiscIO::Region::NTSC_K;
  default:
    return DiscIO::Region::PAL;
  }
}

DiscIO::Language GetDefaultLanguage()
{
  static const DiscIO::Language language = ComputeDefaultLanguage();
  return language;
}

std::optional<u8> GetOptionalDefaultCountry()
{
  static const std::optional<u8> country = ComputeDefaultCountry();
  return country;
}

u8 GetDefaultCountry()
{
  constexpr u8 FALLBACK_COUNTRY = 0x6c;  // Switzerland, as per Dolphin tradition
  return GetOptionalDefaultCountry().value_or(FALLBACK_COUNTRY);
}

DiscIO::Region GetDefaultRegion()
{
  static const DiscIO::Region region = ComputeDefaultRegion();
  return region;
}

}  // namespace Config
