// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/TextureAsset.h"

#include <optional>

#include "Common/JsonUtil.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "VideoCommon/BPMemory.h"

namespace VideoCommon
{
namespace
{
std::optional<WrapMode> ReadWrapModeFromJSON(const picojson::object& json, const std::string& uv)
{
  auto uv_mode = ReadStringFromJson(json, uv).value_or("");
  Common::ToLower(&uv_mode);

  if (uv_mode == "clamp")
  {
    return WrapMode::Clamp;
  }
  else if (uv_mode == "repeat")
  {
    return WrapMode::Repeat;
  }
  else if (uv_mode == "mirror")
  {
    return WrapMode::Mirror;
  }

  return std::nullopt;
}

std::optional<FilterMode> ReadFilterModeFromJSON(const picojson::object& json,
                                                 const std::string& filter)
{
  auto filter_mode = ReadStringFromJson(json, filter).value_or("");
  Common::ToLower(&filter_mode);

  if (filter_mode == "linear")
  {
    return FilterMode::Linear;
  }
  else if (filter_mode == "near")
  {
    return FilterMode::Near;
  }

  return std::nullopt;
}

bool ParseSampler(const VideoCommon::CustomAssetLibrary::AssetID& asset_id,
                  const picojson::object& json, SamplerState* sampler)
{
  if (!sampler) [[unlikely]]
    return false;

  *sampler = RenderState::GetLinearSamplerState();

  const auto sampler_state_wrap_iter = json.find("wrap_mode");
  if (sampler_state_wrap_iter != json.end())
  {
    if (!sampler_state_wrap_iter->second.is<picojson::object>())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'wrap_mode' is not the right type",
                    asset_id);
      return false;
    }
    const auto sampler_state_wrap_obj = sampler_state_wrap_iter->second.get<picojson::object>();

    if (const auto mode = ReadWrapModeFromJSON(sampler_state_wrap_obj, "u"))
    {
      sampler->tm0.wrap_u = *mode;
    }
    else
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, 'wrap_mode[u]' has an invalid "
                    "value",
                    asset_id);
      return false;
    }

    if (const auto mode = ReadWrapModeFromJSON(sampler_state_wrap_obj, "v"))
    {
      sampler->tm0.wrap_v = *mode;
    }
    else
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, 'wrap_mode[v]' has an invalid "
                    "value",
                    asset_id);
      return false;
    }
  }

  const auto sampler_state_filter_iter = json.find("filter_mode");
  if (sampler_state_filter_iter != json.end())
  {
    if (!sampler_state_filter_iter->second.is<picojson::object>())
    {
      ERROR_LOG_FMT(VIDEO, "Asset '{}' failed to parse json, 'filter_mode' is not the right type",
                    asset_id);
      return false;
    }
    const auto sampler_state_filter_obj = sampler_state_filter_iter->second.get<picojson::object>();

    if (const auto mode = ReadFilterModeFromJSON(sampler_state_filter_obj, "min"))
    {
      sampler->tm0.min_filter = *mode;
    }
    else
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, 'filter_mode[min]' has an invalid "
                    "value",
                    asset_id);
      return false;
    }

    if (const auto mode = ReadFilterModeFromJSON(sampler_state_filter_obj, "mag"))
    {
      sampler->tm0.mag_filter = *mode;
    }
    else
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, 'filter_mode[mag]' has an invalid "
                    "value",
                    asset_id);
      return false;
    }

    if (const auto mode = ReadFilterModeFromJSON(sampler_state_filter_obj, "mipmap"))
    {
      sampler->tm0.mipmap_filter = *mode;
    }
    else
    {
      ERROR_LOG_FMT(VIDEO,
                    "Asset '{}' failed to parse json, 'filter_mode[mipmap]' has an invalid "
                    "value",
                    asset_id);
      return false;
    }
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
