// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <optional>

#include "Common/CommonTypes.h"

namespace DiscIO
{
enum class Language;
enum class Region;
}  // namespace DiscIO

namespace Config
{
DiscIO::Language GetDefaultLanguage();
std::optional<u8> GetOptionalDefaultCountry();
u8 GetDefaultCountry();
DiscIO::Region GetDefaultRegion();
}  // namespace Config
