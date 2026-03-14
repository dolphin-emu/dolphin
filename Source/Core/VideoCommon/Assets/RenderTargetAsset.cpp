// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/RenderTargetAsset.h"

#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"

namespace VideoCommon
{
bool RenderTargetData::FromJson(const CustomAssetLibrary::AssetID& asset_id,
                                const picojson::object& json, RenderTargetData* data)
{
  data->texture_format = AbstractTextureFormat::RGBA8;
  // TODO: parse format
  data->height = ReadNumericFromJson<u16>(json, "height").value_or(0);
  data->width = ReadNumericFromJson<u16>(json, "width").value_or(0);
  data->texture_type =
      static_cast<AbstractTextureType>(ReadNumericFromJson<u64>(json, "texture_type").value_or(0));
  data->render_type =
      static_cast<RenderType>(ReadNumericFromJson<u64>(json, "render_type").value_or(0));
  return true;
}

void RenderTargetData::ToJson(picojson::object* obj, const RenderTargetData& data)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;
  json_obj.emplace("height", static_cast<double>(data.height));
  json_obj.emplace("width", static_cast<double>(data.width));
  json_obj.emplace("texture_format", static_cast<double>(data.texture_format));
  json_obj.emplace("texture_type", static_cast<double>(data.texture_type));
  json_obj.emplace("render_type", static_cast<double>(data.render_type));
  // TODO: sampler...
}

CustomAssetLibrary::LoadInfo
RenderTargetAsset::LoadImpl(const CustomAssetLibrary::AssetID& asset_id)
{
  auto potential_data = std::make_shared<RenderTargetData>();
  const auto loaded_info = m_owning_library->LoadRenderTarget(asset_id, potential_data.get());
  if (loaded_info.bytes_loaded == 0)
    return {};
  {
    std::lock_guard lk(m_data_lock);
    m_loaded = true;
    m_data = std::move(potential_data);
  }
  return loaded_info;
}
}  // namespace VideoCommon
