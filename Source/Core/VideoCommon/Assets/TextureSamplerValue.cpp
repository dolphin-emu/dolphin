// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/Assets/TextureSamplerValue.h"

#include <optional>

#include "Common/EnumUtils.h"
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

  // Render targets
  data->is_render_target = ReadBoolFromJson(json, "is_render_target").value_or(false);
  data->camera_originating_draw_call =
      GraphicsModSystem::DrawCallID{ReadNumericFromJson<u64>(json, "camera_draw_call").value_or(0)};
  data->camera_type = static_cast<GraphicsModSystem::CameraType>(
      ReadNumericFromJson<u8>(json, "camera_type").value_or(0));

  return true;
}

void TextureSamplerValue::ToJson(picojson::object* obj, const TextureSamplerValue& data)
{
  if (!obj) [[unlikely]]
    return;

  obj->emplace("asset", data.asset);
  obj->emplace("texture_hash", data.texture_hash);
  obj->emplace("sampler_origin", ToString(data.sampler_origin));
  obj->emplace("is_render_target", data.is_render_target);

  const auto camera_draw_call = static_cast<double>(
      Common::ToUnderlying<GraphicsModSystem::DrawCallID>(data.camera_originating_draw_call));
  obj->emplace("camera_draw_call", camera_draw_call);

  const auto camera_type =
      static_cast<double>(Common::ToUnderlying<GraphicsModSystem::CameraType>(data.camera_type));
  obj->emplace("camera_type", camera_type);
}
}  // namespace VideoCommon
