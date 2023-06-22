// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/DynamicInputTextures/DITSpecification.h"

#include <fmt/format.h>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"

namespace InputCommon::DynamicInputTextures
{
bool ProcessSpecificationV1(picojson::value& root, std::vector<Data>& input_textures,
                            const std::string& base_path, const std::string& json_file)
{
  const picojson::value& output_textures_json = root.get("output_textures");
  if (!output_textures_json.is<picojson::object>())
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Failed to load dynamic input json file '{}' because 'output_textures' is missing or "
        "was not of type object",
        json_file);
    return false;
  }

  const picojson::value& preserve_aspect_ratio_json = root.get("preserve_aspect_ratio");

  bool preserve_aspect_ratio = true;
  if (preserve_aspect_ratio_json.is<bool>())
  {
    preserve_aspect_ratio = preserve_aspect_ratio_json.get<bool>();
  }

  const picojson::value& generated_folder_name_json = root.get("generated_folder_name");

  const std::string& game_id = SConfig::GetInstance().GetGameID();
  std::string generated_folder_name = fmt::format("{}_Generated", game_id);
  if (generated_folder_name_json.is<std::string>())
  {
    generated_folder_name = generated_folder_name_json.get<std::string>();
  }

  const picojson::value& default_host_controls_json = root.get("default_host_controls");
  picojson::object default_host_controls;
  if (default_host_controls_json.is<picojson::object>())
  {
    default_host_controls = default_host_controls_json.get<picojson::object>();
  }

  const auto output_textures = output_textures_json.get<picojson::object>();
  for (auto& [name, data] : output_textures)
  {
    Data texture_data;
    texture_data.m_hires_texture_name = name;

    // Required fields
    const picojson::value& image = data.get("image");
    const picojson::value& emulated_controls = data.get("emulated_controls");

    if (!image.is<std::string>() || !emulated_controls.is<picojson::object>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load dynamic input json file '{}' because required fields "
                    "'image', or 'emulated_controls' are either "
                    "missing or the incorrect type",
                    json_file);
      return false;
    }

    texture_data.m_image_name = image.to_str();
    texture_data.m_preserve_aspect_ratio = preserve_aspect_ratio;
    texture_data.m_generated_folder_name = generated_folder_name;

    const std::string image_full_path = base_path + texture_data.m_image_name;
    if (!File::Exists(image_full_path))
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load dynamic input json file '{}' because the image '{}' "
                    "could not be loaded",
                    json_file, image_full_path);
      return false;
    }

    const auto& emulated_controls_json = emulated_controls.get<picojson::object>();
    for (auto& [emulated_controller_name, map] : emulated_controls_json)
    {
      if (!map.is<picojson::object>())
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load dynamic input json file '{}' because 'emulated_controls' "
                      "map key '{}' is incorrect type.  Expected map  ",
                      json_file, emulated_controller_name);
        return false;
      }

      auto& key_to_regions = texture_data.m_emulated_controllers[emulated_controller_name];
      for (auto& [emulated_control, regions_array] : map.get<picojson::object>())
      {
        if (!regions_array.is<picojson::array>())
        {
          ERROR_LOG_FMT(
              VIDEO,
              "Failed to load dynamic input json file '{}' because emulated controller '{}' "
              "key '{}' has incorrect value type.  Expected array ",
              json_file, emulated_controller_name, emulated_control);
          return false;
        }

        std::vector<Rect> region_rects;
        for (auto& region : regions_array.get<picojson::array>())
        {
          Rect r;
          if (!region.is<picojson::array>())
          {
            ERROR_LOG_FMT(
                VIDEO,
                "Failed to load dynamic input json file '{}' because emulated controller '{}' "
                "key '{}' has a region with the incorrect type.  Expected array ",
                json_file, emulated_controller_name, emulated_control);
            return false;
          }

          auto region_offsets = region.get<picojson::array>();

          if (region_offsets.size() != 4)
          {
            ERROR_LOG_FMT(
                VIDEO,
                "Failed to load dynamic input json file '{}' because emulated controller '{}' "
                "key '{}' has a region that does not have 4 offsets (left, top, right, "
                "bottom).",
                json_file, emulated_controller_name, emulated_control);
            return false;
          }

          if (!std::all_of(region_offsets.begin(), region_offsets.end(),
                           [](picojson::value val) { return val.is<double>(); }))
          {
            ERROR_LOG_FMT(
                VIDEO,
                "Failed to load dynamic input json file '{}' because emulated controller '{}' "
                "key '{}' has a region that has the incorrect offset type.",
                json_file, emulated_controller_name, emulated_control);
            return false;
          }

          r.left = static_cast<u32>(region_offsets[0].get<double>());
          r.top = static_cast<u32>(region_offsets[1].get<double>());
          r.right = static_cast<u32>(region_offsets[2].get<double>());
          r.bottom = static_cast<u32>(region_offsets[3].get<double>());
          region_rects.push_back(r);
        }
        key_to_regions.insert_or_assign(emulated_control, std::move(region_rects));
      }
    }

    // Default to the default controls but overwrite if the creator
    // has provided something specific
    picojson::object host_controls = default_host_controls;
    const picojson::value& host_controls_json = data.get("host_controls");
    if (host_controls_json.is<picojson::object>())
    {
      host_controls = host_controls_json.get<picojson::object>();
    }

    if (host_controls.empty())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load dynamic input json file '{}' because field "
                    "'host_controls' is missing ",
                    json_file);
      return false;
    }

    for (auto& [host_device, map] : host_controls)
    {
      if (!map.is<picojson::object>())
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load dynamic input json file '{}' because 'host_controls' "
                      "map key '{}' is incorrect type ",
                      json_file, host_device);
        return false;
      }
      auto& host_control_to_imagename = texture_data.m_host_devices[host_device];
      for (auto& [host_control, image_name] : map.get<picojson::object>())
      {
        host_control_to_imagename.insert_or_assign(host_control, image_name.to_str());
      }
    }

    input_textures.emplace_back(std::move(texture_data));
  }

  return true;
}
}  // namespace InputCommon::DynamicInputTextures
