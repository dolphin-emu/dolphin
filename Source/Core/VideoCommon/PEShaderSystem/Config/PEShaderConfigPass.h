// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <set>
#include <string>
#include <vector>

#include <picojson.h>

#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigInput.h"
#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigOutput.h"

namespace VideoCommon::PE
{
struct ShaderConfigPass
{
  std::vector<ShaderConfigInput> m_inputs;
  std::vector<ShaderConfigOutput> m_outputs;
  std::string m_entry_point;
  std::string m_dependent_option;

  float m_output_scale = 1.0f;

  bool DeserializeFromConfig(const picojson::object& obj, std::size_t pass_index,
                             std::set<std::string>& known_outputs);
  void SerializeToProfile(picojson::object& obj) const;
  void DeserializeFromProfile(const picojson::object& obj);

  static ShaderConfigPass CreateDefaultPass();
};
}  // namespace VideoCommon::PE
