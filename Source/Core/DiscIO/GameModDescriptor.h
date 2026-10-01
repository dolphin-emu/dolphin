// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"

namespace DiscIO
{
struct GameModDescriptorRiivolutionPatchOption
{
  std::string section_name;
  std::string option_id;
  std::string option_name;
  u32 choice = 0;
};

struct GameModDescriptorRiivolutionPatch
{
  std::string xml;
  std::string root;
  std::vector<GameModDescriptorRiivolutionPatchOption> options;
};

struct GameModDescriptorRiivolution
{
  std::vector<GameModDescriptorRiivolutionPatch> patches;
  u32 console_ng_id = 0;
};

struct GameModDescriptor
{
  std::string base_file;
  std::string display_name;
  std::string maker;
  std::string banner;
  std::optional<GameModDescriptorRiivolution> riivolution = std::nullopt;
};

std::optional<GameModDescriptor> ParseGameModDescriptorFile(const std::string& filename);
std::optional<GameModDescriptor> ParseGameModDescriptorString(std::string_view json,
                                                              std::string_view json_path);
std::string WriteGameModDescriptorString(const GameModDescriptor& descriptor, bool pretty);
bool WriteGameModDescriptorFile(const std::string& filename, const GameModDescriptor& descriptor,
                                bool pretty);
}  // namespace DiscIO
