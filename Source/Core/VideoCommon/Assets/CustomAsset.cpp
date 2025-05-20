// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/CustomAsset.h"

namespace VideoCommon
{
CustomAsset::CustomAsset(std::shared_ptr<CustomAssetLibrary> library,
                         const CustomAssetLibrary::AssetID& asset_id, u64 asset_handle)
    : m_owning_library(std::move(library)), m_asset_id(asset_id), m_handle(asset_handle)
{
}

bool CustomAsset::Load()
{
  const auto load_information = LoadImpl(m_asset_id);
  if (load_information.m_bytes_loaded > 0)
  {
    m_bytes_loaded = load_information.m_bytes_loaded;
    m_last_loaded_time = ClockType::now();
  }
  return load_information.m_bytes_loaded != 0;
}

void CustomAsset::Unload()
{
  UnloadImpl();
  {
    m_bytes_loaded = 0;
    m_last_loaded_time = TimeType{};
  }
}

CustomAsset::TimeType CustomAsset::GetLastLoadedTime() const
{
  return m_last_loaded_time;
}

std::size_t CustomAsset::GetHandle() const
{
  return m_handle;
}

const CustomAssetLibrary::AssetID& CustomAsset::GetAssetId() const
{
  return m_asset_id;
}

std::size_t CustomAsset::GetByteSizeInMemory() const
{
  return m_bytes_loaded;
}

}  // namespace VideoCommon
