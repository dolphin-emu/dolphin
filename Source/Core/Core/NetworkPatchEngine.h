// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include "Common/IniFile.h"

namespace NetworkPatchEngine
{
struct NetworkPatch final
{
  std::string name;
  std::string source;
  std::string replacement;
  bool enabled = false;
  bool is_wc24 = false;
};

void Reload();
bool DeserializeLine(const std::string& line, NetworkPatch* patch);
void LoadPatchSection(const Common::IniFile& globalIni, const Common::IniFile& localIni);
std::optional<NetworkPatch> GetNetworkPatch(const std::string& source);
std::optional<NetworkPatch> GetNetworkPatchByPayload(std::string_view source);
}  // namespace NetworkPatchEngine
