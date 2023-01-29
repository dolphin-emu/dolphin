// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigInput.h"

#include "Common/Logging/Log.h"
#include "Common/VariantUtil.h"
#include "VideoCommon/RenderState.h"

namespace VideoCommon::PE
{
std::optional<ShaderConfigInput>
DeserializeInputFromConfig(const picojson::object& obj, std::size_t texture_unit,
                           std::size_t parent_pass_index,
                           const std::set<std::string>& known_outputs)
{
  const auto type_iter = obj.find("type");
  if (type_iter == obj.end())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, input 'type' not found");
    return std::nullopt;
  }

  const auto process_common = [&](auto& input) -> bool {
    input.m_texture_unit = static_cast<u32>(texture_unit);

    input.m_sampler_state = RenderState::GetLinearSamplerState();

    const auto sampler_state_mode_iter = obj.find("texture_mode");
    if (sampler_state_mode_iter == obj.end())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load shader configuration file, input 'texture_mode' not found");
      return false;
    }
    if (!sampler_state_mode_iter->second.is<std::string>())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load shader configuration file, input 'texture_mode' is not the right type");
      return false;
    }
    std::string sampler_state_mode = sampler_state_mode_iter->second.to_str();
    std::transform(sampler_state_mode.begin(), sampler_state_mode.end(), sampler_state_mode.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // TODO: add "Border"
    if (sampler_state_mode == "clamp")
    {
      input.m_sampler_state.tm0.wrap_u = WrapMode::Clamp;
      input.m_sampler_state.tm0.wrap_v = WrapMode::Clamp;
    }
    else if (sampler_state_mode == "repeat")
    {
      input.m_sampler_state.tm0.wrap_u = WrapMode::Repeat;
      input.m_sampler_state.tm0.wrap_v = WrapMode::Repeat;
    }
    else if (sampler_state_mode == "mirrored_repeat")
    {
      input.m_sampler_state.tm0.wrap_u = WrapMode::Mirror;
      input.m_sampler_state.tm0.wrap_v = WrapMode::Mirror;
    }
    else
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load shader configuration file, input 'texture_mode' has an invalid "
                    "value '{}'",
                    sampler_state_mode);
      return false;
    }

    const auto sampler_state_filter_iter = obj.find("texture_filter");
    if (sampler_state_filter_iter == obj.end())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load shader configuration file, input 'texture_filter' not found");
      return false;
    }
    if (!sampler_state_filter_iter->second.is<std::string>())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load shader configuration file, input 'texture_filter' is not the right type");
      return false;
    }
    std::string sampler_state_filter = sampler_state_filter_iter->second.to_str();
    std::transform(sampler_state_filter.begin(), sampler_state_filter.end(),
                   sampler_state_filter.begin(), [](unsigned char c) { return std::tolower(c); });
    if (sampler_state_filter == "linear")
    {
      input.m_sampler_state.tm0.min_filter = FilterMode::Linear;
      input.m_sampler_state.tm0.mag_filter = FilterMode::Linear;
      input.m_sampler_state.tm0.mipmap_filter = FilterMode::Linear;
    }
    else if (sampler_state_filter == "point")
    {
      input.m_sampler_state.tm0.min_filter = FilterMode::Linear;
      input.m_sampler_state.tm0.mag_filter = FilterMode::Linear;
      input.m_sampler_state.tm0.mipmap_filter = FilterMode::Linear;
    }
    else
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load shader configuration file, input 'texture_filter' has an invalid "
          "value '{}'",
          sampler_state_filter);
      return false;
    }

    return true;
  };

  std::string type = type_iter->second.to_str();
  std::transform(type.begin(), type.end(), type.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  if (type == "user_image")
  {
    UserImage input;
    if (process_common(input))
      return input;
  }
  else if (type == "internal_image")
  {
    InternalImage input;
    if (!process_common(input))
      return std::nullopt;

    const auto path_iter = obj.find("path");
    if (path_iter == obj.end())
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, input 'path' not found");
      return std::nullopt;
    }
    if (!path_iter->second.is<std::string>())
    {
      ERROR_LOG_FMT(VIDEO,
                    "Failed to load shader configuration file, input 'path' is not the right type");
      return std::nullopt;
    }
    input.m_path = path_iter->second.to_str();

    return input;
  }
  else if (type == "color_buffer")
  {
    ColorBuffer input;
    if (process_common(input))
      return input;
  }
  else if (type == "depth_buffer")
  {
    DepthBuffer input;
    if (process_common(input))
      return input;
  }
  else if (type == "previous_shader")
  {
    PreviousShader input;
    if (process_common(input))
      return input;
  }
  else if (type == "previous_pass")
  {
    PreviousPass input;
    input.m_parent_pass_index = static_cast<u32>(parent_pass_index);
    if (process_common(input))
      return input;
  }
  else if (type == "named_output")
  {
    NamedOutput input;
    if (!process_common(input))
      return std::nullopt;

    const auto name_iter = obj.find("name");
    if (name_iter == obj.end())
    {
      ERROR_LOG_FMT(
          VIDEO,
          "Failed to load shader configuration file, input 'name' for named_output not found");
      return std::nullopt;
    }
    if (!name_iter->second.is<std::string>())
    {
      ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, input 'name' for "
                           "named_output is not the right type");
      return std::nullopt;
    }
    input.m_name = name_iter->second.get<std::string>();
    if (known_outputs.find(input.m_name) == known_outputs.end())
    {
      ERROR_LOG_FMT(
          VIDEO, "Failed to load shader configuration file, input 'name'  for named_output was not "
                 "a valid output");
      return std::nullopt;
    }

    return input;
  }
  else
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, input invalid type '{}'", type);
  }

  return std::nullopt;
}

void SerializeInputToProfile(picojson::object& obj, const ShaderConfigInput& input)
{
  std::visit(overloaded{[&](const UserImage& i) { obj["image_path"] = picojson::value{i.m_path}; },
                        [&](const InternalImage& i) {}, [&](const ColorBuffer& i) {},
                        [&](const DepthBuffer& i) {}, [&](const PreviousPass& i) {},
                        [&](const PreviousShader& i) {}, [&](const NamedOutput& i) {}},
             input);
}

void DeserializeInputFromProfile(const picojson::object& obj, ShaderConfigInput& input)
{
  std::visit(overloaded{[&](UserImage& i) {
                          if (auto it = obj.find("image_path"); it != obj.end())
                          {
                            i.m_path = it->second.to_str();
                          }
                        },
                        [&](InternalImage& i) {}, [&](ColorBuffer& i) {}, [&](DepthBuffer& i) {},
                        [&](PreviousPass& i) {}, [&](PreviousShader& i) {}, [&](NamedOutput& i) {}},
             input);
}
}  // namespace VideoCommon::PE
