// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>

#include <picojson.h>

namespace VideoCommon::PE
{
struct ShaderConfigOutput
{
  std::string m_name;
};

std::optional<ShaderConfigOutput> DeserializeOutputFromConfig(const picojson::object& obj);

}  // namespace VideoCommon::PE
