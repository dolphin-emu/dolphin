// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <nlohmann/json.hpp>

struct GraphicsModFeatureConfig
{
  std::string m_group;
  std::string m_action;
  nlohmann::json m_action_data;

  void SerializeToConfig(nlohmann::json& json_obj) const;
  bool DeserializeFromConfig(const nlohmann::json& value);

  void SerializeToProfile(nlohmann::json* value) const;
  void DeserializeFromProfile(const nlohmann::json& value);
};
