// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigOutput.h"

#include "Common/Logging/Log.h"

namespace VideoCommon::PE
{
std::optional<ShaderConfigOutput> DeserializeOutputFromConfig(const picojson::object& obj)
{
  const auto name_iter = obj.find("name");
  if (name_iter == obj.end())
  {
    ERROR_LOG_FMT(VIDEO, "Failed to load shader configuration file, output 'name' not found");
    return std::nullopt;
  }

  ShaderConfigOutput output;
  output.m_name = name_iter->second.to_str();
  return output;
}
}  // namespace VideoCommon::PE
