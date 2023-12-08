// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <picojson.h>

#include "Common/CommonTypes.h"

namespace Subtitles
{
void OSDInfo(std::string msg);
void Info(std::string msg);
void Error(std::string err);
u32 TryParsecolor(const picojson::value& raw, u32 defaultColor);
}  // namespace Subtitles
