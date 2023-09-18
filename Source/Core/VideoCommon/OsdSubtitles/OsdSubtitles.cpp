#include "OsdSubtitles.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <string>

#include "Common/CommonTypes.h"
#include "Common/Config/Config.h"
#include "Common/Timer.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <picojson.h>
#include <imgui.h>

namespace OSDSubtitles
{
constexpr float LEFT_MARGIN = 10.0f;         // Pixels to the left of OSD messages.
constexpr float TOP_MARGIN = 10.0f;          // Pixels above the first OSD message.
constexpr float WINDOW_PADDING = 4.0f;       // Pixels between subsequent OSD messages.
constexpr float MESSAGE_FADE_TIME = 1000.f;  // Ms to fade OSD messages at the end of their life.
constexpr float MESSAGE_DROP_TIME = 5000.f;  // Ms to drop OSD messages that has yet to ever render.

static ImVec4 ARGBToImVec4(const u32 argb)
{
  return ImVec4(static_cast<float>((argb >> 16) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 0) & 0xFF) / 255.0f,
                static_cast<float>((argb >> 24) & 0xFF) / 255.0f);
}

const std::string TranslationFile = "C:\\games\\wii\\translate.json";

// TODO check if just using OnScreenDisplay::Message will be enough
struct Subtitle
{
  Subtitle() = default;
  Subtitle(std::string text_, u32 duration_, u32 color_)
      : text(std::move(text_)), duration(duration_), color(color_)
  {
    timer.Start();
  }
  s64 TimeRemaining() const { return duration - timer.ElapsedMs(); }
  std::string text;
  Common::Timer timer;
  u32 duration = 0;
  bool ever_drawn = false;
  u32 color = 0;

  // TODO Position from json?
};

// TODO check if thers some ready io helper to use for configs?
auto read_file(std::string_view path, bool throwOnMissingFile = false) -> std::string
{
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

  INFO_LOG_FMT(FILEMON, "{}", "reading file");

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
bool isInitialized = false;
static std::mutex init_mutex;
static std::mutex subtitles_mutex;
void InitTranslations(const std::string& filename)
{
  std::lock_guard lock{init_mutex};

  if (isInitialized)
  {
    //another call already did the init
    return;
  }

  auto json = read_file(filename);

  std::string err = picojson::parse(Translations, json);
  if (!err.empty())
  {
    std::cerr << err << std::endl;
  }

  isInitialized = true;
}

static std::multimap<std::string, Subtitle> currentSubtitles;
void AddSubtitle(std::string soundFile)
{
  if (!isInitialized)
  {
    InitTranslations(TranslationFile);
  }

  auto tlnode = Translations.get(soundFile);
  if (tlnode.is<picojson::object>())
  {
    auto tl = tlnode.get("Translation");
    if (tl.is<std::string>())
    {
      //check if subtitle is still on screen
      if (currentSubtitles.contains(soundFile))
      {
        if (tlnode.get("AllowDuplicate").evaluate_as_boolean() == false)
        {
          return;
        }
      }

      auto msnode = tlnode.get("Miliseconds");
      //TODO allow for text/hex color (web codes?)
      auto colornode = tlnode.get("Color");

      u32 ms = msnode.is<double>() ? msnode.get<double>() : 3000;
      u32 argb = colornode.is<double>() ? colornode.get<double>() : 4294967040;

      currentSubtitles.emplace(soundFile, Subtitle(std::move(tl.to_str()), ms, argb));
    }
  }
}

static float DrawMessage(int index, Subtitle& msg, const ImVec2& position, int time_left)
{
  // We have to provide a window name, and these shouldn't be duplicated.
  // So instead, we generate a name based on the number of messages drawn.
  const std::string window_name = fmt::format("osd_subtitle_{}", index);

  // The size must be reset, otherwise the length of old messages could influence new ones.
  ImGui::SetNextWindowPos(position);
  ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));

  // Gradually fade old messages away (except in their first frame)
  const float fade_time = std::max(std::min(MESSAGE_FADE_TIME, (float)msg.duration), 1.f);
  const float alpha = std::clamp(time_left / fade_time, 0.f, 1.f);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, msg.ever_drawn ? alpha : 1.0);

  float window_height = 0.0f;
  if (ImGui::Begin(window_name.c_str(), nullptr,
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav |
                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing))
  {
    // Use %s in case message contains %.
    ImGui::TextColored(ARGBToImVec4(msg.color), "%s", msg.text.c_str());
    window_height =
        ImGui::GetWindowSize().y + (WINDOW_PADDING * ImGui::GetIO().DisplayFramebufferScale.y);
  }

  ImGui::End();
  ImGui::PopStyleVar();

  msg.ever_drawn = true;

  return window_height;
}

void DrawMessages()
{
  const bool draw_messages = true; //Config::Get(Config::MAIN_OSD_MESSAGES);
  const float current_x = 0;
      //LEFT_MARGIN * ImGui::GetIO().DisplayFramebufferScale.x + s_obscured_pixels_left;
  float current_y = 0; //  TOP_MARGIN* ImGui::GetIO().DisplayFramebufferScale.y + s_obscured_pixels_top;
  int index = 0;

  std::lock_guard lock{subtitles_mutex};

  for (auto it = currentSubtitles.begin(); it != currentSubtitles.end();)
  {
    Subtitle& msg = it->second; //?!!?!? second?!
    const s64 time_left = msg.TimeRemaining();

    // Make sure we draw them at least once if they were printed with 0ms,
    // unless enough time has expired, in that case, we drop them
    if (time_left <= 0 && (msg.ever_drawn || -time_left >= MESSAGE_DROP_TIME))
    {
      it = currentSubtitles.erase(it);
      continue;
    }
    else
    {
      ++it;
    }

    if (draw_messages)
      current_y += DrawMessage(index++, msg, ImVec2(current_x, current_y), time_left);
  }
}

void ClearMessages()
{
  std::lock_guard lock{subtitles_mutex};
  currentSubtitles.clear();
}

}  // namespace OSD
