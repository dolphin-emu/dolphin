// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/DynamicInputTextures/DITConfiguration.h"

#include <optional>
#include <sstream>
#include <string>

#include <picojson.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Common/VariantUtil.h"
#include "Core/ConfigManager.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/DynamicInputTextures/DITSpecification.h"
#include "InputCommon/ImageOperations.h"

namespace
{
std::string GetStreamAsString(std::ifstream& stream)
{
  std::stringstream ss;
  ss << stream.rdbuf();
  return ss.str();
}
}  // namespace

namespace InputCommon::DynamicInputTextures
{
Configuration::Configuration(const std::string& json_file)
{
  std::ifstream json_stream;
  File::OpenFStream(json_stream, json_file, std::ios_base::in);
  if (!json_stream.is_open())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load dynamic input json file '{}'", json_file);
    m_valid = false;
    return;
  }

  picojson::value root;
  const auto error = picojson::parse(root, GetStreamAsString(json_stream));

  if (!error.empty())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load dynamic input json file '{}' due to parse error: {}",
                  json_file, error);
    m_valid = false;
    return;
  }

  SplitPath(json_file, &m_base_path, nullptr, nullptr);

  const picojson::value& specification_json = root.get("specification");
  u8 specification = 1;
  if (specification_json.is<double>())
  {
    const double spec_from_json = specification_json.get<double>();
    if (spec_from_json < static_cast<double>(std::numeric_limits<u8>::min()) ||
        spec_from_json > static_cast<double>(std::numeric_limits<u8>::max()))
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load dynamic input json file '{}', specification '{}' is not within bounds",
          json_file, spec_from_json);
      m_valid = false;
      return;
    }
    specification = static_cast<u8>(spec_from_json);
  }

  if (specification != 1)
  {
    ERROR_LOG_FMT(VIDEO,
                  "Failed to load dynamic input json file '{}', specification '{}' is invalid",
                  json_file, specification);
    m_valid = false;
    return;
  }

  m_valid = ProcessSpecificationV1(root, m_dynamic_input_textures, m_base_path, json_file);
}

Configuration::~Configuration() = default;

bool Configuration::GenerateTextures(const IniFile& file,
                                     const std::vector<std::string>& controller_names) const
{
  bool any_dirty = false;
  for (const auto& texture_data : m_dynamic_input_textures)
  {
    any_dirty |= GenerateTexture(file, controller_names, texture_data);
  }

  return any_dirty;
}

bool Configuration::GenerateTexture(const IniFile& file,
                                    const std::vector<std::string>& controller_names,
                                    const Data& texture_data) const
{
  // Two copies of the loaded texture
  // The first one is used as a fallback if a key or device isn't mapped
  // the second one is used as the final image to write to the textures directory
  const auto original_image = LoadImage(m_base_path + texture_data.m_image_name);
  auto image_to_write = original_image;

  bool dirty = false;
  for (auto& emulated_entry : emulated_controls_iter->second)
  {
    /*auto apply_original = [&] {
      CopyImageRegion(*original_image, *image_to_write, emulated_entry.m_region,
                      emulated_entry.m_region);
      dirty = true;
    };

    if (!device_found)
    {
      // If we get here, that means the controller is set to a
      // device not exposed to the pack
      // We still apply the original image, in case the user
      // switched devices and wants to see the changes
      apply_original();
      continue;
    }*/

    if (ApplyEmulatedEntry(host_devices_iter->second, emulated_entry, sec, *image_to_write, texture_data.m_preserve_aspect_ratio))
    {
      dirty = true;
    }

    for (auto& [emulated_key, rects] : emulated_controls_iter->second)
    {
      dirty = true;
      //apply_original();
    }
  }

  if (dirty)
  {
    const std::string& game_id = SConfig::GetInstance().GetGameID();
    const auto hi_res_folder =
        File::GetUserPath(D_HIRESTEXTURES_IDX) + texture_data.m_generated_folder_name;
    if (!File::IsDirectory(hi_res_folder))
    {
      File::CreateDir(hi_res_folder);
    }
    WriteImage(hi_res_folder + DIR_SEP + texture_data.m_hires_texture_name, *image_to_write);

    const auto game_id_folder = hi_res_folder + DIR_SEP + "gameids";
    if (!File::IsDirectory(game_id_folder))
    {
      File::CreateDir(game_id_folder);
    }
    File::CreateEmptyFile(game_id_folder + DIR_SEP + game_id + ".txt");

    return true;
  }

  return false;
}

