// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/Resources/Resource.h"

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/TextureAsset.h"

namespace VideoCommon
{
class TextureAndSamplerResource final : public Resource
{
public:
  explicit TextureAndSamplerResource(Resource::ResourceContext resource_context);
  void MarkAsActive() override;
  void MarkAsPending() override;

  class Data
  {
  public:
    AbstractTexture* GetTexture() const { return m_texture.get(); }
    const SamplerState& GetSampler() const { return m_texture_and_sampler_data->sampler; }

  private:
    friend class TextureAndSamplerResource;

    std::shared_ptr<TextureAndSamplerData> m_texture_and_sampler_data;
    std::unique_ptr<AbstractTexture> m_texture;
    TextureConfig m_config;
  };

  const std::shared_ptr<Data>& GetData() const;

private:
  void ResetData() override;
  TaskComplete CollectPrimaryData() override;
  TaskComplete ProcessData() override;

  void OnUnloadRequested() override;

  // Note: asset cache owns the asset, we access as a reference
  TextureAndSamplerAsset* m_texture_and_sampler_asset = nullptr;

  std::shared_ptr<Data> m_current_data;
  std::shared_ptr<Data> m_load_data;
};
}  // namespace VideoCommon
