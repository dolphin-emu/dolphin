// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include <picojson.h>

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"
#include "VideoCommon/RenderState.h"

class TexturePropertiesAction final : public GraphicsModAction
{
public:
  static std::unique_ptr<TexturePropertiesAction> Create(const picojson::value& json_data);
  explicit TexturePropertiesAction(std::optional<FilterMode> min_filter,
                                   std::optional<FilterMode> mag_filter,
                                   std::optional<FilterMode> mipmap_filter,
                                   std::optional<WrapMode> wrap_u, std::optional<WrapMode> wrap_v);
  void OnTextureLoad(GraphicsModActionData::TextureLoad*) override;

private:
  std::optional<FilterMode> m_min_filter;
  std::optional<FilterMode> m_mag_filter;
  std::optional<FilterMode> m_mipmap_filter;
  std::optional<WrapMode> m_wrap_u;
  std::optional<WrapMode> m_wrap_v;
};
