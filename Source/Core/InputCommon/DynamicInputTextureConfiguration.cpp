// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/DynamicInputTextureConfiguration.h"

#include <optional>
#include <sstream>
#include <string>

#include <fmt/format.h>
#include <picojson.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ImageOperations.h"
#include "VideoCommon/RenderBase.h"

namespace
{
std::string GetStreamAsString(std::ifstream& stream)
{
  std::stringstream ss;
  ss << stream.rdbuf();
  return ss.str();
}
}  // namespace

namespace InputCommon
{
DynamicInputTextureConfiguration::DynamicInputTextureConfiguration(const std::string& json_file)
{
  std::ifstream json_stream;
  File::OpenFStream(json_stream, json_file, std::ios_base::in);
  if (!json_stream.is_open())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load dynamic input json file '{}'", json_file);
    m_valid = false;
    return;
  }

  picojson::value out;
  const auto error = picojson::parse(out, GetStreamAsString(json_stream));

  if (!error.empty())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load dynamic input json file '{}' due to parse error: {}",
                  json_file, error);
    m_valid = false;
    return;
  }

  const picojson::value& output_textures_json = out.get("output_textures");
  if (!output_textures_json.is<picojson::object>())
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Failed to load dynamic input json file '{}' because 'output_textures' is missing or "
        "was not of type object",
        json_file);
    m_valid = false;
    return;
  }

  const picojson::value& preserve_aspect_ratio_json = out.get("preserve_aspect_ratio");

  bool preserve_aspect_ratio = true;
  if (preserve_aspect_ratio_json.is<bool>())
  {
    preserve_aspect_ratio = preserve_aspect_ratio_json.get<bool>();
  }

  const picojson::value& generated_folder_name_json = out.get("generated_folder_name");

  const std::string& game_id = SConfig::GetInstance().GetGameID();
  std::string generated_folder_name = fmt::format("{}_Generated", game_id);
  if (generated_folder_name_json.is<std::string>())
  {
    generated_folder_name = generated_folder_name_json.get<std::string>();
  }

  const picojson::value& default_host_controls_json = out.get("default_host_controls");
  picojson::object default_host_controls;
  if (default_host_controls_json.is<picojson::object>())
  {
    default_host_controls = default_host_controls_json.get<picojson::object>();
  }

  const auto output_textures = output_textures_json.get<picojson::object>();
  for (auto& [name, data] : output_textures)
  {
    DynamicInputTextureData texture_data;
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
      m_valid = false;
      return;
    }

    texture_data.m_image_name = image.to_str();
    texture_data.m_preserve_aspect_ratio = preserve_aspect_ratio;
    texture_data.m_generated_folder_name = generated_folder_name;

    SplitPath(json_file, &m_base_path, nullptr, nullptr);

    const std::string image_full_path = m_base_path + texture_data.m_image_name;
    if (!File::Exists(image_full_path))
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load dynamic input json file '{}' because the image '{}' "
                    "could not be loaded",
                    json_file, image_full_path);
      m_valid = false;
      return;
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
        m_valid = false;
        return;
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
          m_valid = false;
          return;
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
            m_valid = false;
            return;
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
            m_valid = false;
            return;
          }

          if (!std::all_of(region_offsets.begin(), region_offsets.end(),
                           [](picojson::value val) { return val.is<double>(); }))
          {
            ERROR_LOG_FMT(
                VIDEO,
                "Failed to load dynamic input json file '{}' because emulated controller '{}' "
                "key '{}' has a region that has the incorrect offset type.",
                json_file, emulated_controller_name, emulated_control);
            m_valid = false;
            return;
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
      m_valid = false;
      return;
    }

    for (auto& [host_device, map] : host_controls)
    {
      if (!map.is<picojson::object>())
      {
        ERROR_LOG_FMT(VIDEO,
                      "Failed to load dynamic input json file '{}' because 'host_controls' "
                      "map key '{}' is incorrect type ",
                      json_file, host_device);
        m_valid = false;
        return;
      }
      auto& host_control_to_imagename = texture_data.m_host_devices[host_device];
      for (auto& [host_control, image_name] : map.get<picojson::object>())
      {
        host_control_to_imagename.insert_or_assign(host_control, image_name.to_str());
      }
    }

    m_dynamic_input_textures.emplace_back(std::move(texture_data));
  }
}

