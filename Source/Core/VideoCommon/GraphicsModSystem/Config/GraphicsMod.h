// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <picojson.h>

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModAsset.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModFeature.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsTargetGroup.h"

struct GraphicsModConfig
{
  std::string m_title;
  std::string m_author;
  std::string m_description;
  bool m_enabled = false;
  u16 m_weight = 0;
  std::string m_relative_path;

  enum class Source
  {
    User,
    System
  };
  Source m_source = Source::User;

  std::vector<GraphicsTargetGroupConfig> m_groups;
  std::vector<GraphicsModFeatureConfig> m_features;
  std::vector<GraphicsModAssetConfig> m_assets;

  static std::optional<GraphicsModConfig> Create(const std::string& file, Source source);
  static std::optional<GraphicsModConfig> Create(const picojson::object* obj);

  std::string GetAbsolutePath() const;

  void SerializeToConfig(picojson::object& json_obj) const;
  bool DeserializeFromConfig(const picojson::value& value);

  void SerializeToProfile(picojson::object* value) const;
  void DeserializeFromProfile(const picojson::object& value);

  bool operator<(const GraphicsModConfig& other) const;
};
