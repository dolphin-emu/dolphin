// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <nlohmann/json_fwd.hpp>

#include "VideoCommon/Assets/CustomAssetLibrary.h"
#include "VideoCommon/Assets/Types.h"

struct GraphicsModAssetConfig
{
  VideoCommon::CustomAssetLibrary::AssetID m_asset_id;
  VideoCommon::Assets::AssetMap m_map;

  void SerializeToConfig(nlohmann::json& json_obj) const;
  bool DeserializeFromConfig(const nlohmann::json& obj);
};
