// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <picojson.h>

#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/Types.h"

namespace GraphicsModSystem::Config
{
struct GraphicsModAsset
{
  VideoCommon::CustomAssetLibrary::AssetID m_asset_id;
  VideoCommon::Assets::AssetMap m_map;

  void Serialize(picojson::object& json_obj) const;
  bool Deserialize(const picojson::object& json_obj);
};
}  // namespace GraphicsModSystem::Config
