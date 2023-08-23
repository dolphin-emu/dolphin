// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>

#include <picojson.h>

#include "VideoCommon/Assets/DirectFilesystemAssetLibrary.h"

struct GraphicsModAssetConfig
{
  std::string m_name;
  VideoCommon::DirectFilesystemAssetLibrary::AssetMap m_map;

  bool DeserializeFromConfig(const picojson::object& obj);
};
