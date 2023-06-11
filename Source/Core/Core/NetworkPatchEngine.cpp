// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// NetworkPatchEngine
// Allows for replacing URLs used in Network requests

#include "Core/NetworkPatchEngine.h"
#include <algorithm>
#include <vector>
#include "Common/StringUtil.h"
#include "Core/CheatCodes.h"
#include "Core/ConfigManager.h"

namespace NetworkPatchEngine
{
static std::vector<NetworkPatch> s_patches;

bool DeserializeLine(const std::string& line, NetworkPatch* patch)
{
  const std::vector<std::string> items = SplitString(line, ':');

  if (items.size() != 3)
    return false;

  patch->source = items[0];
  patch->replacement = items[1];

  if (!TryParse(items[2], &patch->is_wc24))
    return false;

  return patch;
}

void LoadPatchSection(const Common::IniFile& globalIni, const Common::IniFile& localIni)
{
  for (const auto* ini : {&globalIni, &localIni})
  {
    std::vector<std::string> lines;
    NetworkPatch patch;
    ini->GetLines("NetworkPatch", &lines);

    for (std::string& line : lines)
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

    ReadEnabledAndDisabled(*ini, "NetworkPatch", &s_patches);
  }
}

void LoadPatches()
{
  const auto& sconfig = SConfig::GetInstance();
  Common::IniFile globalIni = sconfig.LoadDefaultGameIni();
  Common::IniFile localIni = sconfig.LoadLocalGameIni();

  LoadPatchSection(globalIni, localIni);
}

void Reload()
{
  s_patches.clear();
  LoadPatches();
}

std::optional<NetworkPatch> GetNetworkPatch(const std::string& source)
{
  const auto patch =
      std::find_if(s_patches.begin(), s_patches.end(), [&source](NetworkPatch& patch) {
        return patch.source == source && !patch.is_wc24;
      });
  if (patch == s_patches.end())
    return std::nullopt;

  return *patch;
}

// When we patch for the Socket, we aren't given the URL. Instead, it is in a Host header
// in the payload that the socket is going to send. We need to manually find which patch to apply.
std::optional<NetworkPatch> GetNetworkPatchByPayload(std::string_view source)
{
  for (const NetworkPatch& patch : s_patches)
  {
    if (source.find(patch.source) != std::string::npos && !patch.is_wc24)
      // We found the correct patch, return it!
      return patch;
  }

  return std::nullopt;
}
}  // namespace NetworkPatchEngine