DynamicInputTextureConfiguration::~DynamicInputTextureConfiguration() = default;

void DynamicInputTextureConfiguration::GenerateTextures(const IniFile::Section* sec,
                                                        const std::string& controller_name) const
{
  bool any_dirty = false;
  for (const auto& texture_data : m_dynamic_input_textures)
  {
    any_dirty |= GenerateTexture(sec, controller_name, texture_data);
  }

  if (!any_dirty)
    return;
  if (Core::GetState() == Core::State::Starting)
    return;
  if (!g_renderer)
    return;
  g_renderer->ForceReloadTextures();
}

bool DynamicInputTextureConfiguration::GenerateTexture(
    const IniFile::Section* sec, const std::string& controller_name,
    const DynamicInputTextureData& texture_data) const
{
  std::string device_name;
  if (!sec->Get("Device", &device_name))
  {
    return false;
  }

  auto emulated_controls_iter = texture_data.m_emulated_controllers.find(controller_name);
  if (emulated_controls_iter == texture_data.m_emulated_controllers.end())
  {
    return false;
  }

  bool device_found = true;
  auto host_devices_iter = texture_data.m_host_devices.find(device_name);
  if (host_devices_iter == texture_data.m_host_devices.end())
  {
    // If we fail to find our exact device,
    // it's possible the creator doesn't care (single player game)
    // and has used a wildcard for any device
    host_devices_iter = texture_data.m_host_devices.find("");

    if (host_devices_iter == texture_data.m_host_devices.end())
    {
      device_found = false;
    }
  }

  // Two copies of the loaded texture
  // The first one is used as a fallback if a key or device isn't mapped
  // the second one is used as the final image to write to the textures directory
  const auto original_image = LoadImage(m_base_path + texture_data.m_image_name);
  auto image_to_write = original_image;

  bool dirty = false;
  for (auto& [emulated_key, rects] : emulated_controls_iter->second)
  {
    // TODO: Remove this line when we move to C++20
    auto& rects_ref = rects;
    auto apply_original = [&] {
      for (const auto& rect : rects_ref)
      {
        CopyImageRegion(*original_image, *image_to_write, rect, rect);
        dirty = true;
      }
    };

    if (!device_found)
    {
      // If we get here, that means the controller is set to a
      // device not exposed to the pack
      // We still apply the original image, in case the user
      // switched devices and wants to see the changes
      apply_original();
      continue;
    }

    std::string host_key;
    sec->Get(emulated_key, &host_key);

    const auto input_image_iter = host_devices_iter->second.find(host_key);
    if (input_image_iter == host_devices_iter->second.end())
    {
      apply_original();
    }
    else
    {
      const auto host_key_image = LoadImage(m_base_path + input_image_iter->second);

      for (const auto& rect : rects)
      {
        InputCommon::ImagePixelData pixel_data;
        if (host_key_image->width == rect.GetWidth() && host_key_image->height == rect.GetHeight())
        {
          pixel_data = *host_key_image;
        }
        else if (texture_data.m_preserve_aspect_ratio)
        {
          pixel_data = ResizeKeepAspectRatio(ResizeMode::Nearest, *host_key_image, rect.GetWidth(),
                                             rect.GetHeight(), Pixel{0, 0, 0, 0});
        }
        else
        {
          pixel_data =
              Resize(ResizeMode::Nearest, *host_key_image, rect.GetWidth(), rect.GetHeight());
        }

        CopyImageRegion(pixel_data, *image_to_write, Rect{0, 0, rect.GetWidth(), rect.GetHeight()},
                        rect);
        dirty = true;
      }
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
}  // namespace InputCommon
