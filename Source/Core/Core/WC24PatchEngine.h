// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include "Common/IniFile.h"

namespace WC24PatchEngine
{
enum class IsKD : bool;

struct NetworkPatch final
{
  std::string name;
  std::string source;
  std::string replacement;
  bool enabled = false;
  IsKD is_kd = IsKD{false};
};

void Reload();
bool DeserializeLine(const std::string& line, NetworkPatch* patch);
bool IsWC24Channel();
void LoadPatchSection(const Common::IniFile& ini);
std::optional<std::string> GetNetworkPatch(const std::string& source, IsKD is_kd);
std::optional<std::string> GetNetworkPatchByPayload(std::string_view source);
}  // namespace WC24PatchEngine
