// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <set>
#include <string>
#include <variant>

#include <picojson.h>

#include "Common/CommonTypes.h"
#include "VideoCommon/RenderState.h"

namespace VideoCommon::PE
{
struct UserImage
{
  SamplerState m_sampler_state;
  u32 m_texture_unit = 0;
  std::string m_path;
};

struct InternalImage
{
  SamplerState m_sampler_state;
  u32 m_texture_unit = 0;
  std::string m_path;
};

struct ColorBuffer
{
  SamplerState m_sampler_state;
  u32 m_texture_unit = 0;
};

struct DepthBuffer
{
  SamplerState m_sampler_state;
  u32 m_texture_unit = 0;
};

struct PreviousShader
{
  SamplerState m_sampler_state;
  u32 m_texture_unit = 0;
};

struct PreviousPass
{
  SamplerState m_sampler_state;
  u32 m_texture_unit = 0;
  u32 m_parent_pass_index;
};

struct NamedOutput
{
  SamplerState m_sampler_state;
  u32 m_texture_unit = 0;
  std::string m_name;
};

using ShaderConfigInput = std::variant<UserImage, InternalImage, ColorBuffer, DepthBuffer,
                                       PreviousShader, PreviousPass, NamedOutput>;

std::optional<ShaderConfigInput>
DeserializeInputFromConfig(const picojson::object& obj, std::size_t texture_unit,
                           std::size_t parent_pass_index,
                           const std::set<std::string>& known_outputs);
void SerializeInputToProfile(picojson::object& obj, const ShaderConfigInput& input);
void DeserializeInputFromProfile(const picojson::object& obj, ShaderConfigInput& input);

}  // namespace VideoCommon::PE
