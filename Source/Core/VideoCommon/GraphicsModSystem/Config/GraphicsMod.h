// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

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
  static std::optional<GraphicsModConfig> Create(const nlohmann::json* obj);

  std::string GetAbsolutePath() const;

  void SerializeToConfig(nlohmann::json& json_obj) const;
  bool DeserializeFromConfig(const nlohmann::json& value);

  void SerializeToProfile(nlohmann::json* value) const;
  void DeserializeFromProfile(const nlohmann::json& value);
};
