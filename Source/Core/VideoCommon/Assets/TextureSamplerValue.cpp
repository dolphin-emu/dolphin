// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/TextureSamplerValue.h"

#include <optional>

#include "Common/JsonUtil.h"
#include "Common/StringUtil.h"

namespace VideoCommon
{
namespace
{
std::optional<TextureSamplerValue::SamplerOrigin>
ReadSamplerOriginFromJSON(const picojson::object& json)
{
  auto sampler_origin = ReadStringFromJson(json, "sampler_origin").value_or("");
  Common::ToLower(&sampler_origin);

  if (sampler_origin == "asset")
  {
    return TextureSamplerValue::SamplerOrigin::Asset;
  }
  else if (sampler_origin == "texture_hash")
  {
    return TextureSamplerValue::SamplerOrigin::TextureHash;
  }

  return std::nullopt;
}
}  // namespace
std::string TextureSamplerValue::ToString(SamplerOrigin sampler_origin)
{
  if (sampler_origin == SamplerOrigin::Asset)
    return "asset";

  return "texture_hash";
}

bool TextureSamplerValue::FromJson(const picojson::object& json, TextureSamplerValue* data)
{
  data->asset = ReadStringFromJson(json, "asset").value_or("");
  data->texture_hash = ReadStringFromJson(json, "texture_hash").value_or("");
  data->sampler_origin =
      ReadSamplerOriginFromJSON(json).value_or(TextureSamplerValue::SamplerOrigin::Asset);

  return true;
}

void TextureSamplerValue::ToJson(picojson::object* obj, const TextureSamplerValue& data)
{
  if (!obj) [[unlikely]]
    return;

  obj->emplace("asset", data.asset);
  obj->emplace("texture_hash", data.texture_hash);
  obj->emplace("sampler_origin", ToString(data.sampler_origin));
}
}  // namespace VideoCommon
