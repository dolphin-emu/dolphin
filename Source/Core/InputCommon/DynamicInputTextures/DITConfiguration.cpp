// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/DynamicInputTextures/DITConfiguration.h"

#include <optional>
#include <sstream>
#include <string>

#include <picojson.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
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

bool Configuration::GenerateTextures(const IniFile::Section* sec,
                                     const std::string& controller_name) const
{
  bool any_dirty = false;
  for (const auto& texture_data : m_dynamic_input_textures)
  {
    any_dirty |= GenerateTexture(sec, controller_name, texture_data);
  }

  return any_dirty;
}

bool Configuration::GenerateTexture(const IniFile::Section* sec, const std::string& controller_name,
                                    const Data& texture_data) const
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
}  // namespace InputCommon::DynamicInputTextures
