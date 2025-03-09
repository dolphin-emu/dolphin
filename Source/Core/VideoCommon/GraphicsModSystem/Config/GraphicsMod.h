// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <picojson.h>

#include "Common/CommonTypes.h"

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModAction.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModAsset.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModHashPolicy.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsModTag.h"
#include "VideoCommon/GraphicsModSystem/Config/GraphicsTarget.h"

namespace GraphicsModSystem::Config
{
// Update this version when the hashing approach or data
// changes
static constexpr u16 LATEST_SCHEMA_VERSION = 1;

struct GraphicsMod
{
  u16 m_schema_version = LATEST_SCHEMA_VERSION;

  std::string m_title;
  std::string m_author;
  std::string m_description;
  std::string m_mod_version;

  std::vector<GraphicsModAsset> m_assets;
  std::vector<GraphicsModTag> m_tags;
  std::vector<AnyTarget> m_targets;
  std::vector<GraphicsModAction> m_actions;
  std::map<u64, std::vector<u64>> m_target_index_to_action_indexes;
  std::map<std::string, std::vector<u64>> m_tag_name_to_action_indexes;

  HashPolicy m_default_hash_policy;

  static std::optional<GraphicsMod> Create(const std::string& file);

  void Serialize(picojson::object& json_obj) const;
  bool Deserialize(const picojson::value& value);
};
}  // namespace GraphicsModSystem::Config
