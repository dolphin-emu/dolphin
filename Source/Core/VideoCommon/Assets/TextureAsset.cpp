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
bool TextureAndSamplerData::FromJson(const CustomAssetLibrary::AssetID& asset_id,
                                     const picojson::object& json, TextureAndSamplerData* data)
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
    data->type = AbstractTextureType::Texture_2D;

    if (!ParseSampler(asset_id, json, &data->sampler))
    {
      return false;
    }
  }
  else if (type == "texturecube")
  {
    data->type = AbstractTextureType::Texture_CubeMap;
  }
  else if (type == "texture2darray")
  {
    data->type = AbstractTextureType::Texture_2DArray;
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

void TextureAndSamplerData::ToJson(picojson::object* obj, const TextureAndSamplerData& data)
{
  if (!obj) [[unlikely]]
    return;

  auto& json_obj = *obj;
  switch (data.type)
  {
  case AbstractTextureType::Texture_2D:
    json_obj.emplace("type", "texture2d");
    break;
  case AbstractTextureType::Texture_CubeMap:
    json_obj.emplace("type", "texturecube");
    break;
  case AbstractTextureType::Texture_2DArray:
    json_obj.emplace("type", "texture2darray");
    break;
  };

  auto wrap_mode_to_string = [](WrapMode mode) {
    switch (mode)
    {
    case WrapMode::Clamp:
      return "clamp";
    case WrapMode::Mirror:
      return "mirror";
    case WrapMode::Repeat:
      return "repeat";
    };

    return "";
  };
  auto filter_mode_to_string = [](FilterMode mode) {
    switch (mode)
    {
    case FilterMode::Linear:
      return "linear";
    case FilterMode::Near:
      return "near";
    };

    return "";
  };

  picojson::object wrap_mode;
  wrap_mode.emplace("u", wrap_mode_to_string(data.sampler.tm0.wrap_u));
  wrap_mode.emplace("v", wrap_mode_to_string(data.sampler.tm0.wrap_v));
  json_obj.emplace("wrap_mode", wrap_mode);

  picojson::object filter_mode;
  filter_mode.emplace("min", filter_mode_to_string(data.sampler.tm0.min_filter));
  filter_mode.emplace("mag", filter_mode_to_string(data.sampler.tm0.mag_filter));
  filter_mode.emplace("mipmap", filter_mode_to_string(data.sampler.tm0.mipmap_filter));
  json_obj.emplace("filter_mode", filter_mode);
}

CustomAssetLibrary::LoadInfo TextureAsset::LoadImpl(const CustomAssetLibrary::AssetID& asset_id)
{
  auto potential_data = std::make_shared<CustomTextureData>();
  const auto loaded_info = m_owning_library->LoadTexture(asset_id, potential_data.get());
  if (loaded_info.bytes_loaded == 0)
    return {};
  {
    std::lock_guard lk(m_data_lock);
    m_loaded = true;
    m_data = std::move(potential_data);
  }
  return loaded_info;
}

CustomAssetLibrary::LoadInfo
TextureAndSamplerAsset::LoadImpl(const CustomAssetLibrary::AssetID& asset_id)
{
  auto potential_data = std::make_shared<TextureAndSamplerData>();
  const auto loaded_info = m_owning_library->LoadTexture(asset_id, potential_data.get());
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
