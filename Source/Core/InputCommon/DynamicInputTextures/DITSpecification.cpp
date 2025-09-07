// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/DynamicInputTextures/DITSpecification.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Core/ConfigManager.h"

namespace InputCommon::DynamicInputTextures
{
bool ProcessSpecificationV1(nlohmann::json& root, std::vector<Data>& input_textures,
                            const std::string& base_path, const std::string& json_file)
{
  auto it = root.find("output_textures");
  if (it == root.end() || !it->is_object())
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Failed to load dynamic input json file '{}' because 'output_textures' is missing or "
        "was not of type object",
        json_file);
    return false;
  }
  const nlohmann::json& output_textures = *it;

  const bool preserve_aspect_ratio = ReadBoolFromJson(root, "preserve_aspect_ratio").value_or(true);

  const std::string generated_folder_name =
      ReadStringFromJson(root, "generated_folder_name")
          .value_or(fmt::format("{}_Generated", SConfig::GetInstance().GetGameID()));

  const nlohmann::json default_host_controls =
      ReadObjectFromJson(root, "default_host_controls").value_or(nlohmann::json{});

  for (auto& [name, data] : output_textures.items())
  {
    Data texture_data;
    texture_data.m_hires_texture_name = name;

    // Required fields
    auto image_it = data.find("image");
    auto controls_it = data.find("emulated_controls");

    if (image_it == data.end() || !image_it->is_string() || controls_it == data.end() ||
        !controls_it->is_object())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load dynamic input json file '{}' because required fields "
                    "'image', or 'emulated_controls' are either "
                    "missing or the incorrect type",
                    json_file);
      return false;
    }
    const std::string& image = *image_it;
    const nlohmann::json& emulated_controls = *controls_it;

    texture_data.m_image_name = image;
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

    for (auto& [emulated_controller_name, map] : emulated_controls.items())
    {
      if (!map.is_object())
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load dynamic input json file '{}' because 'emulated_controls' "
                      "map key '{}' is incorrect type.  Expected map  ",
                      json_file, emulated_controller_name);
        return false;
      }

      auto& key_to_regions = texture_data.m_emulated_controllers[emulated_controller_name];
      for (auto& [emulated_control, regions_array] : map.items())
      {
        if (!regions_array.is_array())
        {
          ERROR_LOG_FMT(
              VIDEO,
              "Failed to load dynamic input json file '{}' because emulated controller '{}' "
              "key '{}' has incorrect value type.  Expected array ",
              json_file, emulated_controller_name, emulated_control);
          return false;
        }

        std::vector<Rect> region_rects;
        for (auto& region : regions_array)
        {
          if (!region.is_array())
          {
            ERROR_LOG_FMT(
                VIDEO,
                "Failed to load dynamic input json file '{}' because emulated controller '{}' "
                "key '{}' has a region with the incorrect type.  Expected array ",
                json_file, emulated_controller_name, emulated_control);
            return false;
          }

          if (region.size() != 4)
          {
            ERROR_LOG_FMT(
                VIDEO,
                "Failed to load dynamic input json file '{}' because emulated controller '{}' "
                "key '{}' has a region that does not have 4 offsets (left, top, right, "
                "bottom).",
                json_file, emulated_controller_name, emulated_control);
            return false;
          }

          if (!std::ranges::all_of(region, &nlohmann::json::is_number))
          {
            ERROR_LOG_FMT(
                VIDEO,
                "Failed to load dynamic input json file '{}' because emulated controller '{}' "
                "key '{}' has a region that has the incorrect offset type.",
                json_file, emulated_controller_name, emulated_control);
            return false;
          }

          Rect r;
          r.left = region[0].get<u32>();
          r.top = region[1].get<u32>();
          r.right = region[2].get<u32>();
          r.bottom = region[3].get<u32>();
          region_rects.push_back(r);
        }
        key_to_regions.insert_or_assign(emulated_control, std::move(region_rects));
      }
    }

    // Default to the default controls but overwrite if the creator
    // has provided something specific
    const nlohmann::json host_controls =
        ReadObjectFromJson(data, "host_controls").value_or(default_host_controls);

    if (host_controls.empty())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load dynamic input json file '{}' because field "
                    "'host_controls' is missing ",
                    json_file);
      return false;
    }

    for (auto& [host_device, map] : host_controls.items())
    {
      if (!map.is_object())
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load dynamic input json file '{}' because 'host_controls' "
                      "map key '{}' is incorrect type ",
                      json_file, host_device);
        return false;
      }
      auto& host_control_to_imagename = texture_data.m_host_devices[host_device];
      for (auto& [host_control, image_name] : map.items())
      {
        if (!image_name.is_string())
        {
          ERROR_LOG_FMT(VIDEO,
                        "Failed to load dynamic input json file '{}' because 'host_controls' "
                        "value '{}' is incorrect type ",
                        json_file, host_control);
          return false;
        }
        host_control_to_imagename[host_control] = image_name.get<std::string>();
      }
    }

    input_textures.emplace_back(std::move(texture_data));
  }

  return true;
}
}  // namespace InputCommon::DynamicInputTextures
