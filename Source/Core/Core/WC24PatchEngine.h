// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

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

std::optional<std::string> GetNetworkPatch(std::string_view source, IsKD is_kd);
std::optional<std::string> GetNetworkPatchByPayload(std::string_view source);
}  // namespace WC24PatchEngine
