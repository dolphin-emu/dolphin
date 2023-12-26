// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// WC24PatchEngine
// Allows for replacing URLs used in WC24 requests

#include "Core/WC24PatchEngine.h"

#include <algorithm>
#include <array>
#include <fmt/format.h>

#include "Common/IniFile.h"
#include "Common/StringUtil.h"

#include "Core/CheatCodes.h"
#include "Core/CommonTitles.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"

namespace WC24PatchEngine
{
static constexpr std::array<u64, 15> s_wc24_channels{
    Titles::NINTENDO_CHANNEL_NTSC_U,
    Titles::NINTENDO_CHANNEL_NTSC_J,
    Titles::NINTENDO_CHANNEL_PAL,
    Titles::FORECAST_CHANNEL_NTSC_U,
    Titles::FORECAST_CHANNEL_NTSC_J,
    Titles::FORECAST_CHANNEL_PAL,
    Titles::NEWS_CHANNEL_NTSC_U,
    Titles::NEWS_CHANNEL_NTSC_J,
    Titles::NEWS_CHANNEL_PAL,
    Titles::EVERYBODY_VOTES_CHANNEL_NTSC_U,
    Titles::EVERYBODY_VOTES_CHANNEL_NTSC_J,
    Titles::EVERYBODY_VOTES_CHANNEL_PAL,
    Titles::REGION_SELECT_CHANNEL_NTSC_U,
    Titles::REGION_SELECT_CHANNEL_NTSC_J,
    Titles::REGION_SELECT_CHANNEL_PAL,
};

static std::vector<NetworkPatch> s_patches;

static bool DeserializeLine(const std::string& line, NetworkPatch* patch)
{
  const std::vector<std::string> items = SplitString(line, ':');

  if (items.size() != 3)
    return false;

  patch->source = items[0];
  patch->replacement = items[1];

  if (!TryParse(items[2], &patch->is_kd))
    return false;

  return patch;
}

static void LoadPatchSection(const Common::IniFile& ini)
{
  std::vector<std::string> lines;
  NetworkPatch patch;
  ini.GetLines("WC24Patch", &lines);

  for (const std::string& line : lines)
  {
    if (line.empty())
      continue;

    if (line[0] == '$')
    {
      patch.name = line.substr(1, line.size() - 1);
    }
    else
    {
      if (DeserializeLine(line, &patch))
      {
        s_patches.push_back(patch);
      }
    }
  }

  ReadEnabledAndDisabled(ini, "WC24Patch", &s_patches);
}

static bool IsWC24Channel()
{
  const auto& sconfig = SConfig::GetInstance();
  const auto found =
      std::find(s_wc24_channels.begin(), s_wc24_channels.end(), sconfig.GetTitleID());

  return found != s_wc24_channels.end();
}

static void LoadPatches()
{
  const auto& sconfig = SConfig::GetInstance();
  // We can only load WC24 Channels.
  if (!IsWC24Channel())
    return;

  Common::IniFile ini;
  // If WiiLink is enabled then we load the Default Ini as that has the WiiLink URLs.
  if (Config::Get(Config::MAIN_WII_WIILINK_ENABLE))
    ini = sconfig.LoadDefaultGameIni();
  else
    ini = sconfig.LoadLocalGameIni();

  LoadPatchSection(ini);
}

void Reload()
{
  s_patches.clear();
  LoadPatches();
}

std::optional<std::string> GetNetworkPatch(std::string_view source, IsKD is_kd)
{
  const auto patch =
      std::find_if(s_patches.begin(), s_patches.end(), [&source, &is_kd](const NetworkPatch& p) {
        return p.source == source && p.is_kd == is_kd && p.enabled;
      });
  if (patch == s_patches.end())
    return std::nullopt;

  return patch->replacement;
}

// When we patch for the Socket, we aren't given the URL. Instead, it is in a Host header
// in the payload that the socket is going to send. We need to manually find which patch to apply.
std::optional<std::string> GetNetworkPatchByPayload(std::string_view source)
{
  size_t pos{};
  while (pos < source.size())
  {
    const size_t end_of_line = source.find("\r\n", pos);
    if (source.substr(pos).starts_with("Host: "))
    {
      const std::string_view domain =
          source.substr(pos + 6, end_of_line == std::string_view::npos ? std::string_view::npos :
                                                                         (end_of_line - pos - 6));
      for (const WC24PatchEngine::NetworkPatch& patch : s_patches)
      {
        if (patch.is_kd != WC24PatchEngine::IsKD{true} && domain == patch.source && patch.enabled)
        {
          return fmt::format("{}Host: {}{}", source.substr(0, pos), patch.replacement,
                             end_of_line == std::string_view::npos ? "" :
                                                                     source.substr(end_of_line));
        }
      }

      // No matching patch
      return std::nullopt;
    }

    if (end_of_line == std::string_view::npos)
      break;

    pos = end_of_line + 2;
  }

  return std::nullopt;
}
}  // namespace WC24PatchEngine
