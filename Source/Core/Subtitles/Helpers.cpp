// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Subtitles/Helpers.h"

#include <string>

#include <picojson.h>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"
#include "Subtitles/WebColors.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace Subtitles
{

inline void Info(std::string msg)
{
  OSD::AddMessage(msg, 5000, OSD::Color::GREEN);
  INFO_LOG_FMT(SUBTITLES, "{}", msg);
}

inline void Error(std::string err)
{
  OSD::AddMessage(err, 5000, OSD::Color::RED);
  ERROR_LOG_FMT(SUBTITLES, "{}", err);
}

u32 TryParsecolor(picojson::value& raw, u32 defaultColor)
{
  if (raw.is<double>())
  {
    return raw.get<double>();
  }
  else
  {
    auto str = raw.to_str();
    Common::ToLower(&str);

    if (str.starts_with("0x"))
    {
      u32 parsedHex = 0;
      if (TryParse<u32>(str, &parsedHex))
      {
        return parsedHex;
      }
    }
    else if (WebColors.count(str) == 1)
    {
      // html color name
      return WebColors[str];
    }
  }
  return defaultColor;
}
}  // namespace Subtitles
