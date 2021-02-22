// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "InputCommon/DynamicInputTextures/DITData.h"
#include "InputCommon/ImageOperations.h"

namespace InputCommon::DynamicInputTextures
{
class Configuration
{
public:
  explicit Configuration(const std::string& json_file);
  ~Configuration();
  bool GenerateTextures(const IniFile& file,
                        const std::vector<std::string>& controller_names) const;

private:
  bool GenerateTexture(const IniFile& file, const std::vector<std::string>& controller_names,
                       const Data& texture_data) const;

  using HostEntries = std::vector<Data::HostEntry>;
  bool ApplyEmulatedEntry(const HostEntries& host_entries,
                          const Data::EmulatedEntry& emulated_entry,
                          const IniFile::Section* section, ImagePixelData& image_to_write,
                          bool preserve_aspect_ratio) const;
  bool ApplyEmulatedSingleEntry(const HostEntries& host_entries,
                                const std::vector<std::string> keys,
                                const std::optional<std::string> tag, const Rect& region,
                                ImagePixelData& image_to_write, bool preserve_aspect_ratio) const;
  bool ApplyEmulatedMultiEntry(const HostEntries& host_entries,
                               const Data::EmulatedMultiEntry& emulated_entry,
                               const IniFile::Section* section, ImagePixelData& image_to_write,
                               bool preserve_aspect_ratio) const;

  std::vector<std::string> GetKeysFrom(const Data::EmulatedEntry& emulated_entry) const;

  std::vector<std::string> GetKeysFrom(const Data::EmulatedMultiEntry& emulated_entry) const;

  std::vector<Data> m_dynamic_input_textures;
  std::string m_base_path;
  bool m_valid = true;
};
}  // namespace InputCommon::DynamicInputTextures
