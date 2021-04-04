#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "InputCommon/ImageOperations.h"

namespace InputCommon::DynamicInputTextures
{
struct Data
{
  std::string m_image_name;
  std::string m_hires_texture_name;
  std::string m_generated_folder_name;

  struct EmulatedSingleEntry;
  struct EmulatedMultiEntry;
  using EmulatedEntry = std::variant<EmulatedSingleEntry, EmulatedMultiEntry>;

  struct EmulatedSingleEntry
  {
    std::string m_key;
    std::optional<std::string> m_tag;
    Rect m_region;
  };

  struct EmulatedMultiEntry
  {
    std::string m_combined_tag;
    Rect m_combined_region;

    std::vector<EmulatedEntry> m_sub_entries;
  };

  std::unordered_map<std::string, std::vector<EmulatedEntry>> m_emulated_controllers;

  struct HostEntry
  {
    std::vector<std::string> m_keys;
    std::optional<std::string> m_tag;
    std::string m_path;
  };

  std::unordered_map<std::string, std::vector<HostEntry>> m_host_devices;
  bool m_preserve_aspect_ratio = true;
};
}  // namespace InputCommon::DynamicInputTextures
