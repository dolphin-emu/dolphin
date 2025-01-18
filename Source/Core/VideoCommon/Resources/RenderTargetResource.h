// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/Resources/Resource.h"

#include <memory>

#include "VideoCommon/AbstractTexture.h"
#include "VideoCommon/Assets/RenderTargetAsset.h"

namespace VideoCommon
{
class RenderTargetResource final : public Resource
{
public:
  RenderTargetResource(Resource::ResourceContext resource_context);

  void MarkAsActive() override;
  void MarkAsPending() override;

  class Data
  {
  public:
    AbstractTexture* GetTexture() const;
    CustomAsset::TimeType GetLoadTime() const { return m_load_time; }
    const SamplerState& GetSampler() const { return m_render_target_data->sampler; }
    RenderTargetData::Color GetClearColor() const { return m_render_target_data->clear_color; }

  private:
    friend class RenderTargetResource;

    std::shared_ptr<RenderTargetData> m_render_target_data;
    std::unique_ptr<AbstractTexture> m_texture;
    TextureConfig m_config;
    CustomAsset::TimeType m_load_time;
  };

  const std::shared_ptr<Data>& GetData() const;

private:
  void ResetData() override;
  TaskComplete CollectPrimaryData() override;
  TaskComplete ProcessData() override;

  void OnUnloadRequested() override;

  RenderTargetAsset* m_render_target_asset = nullptr;
  std::shared_ptr<Data> m_current_data;
  std::shared_ptr<Data> m_load_data;
};
}  // namespace VideoCommon
