// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
