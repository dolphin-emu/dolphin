// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "VideoCommon/GraphicsModSystem/Config/GraphicsTarget.h"

struct GraphicsTargetGroupConfig
{
  std::string m_name;
  std::vector<GraphicsTargetConfig> m_targets;

  void SerializeToConfig(nlohmann::json& json_obj) const;
  bool DeserializeFromConfig(const nlohmann::json& obj);

  void SerializeToProfile(nlohmann::json* obj) const;
  void DeserializeFromProfile(const nlohmann::json& obj);
};
