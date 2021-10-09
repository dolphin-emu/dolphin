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
  std::string m_section_name;
  std::string m_option_id;
  std::string m_option_name;
  u32 m_choice = 0;
};

struct GameModDescriptorRiivolutionPatch
{
  std::string m_xml;
  std::string m_root;
  std::vector<GameModDescriptorRiivolutionPatchOption> m_options;
};

struct GameModDescriptorRiivolution
{
  std::vector<GameModDescriptorRiivolutionPatch> m_patches;
};

struct GameModDescriptor
{
  std::string m_base_file;
  std::string m_display_name;
  std::string m_banner;
  std::optional<GameModDescriptorRiivolution> m_riivolution = std::nullopt;
};

std::optional<GameModDescriptor> ParseGameModDescriptorFile(const std::string& filename);
std::optional<GameModDescriptor> ParseGameModDescriptorString(std::string_view json,
                                                              std::string_view json_path);
}  // namespace DiscIO
