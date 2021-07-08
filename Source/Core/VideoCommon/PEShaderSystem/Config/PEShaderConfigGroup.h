// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>

#include <picojson.h>

#include "Common/CommonTypes.h"
#include "VideoCommon/PEShaderSystem/Config/PEShaderConfig.h"

namespace VideoCommon::PE
{
struct ShaderConfigGroup
{
  std::vector<ShaderConfig> m_shaders;
  std::vector<u32> m_shader_order;
  u32 m_changes = 0;

  bool AddShader(const std::string& path, ShaderConfig::Source source);
  void RemoveShader(u32 index);

  void SerializeToProfile(picojson::value& value) const;
  void DeserializeFromProfile(const picojson::array& serialized_shaders);

  static ShaderConfigGroup CreateDefaultGroup();
};
}  // namespace VideoCommon::PE
