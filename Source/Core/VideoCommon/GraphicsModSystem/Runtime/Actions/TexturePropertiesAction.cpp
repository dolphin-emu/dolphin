// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/TexturePropertiesAction.h"

#include <string>

#include "Common/StringUtil.h"

std::unique_ptr<TexturePropertiesAction>
TexturePropertiesAction::Create(const picojson::value& json_data)
{
  auto parse_filter = [](const std::string& str) -> std::optional<FilterMode> {
    std::string lower(str);
    Common::ToLower(&lower);
    if (lower == "linear")
      return FilterMode::Linear;
    if (lower == "near")
      return FilterMode::Near;

    return std::nullopt;
  };

  auto parse_wrap_mode = [](const std::string& str) -> std::optional<WrapMode> {
    std::string lower(str);
    Common::ToLower(&lower);
    if (lower == "clamp")
      return WrapMode::Clamp;
    if (lower == "repeat")
      return WrapMode::Repeat;
    if (lower == "mirror")
      return WrapMode::Mirror;

    return std::nullopt;
  };

  std::optional<FilterMode> min_filter;
  const auto& min_filter_json = json_data.get("min_filter");
  if (min_filter_json.is<std::string>())
  {
    min_filter = parse_filter(min_filter_json.to_str());
  }

  std::optional<FilterMode> mag_filter;
  const auto& mag_filter_json = json_data.get("mag_filter");
  if (mag_filter_json.is<std::string>())
  {
    mag_filter = parse_filter(mag_filter_json.to_str());
  }

  std::optional<FilterMode> mipmap_filter;
  const auto& mipmap_filter_json = json_data.get("mipmap_filter");
  if (mipmap_filter_json.is<std::string>())
  {
    mipmap_filter = parse_filter(mipmap_filter_json.to_str());
  }

  std::optional<WrapMode> wrap_u;
  const auto& wrap_u_json = json_data.get("wrap_u");
  if (wrap_u_json.is<std::string>())
  {
    wrap_u = parse_wrap_mode(wrap_u_json.to_str());
  }

  std::optional<WrapMode> wrap_v;
  const auto& wrap_v_json = json_data.get("wrap_v");
  if (wrap_v_json.is<std::string>())
  {
    wrap_v = parse_wrap_mode(wrap_v_json.to_str());
  }

  return std::make_unique<TexturePropertiesAction>(std::move(min_filter), std::move(mag_filter),
                                                   std::move(mipmap_filter), std::move(wrap_u),
                                                   std::move(wrap_v));
}

TexturePropertiesAction::TexturePropertiesAction(std::optional<FilterMode> min_filter,
                                                 std::optional<FilterMode> mag_filter,
                                                 std::optional<FilterMode> mipmap_filter,
                                                 std::optional<WrapMode> wrap_u,
                                                 std::optional<WrapMode> wrap_v)
    : m_min_filter(std::move(min_filter)), m_mag_filter(std::move(mag_filter)),
      m_mipmap_filter(std::move(mipmap_filter)), m_wrap_u(std::move(wrap_u)),
      m_wrap_v(std::move(wrap_v))
{
}

void TexturePropertiesAction::OnTextureLoad(GraphicsModActionData::TextureLoad* texture_load)
{
  if (!texture_load) [[unlikely]]
    return;

  if (!texture_load->min_filter) [[unlikely]]
    return;

  if (!texture_load->mag_filter) [[unlikely]]
    return;

  if (!texture_load->mipmap_filter) [[unlikely]]
    return;

  if (!texture_load->wrap_u) [[unlikely]]
    return;

  if (!texture_load->wrap_v) [[unlikely]]
    return;

  if (m_min_filter)
    *texture_load->min_filter = m_min_filter;

  if (m_mag_filter)
    *texture_load->mag_filter = m_mag_filter;

  if (m_mipmap_filter)
    *texture_load->mipmap_filter = m_mipmap_filter;

  if (m_wrap_u)
    *texture_load->wrap_u = m_wrap_u;

  if (m_wrap_v)
    *texture_load->wrap_v = m_wrap_v;
}
