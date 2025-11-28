// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/DynamicInputTextures/DITConfiguration.h"

#include <optional>
#include <sstream>
#include <string>

#include <picojson.h>

#include "Common/CommonPaths.h"
#include "Common/IniFile.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/DynamicInputTextures/DITSpecification.h"
#include "InputCommon/ImageOperations.h"

namespace InputCommon::DynamicInputTextures
{
Configuration::Configuration(const std::string& json_path)
{
  picojson::value root;
  std::string error;
  if (!JsonFromFile(json_path, &root, &error))
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load dynamic input json file '{}' due to parse error: {}",
                  json_path, error);
    m_valid = false;
    return;
  }

  SplitPath(json_path, &m_base_path, nullptr, nullptr);

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
          json_path, spec_from_json);
      m_valid = false;
      return;
    }
    specification = static_cast<u8>(spec_from_json);
  }

  if (specification != 1)
  {
    ERROR_LOG_FMT(VIDEO,
                  "Failed to load dynamic input json file '{}', specification '{}' is invalid",
                  json_path, specification);
    m_valid = false;
    return;
  }

  m_valid = ProcessSpecificationV1(root, m_dynamic_input_textures, m_base_path, json_path);
}

Configuration::~Configuration() = default;

void Configuration::GenerateTextures(const Common::IniFile& file,
                                     const std::vector<std::string>& controller_names,
                                     OutputDetails* output) const
{
  for (const auto& texture_data : m_dynamic_input_textures)
  {
    GenerateTexture(file, controller_names, texture_data, output);
  }
}

void Configuration::GenerateTexture(const Common::IniFile& file,
                                    const std::vector<std::string>& controller_names,
                                    const Data& texture_data, OutputDetails* output) const
{
  // Two copies of the loaded texture
  // The first one is used as a fallback if a key or device isn't mapped
  // the second one is used as the final image to write to the textures directory
  const auto original_image = LoadImage(m_base_path + texture_data.m_image_name);
  if (!original_image)
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load image '{}' needed for dynamic input texture generation",
                  texture_data.m_image_name);
    return;
  }
  auto image_to_write = original_image;

  bool dirty = false;

  for (const auto& controller_name : controller_names)
  {
    auto* sec = file.GetSection(controller_name);
    if (!sec)
    {
      continue;
    }

    std::string device_name;
    if (!sec->Get("Device", &device_name))
    {
      continue;
    }

    auto emulated_controls_iter = texture_data.m_emulated_controllers.find(controller_name);
    if (emulated_controls_iter == texture_data.m_emulated_controllers.end())
    {
      continue;
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

    for (auto& [emulated_key, rects] : emulated_controls_iter->second)
    {
      if (!device_found)
      {
        // If we get here, that means the controller is set to a
        // device not exposed to the pack
        dirty = true;
        continue;
      }

      std::string host_key;
      sec->Get(emulated_key, &host_key);

      const auto input_image_iter = host_devices_iter->second.find(host_key);
      if (input_image_iter == host_devices_iter->second.end())
      {
        dirty = true;
      }
      else
      {
        const std::string full_image_path = m_base_path + input_image_iter->second;
        const auto host_key_image = LoadImage(full_image_path);
        if (!host_key_image)
        {
          ERROR_LOG_FMT(VIDEO,
                        "Failed to load image '{}' needed for dynamic input texture generation",
                        full_image_path);
          continue;
        }

        for (const auto& rect : rects)
        {
          InputCommon::ImagePixelData pixel_data;
          if (host_key_image->width == rect.GetWidth() &&
              host_key_image->height == rect.GetHeight())
          {
            pixel_data = *host_key_image;
          }
          else if (texture_data.m_preserve_aspect_ratio)
          {
            pixel_data =
                ResizeKeepAspectRatio(ResizeMode::Nearest, *host_key_image, rect.GetWidth(),
                                      rect.GetHeight(), Pixel{0, 0, 0, 0});
          }
          else
          {
            pixel_data =
                Resize(ResizeMode::Nearest, *host_key_image, rect.GetWidth(), rect.GetHeight());
          }

          CopyImageRegion(pixel_data, *image_to_write,
                          Rect{0, 0, rect.GetWidth(), rect.GetHeight()}, rect);
          dirty = true;
        }
      }
    }
  }

  if (dirty)
  {
    (*output)[texture_data.m_generated_folder_name][texture_data.m_hires_texture_name] =
        std::move(*image_to_write);
  }
}
}  // namespace InputCommon::DynamicInputTextures