bool Configuration::ApplyEmulatedEntry(const Configuration::HostEntries& host_entries,
                                       const Data::EmulatedEntry& emulated_entry,
                                       const IniFile::Section* section,
                                       ImagePixelData& image_to_write,
                                       bool preserve_aspect_ratio) const
{
  return std::visit(
      overloaded{
          [&, this](const Data::EmulatedSingleEntry& entry) {
            std::string host_key;
            section->Get(entry.m_key, &host_key);
            return ApplyEmulatedSingleEntry(host_entries, std::vector<std::string>{host_key},
                                            entry.m_tag, entry.m_region, image_to_write,
                                            preserve_aspect_ratio);
          },
          [&, this](const Data::EmulatedMultiEntry& entry) {
            return ApplyEmulatedMultiEntry(host_entries, entry, section, image_to_write,
                                           preserve_aspect_ratio);
          },
      },
      emulated_entry);
}

bool Configuration::ApplyEmulatedSingleEntry(const Configuration::HostEntries& host_entries,
                                             const std::vector<std::string> keys,
                                             const std::optional<std::string> tag,
                                             const Rect& region, ImagePixelData& image_to_write,
                                             bool preserve_aspect_ratio) const
{
  for (auto& host_entry : host_entries)
  {
    if (keys == host_entry.m_keys && tag == host_entry.m_tag)
    {
      const auto host_key_image = LoadImage(m_base_path + host_entry.m_path);
      ImagePixelData pixel_data;
      if (host_key_image->width == region.GetWidth() &&
          host_key_image->height == region.GetHeight())
      {
        pixel_data = *host_key_image;
      }
      else if (preserve_aspect_ratio)
      {
        pixel_data = ResizeKeepAspectRatio(ResizeMode::Nearest, *host_key_image, region.GetWidth(),
                                           region.GetHeight(), Pixel{0, 0, 0, 0});
      }
      else
      {
        pixel_data =
            Resize(ResizeMode::Nearest, *host_key_image, region.GetWidth(), region.GetHeight());
      }

      CopyImageRegion(pixel_data, image_to_write, Rect{0, 0, region.GetWidth(), region.GetHeight()},
                      region);

      return true;
    }
  }

  return false;
}

bool Configuration::ApplyEmulatedMultiEntry(const Configuration::HostEntries& host_entries,
                                            const Data::EmulatedMultiEntry& emulated_entry,
                                            const IniFile::Section* section,
                                            InputCommon::ImagePixelData& image_to_write,
                                            bool preserve_aspect_ratio) const
{
  // Try to apply our group'd region first
  const auto emulated_keys = GetKeysFrom(emulated_entry);
  std::vector<std::string> host_keys;
  host_keys.reserve(emulated_keys.size());
  for (const auto& emulated_key : emulated_keys)
  {
    std::string host_key;
    section->Get(emulated_key, &host_key);
    host_keys.push_back(host_key);
  }
  if (ApplyEmulatedSingleEntry(host_entries, host_keys, emulated_entry.m_combined_tag,
                               emulated_entry.m_combined_region, image_to_write,
                               preserve_aspect_ratio))
  {
    return true;
  }

  ImagePixelData temporary_pixel_data(emulated_entry.m_combined_region.GetWidth(),
                                      emulated_entry.m_combined_region.GetHeight());
  bool apply = false;
  for (const auto& sub_entry : emulated_entry.m_sub_entries)
  {
    apply |= ApplyEmulatedEntry(host_entries, sub_entry, section, temporary_pixel_data,
                                preserve_aspect_ratio);
  }

  if (apply)
  {
    CopyImageRegion(temporary_pixel_data, image_to_write,
                    Rect{0, 0, emulated_entry.m_combined_region.GetWidth(),
                         emulated_entry.m_combined_region.GetHeight()},
                    emulated_entry.m_combined_region);
  }

  return apply;
}

std::vector<std::string> Configuration::GetKeysFrom(const Data::EmulatedEntry& emulated_entry) const
{
  return std::visit(
      overloaded{
          [&, this](const Data::EmulatedSingleEntry& entry) {
            return std::vector<std::string>{entry.m_key};
          },
          [&, this](const Data::EmulatedMultiEntry& entry) { return GetKeysFrom(entry); },
      },
      emulated_entry);
}

std::vector<std::string>
Configuration::GetKeysFrom(const Data::EmulatedMultiEntry& emulated_entry) const
{
  std::vector<std::string> result;
  for (const auto& sub_entry : emulated_entry.m_sub_entries)
  {
    const auto sub_entry_keys = GetKeysFrom(sub_entry);
    result.reserve(result.size() + sub_entry_keys.size());
    result.insert(result.end(), sub_entry_keys.begin(), sub_entry_keys.end());
  }
  return result;
}
}  // namespace InputCommon::DynamicInputTextures
