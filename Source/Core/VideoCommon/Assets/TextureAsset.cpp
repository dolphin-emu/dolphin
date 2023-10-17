// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/TextureAsset.h"

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "VideoCommon/BPMemory.h"

namespace VideoCommon
{
namespace
{
bool ParseSampler(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                  const picojson::object& json, SamplerState* sampler)
{
  if (!sampler) [[unlikely]]
    return false;

  *sampler = RenderState::GetLinearSamplerState();

  const auto sampler_state_mode_iter = json.find("texture_mode");
  if (sampler_state_mode_iter == json.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'texture_mode' not found", asset_id);
    return false;
  }
  if (!sampler_state_mode_iter->second.is<std::string>())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'texture_mode' is not the right type",
                  asset_id);
    return false;
  }
  std::string sampler_state_mode = sampler_state_mode_iter->second.to_str();
  Common::ToLower(&sampler_state_mode);

  if (sampler_state_mode == "clamp")
  {
    sampler->tm0.wrap_u = WrapMode::Clamp;
    sampler->tm0.wrap_v = WrapMode::Clamp;
  }
  else if (sampler_state_mode == "repeat")
  {
    sampler->tm0.wrap_u = WrapMode::Repeat;
    sampler->tm0.wrap_v = WrapMode::Repeat;
  }
  else if (sampler_state_mode == "mirrored_repeat")
  {
    sampler->tm0.wrap_u = WrapMode::Mirror;
    sampler->tm0.wrap_v = WrapMode::Mirror;
  }
  else
  {
    ERROR_LOG_FMT(VIDEO,
                  "Asset '{}' failed to parse json, 'texture_mode' has an invalid "
                  "value '{}'",
                  asset_id, sampler_state_mode);
    return false;
  }

  const auto sampler_state_filter_iter = json.find("texture_filter");
  if (sampler_state_filter_iter == json.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'texture_filter' not found", asset_id);
    return false;
  }
  if (!sampler_state_filter_iter->second.is<std::string>())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'texture_filter' is not the right type",
                  asset_id);
    return false;
  }
  std::string sampler_state_filter = sampler_state_filter_iter->second.to_str();
  Common::ToLower(&sampler_state_filter);
  if (sampler_state_filter == "linear")
  {
    sampler->tm0.min_filter = FilterMode::Linear;
    sampler->tm0.mag_filter = FilterMode::Linear;
    sampler->tm0.mipmap_filter = FilterMode::Linear;
  }
  else if (sampler_state_filter == "point")
  {
    sampler->tm0.min_filter = FilterMode::Linear;
    sampler->tm0.mag_filter = FilterMode::Linear;
    sampler->tm0.mipmap_filter = FilterMode::Linear;
  }
  else
  {
    ERROR_LOG_FMT(VIDEO,
                  "Asset '{}' failed to parse json, 'texture_filter' has an invalid "
                  "value '{}'",
                  asset_id, sampler_state_filter);
    return false;
  }

  return true;
}
}  // namespace
bool TextureData::FromJson(const CustomAssetLibrary::AssetID& asset_id,
                           const picojson::object& json, TextureData* data)
{
  const auto type_iter = json.find("type");
  if (type_iter == json.end())
  {
    ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, property entry 'type' not found",
                  asset_id);
    return false;
  }
  if (!type_iter->second.is<std::string>())
  {
    ERROR_LOG_FMT(VIDEO,
                  "Asset '{}' failed to parse json, property entry 'type' is not "
                  "the right json type",
                  asset_id);
    return false;
  }
  std::string type = type_iter->second.to_str();
  Common::ToLower(&type);

  if (type == "texture2d")
  {
    data->m_type = TextureData::Type::Type_Texture2D;

    if (!ParseSampler(asset_id, json, &data->m_sampler))
    {
      return false;
    }
  }
  else if (type == "texturecube")
  {
    data->m_type = TextureData::Type::Type_TextureCube;
  }
  else
  {
    ERROR_LOG_FMT(VIDEO,
                  "Asset '{}' failed to parse json, texture type '{}' "
                  "an invalid option",
                  asset_id, type);
    return false;
  }

  return true;
}

CustomAssetLibrary::LoadInfo GameTextureAsset::LoadImpl(const CustomAssetLibrary::AssetID& asset_id)
{
  auto potential_data = std::make_shared<TextureData>();
  const auto loaded_info = m_owning_library->LoadGameTexture(asset_id, potential_data.get());
  if (loaded_info.m_bytes_loaded == 0)
    return {};
  {
    std::lock_guard lk(m_data_lock);
    m_loaded = true;
    m_data = std::move(potential_data);
  }
  return loaded_info;
}

bool GameTextureAsset::Validate(u32 native_width, u32 native_height) const
{
  std::lock_guard lk(m_data_lock);

  if (!m_loaded)
  {
    ERROR_LOG_FMT(VIDEO,
                  "Game texture can't be validated for asset '{}' because it is not loaded yet.",
                  GetAssetId());
    return false;
  }

  if (m_data->m_texture.m_slices.empty())
  {
    ERROR_LOG_FMT(VIDEO,
                  "Game texture can't be validated for asset '{}' because no data was available.",
                  GetAssetId());
    return false;
  }

  if (m_data->m_texture.m_slices.size() > 1)
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Game texture can't be validated for asset '{}' because it has more slices than expected.",
        GetAssetId());
    return false;
  }

  const auto& slice = m_data->m_texture.m_slices[0];
  if (slice.m_levels.empty())
  {
    ERROR_LOG_FMT(
        VIDEO,
        "Game texture can't be validated for asset '{}' because first slice has no data available.",
        GetAssetId());
    return false;
  }

  // Verify that the aspect ratio of the texture hasn't changed, as this could have
  // side-effects.
  const VideoCommon::CustomTextureData::ArraySlice::Level& first_mip = slice.m_levels[0];
  if (first_mip.width * native_height != first_mip.height * native_width)
  {
    // Note: this feels like this should return an error but
    // for legacy reasons this is only a notice that something *could*
    // go wrong
    WARN_LOG_FMT(
        VIDEO,
        "Invalid custom texture size {}x{} for game texture asset '{}'. The aspect differs "
        "from the native size {}x{}.",
        first_mip.width, first_mip.height, GetAssetId(), native_width, native_height);
  }

  // Same deal if the custom texture isn't a multiple of the native size.
  if (native_width != 0 && native_height != 0 &&
      (first_mip.width % native_width || first_mip.height % native_height))
  {
    // Note: this feels like this should return an error but
    // for legacy reasons this is only a notice that something *could*
    // go wrong
    WARN_LOG_FMT(
        VIDEO,
        "Invalid custom texture size {}x{} for game texture asset '{}'. Please use an integer "
        "upscaling factor based on the native size {}x{}.",
        first_mip.width, first_mip.height, GetAssetId(), native_width, native_height);
  }

  return true;
}
}  // namespace VideoCommon
