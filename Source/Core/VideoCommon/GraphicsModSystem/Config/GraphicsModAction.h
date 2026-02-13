// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include <picojson.h>

namespace GraphicsModSystem::Config
{
struct GraphicsModAction
{
  std::string m_factory_name;
  picojson::value m_data;

  void Serialize(picojson::object& json_obj) const;
  bool Deserialize(const picojson::object& json_obj);
};
}  // namespace GraphicsModSystem::Config
