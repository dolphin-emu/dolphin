// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <picojson.h>

struct GraphicsModFeatureConfig
{
  std::string m_group;
  std::string m_action;
  picojson::value m_action_data;

  bool DeserializeFromConfig(const picojson::object& value);

  void SerializeToProfile(picojson::object* value) const;
  void DeserializeFromProfile(const picojson::object& value);
};
