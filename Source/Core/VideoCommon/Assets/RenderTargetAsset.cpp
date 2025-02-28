// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/RenderTargetAsset.h"

#include "Common/EnumUtils.h"
#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"

namespace VideoCommon
{
bool RenderTargetData::FromJson(const CustomAssetLibrary::AssetID& asset_id,
                                const picojson::object& json, RenderTargetData* data)
{
  data->m_texture_format = AbstractTextureFormat::RGBA8;
  // TODO: parse format
  return true;
}

void RenderTargetData::ToJson(picojson::object* obj, const RenderTargetData& data)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;
  json_obj.emplace("format", static_cast<double>(data.m_texture_format));
}

CustomAssetLibrary::LoadInfo
RenderTargetAsset::LoadImpl(const CustomAssetLibrary::AssetID& asset_id)
{
  auto potential_data = std::make_shared<RenderTargetData>();
  const auto loaded_info = m_owning_library->LoadRenderTarget(asset_id, potential_data.get());
  if (loaded_info.m_bytes_loaded == 0)
    return {};
  {
    std::lock_guard lk(m_data_lock);
    m_loaded = true;
    m_data = std::move(potential_data);
  }
  return loaded_info;
}
}  // namespace VideoCommon
