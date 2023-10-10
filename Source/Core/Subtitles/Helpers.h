#pragma once
#include <string>
#include <Common/CommonTypes.h>
#include <Common/StringUtil.h>
#include <VideoCommon/OnScreenDisplay.h>
#include <picojson.h>
#include "WebColors.h"

namespace Subtitles
{

void Info(std::string msg)
{
  OSD::AddMessage(msg, 2000, OSD::Color::GREEN);
  INFO_LOG_FMT(SUBTITLES, "{}", msg);
}
void Error(std::string err)
{
  OSD::AddMessage(err, 2000, OSD::Color::RED);
  ERROR_LOG_FMT(SUBTITLES, "{}", err);
}

u32 TryParsecolor(picojson::value raw, u32 defaultColor)
{
  if (raw.is<double>())
    return raw.get<double>();
  else
  {
    auto str = raw.to_str();
    Common::ToLower(&str);

    if (str.starts_with("0x"))
      // hex string
      return std::stoul(str, nullptr, 16);
    else if (WebColors.count(str) == 1)
      // html color name
      return WebColors[str];
    else
      // color noted with 3 or 4 base10 numers (rgb/argb)
      try  // string parsing suucks
      {
        // try parse (a)rgb space delimited color
        u32 a, r, g, b;
        auto parts = SplitString(str, ' ');
        if (parts.size() == 4)
        {
          a = std::stoul(parts[0], nullptr, 10);
          r = std::stoul(parts[1], nullptr, 10);
          g = std::stoul(parts[2], nullptr, 10);
          b = std::stoul(parts[3], nullptr, 10);
          return a << 24 | r << 16 | g << 8 | b;
        }
        else if (parts.size() == 3)
        {
          a = 255;
          r = std::stoul(parts[0], nullptr, 10);
          g = std::stoul(parts[1], nullptr, 10);
          b = std::stoul(parts[2], nullptr, 10);
          return a << 24 | r << 16 | g << 8 | b;
        }
      }
      catch (std::exception x)
      {
        Error("Invalid color: " + str);
      }
  }
  return defaultColor;
}
}  // namespace Subtitles
