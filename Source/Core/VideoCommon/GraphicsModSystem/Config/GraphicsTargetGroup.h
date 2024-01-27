// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include <picojson.h>

#include "VideoCommon/GraphicsModSystem/Config/GraphicsTarget.h"

struct GraphicsTargetGroupConfig
{
  std::string m_name;
  std::vector<GraphicsTargetConfig> m_targets;

  void SerializeToConfig(picojson::object& json_obj) const;
  bool DeserializeFromConfig(const picojson::object& obj);

  void SerializeToProfile(picojson::object* obj) const;
  void DeserializeFromProfile(const picojson::object& obj);
};
