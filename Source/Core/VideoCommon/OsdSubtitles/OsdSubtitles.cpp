#include "OsdSubtitles.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Timer.h"

#include <VideoCommon\OnScreenDisplay.h>
#include <fstream>
#include <imgui.h>
#include <iostream>
#include <iterator>
#include <picojson.h>

// TODO move to plugin? subtitling on file load is specific to 0079 game
namespace OSDSubtitles
{
const std::string TranslationFile = "C:\\games\\wii\\translate.json";

// TODO check if thers some ready io helper to use for configs?
auto read_file(std::string_view path, bool throwOnMissingFile = false) -> std::string
{
  auto stack = OSD::MessageStack(0, 0, OSD::MessageStackDirection::Upward, true, true, "subtitles");
  OSD::AddMessageStack(stack);

  constexpr auto read_size = std::size_t(4096);
  auto stream = std::ifstream(path.data());
  stream.exceptions(std::ios_base::badbit);

  if (not stream)
  {
    if (throwOnMissingFile)
    {
      throw std::ios_base::failure("Subtitle file does not exist");
    }
    else
    {
      return "";
    }
  }

  auto out = std::string();
  auto buf = std::string(read_size, '\0');
  while (stream.read(&buf[0], read_size))
  {
    out.append(buf, 0, stream.gcount());
  }
  out.append(buf, 0, stream.gcount());
  return out;
}

picojson::value Translations;
static bool isInitialized = false;
static std::mutex init_mutex;

// TODO init on rom load
void TryInitTranslations(const std::string& filename)
{
  std::lock_guard lock{init_mutex};
  isInitialized = false;

  if (isInitialized)
  {
    // another call already did the init
    return;
  }

  auto json = read_file(filename);

  if (json == "")
  {
    return;
  }

  std::string err = picojson::parse(Translations, json);
  if (!err.empty())
  {
    std::cerr << err << std::endl;
    return;
  }

  isInitialized = true;
}

void AddSubtitle(std::string soundFile)
{
  if (!isInitialized)
  {
    return;
  }

  // TODO parse json to structs during init, this suuucks
  if (Translations.contains(soundFile))
  {
    auto tlnode = Translations.get(soundFile);
    if (tlnode.is<picojson::object>() && tlnode.contains("Translation"))
    {
      auto tl = tlnode.get("Translation");
      if (tl.is<std::string>())
      {
        bool allowDups = false;
        auto dupsNode = tlnode.get("AllowDuplicate");
        if (dupsNode.is<bool>())
        {
          allowDups = tlnode.get("AllowDuplicate").get<bool>();
        }

        auto msnode = tlnode.get("Miliseconds");
        // TODO allow for text/hex color (web codes?)
        auto colornode = tlnode.get("Color");

        u32 ms = msnode.is<double>() ? msnode.get<double>() : 3000;
        u32 argb = colornode.is<double>() ? colornode.get<double>() : 4294967040;

        OSD::AddMessage(tl.to_str(), ms, argb, "subtitles", true);
      }
    }
  }
}
}  // namespace OSDSubtitles
