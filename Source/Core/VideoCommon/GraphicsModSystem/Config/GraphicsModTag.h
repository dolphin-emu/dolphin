// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <picojson.h>

#include "Common/Matrix.h"

namespace GraphicsModSystem::Config
{
struct GraphicsModTag
{
  std::string m_name;
  std::string m_description;
  Common::Vec3 m_color;

  void Serialize(picojson::object& json_obj) const;
  bool Deserialize(const picojson::object& json_obj);
};
}  // namespace GraphicsModSystem::Config
