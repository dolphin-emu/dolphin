// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/Resources/Resource.h"

#include "VideoCommon/Assets/CustomTextureData.h"
#include "VideoCommon/Assets/TextureAsset.h"

namespace VideoCommon
{
class TextureDataResource final : public Resource
{
public:
  TextureDataResource(Resource::ResourceContext resource_context);

  std::shared_ptr<CustomTextureData> GetData() const;
  CustomAsset::TimeType GetLoadTime() const;

  void MarkAsActive() override;
  void MarkAsPending() override;

private:
  TaskComplete CollectPrimaryData() override;
  TaskComplete ProcessData() override;

  TextureAsset* m_texture_asset = nullptr;

  std::shared_ptr<CustomTextureData> m_load_texture_data;
  CustomAsset::TimeType m_load_time = {};

  std::shared_ptr<CustomTextureData> m_current_texture_data;
  CustomAsset::TimeType m_current_time = {};
};
}  // namespace VideoCommon
