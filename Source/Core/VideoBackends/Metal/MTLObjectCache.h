// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.h>
#include <memory>

#include "VideoBackends/Metal/MRCHelpers.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/RenderState.h"

struct AbstractPipelineConfig;
class AbstractPipeline;

namespace Metal
{
class Shader;
extern MRCOwned<id<MTLDevice>> g_device;
extern MRCOwned<id<MTLCommandQueue>> g_queue;

struct DepthStencilSelector
{
  u8 value;

  DepthStencilSelector() : value(0) {}
  DepthStencilSelector(bool update_enable, enum CompareMode cmp)
      : value(update_enable | (static_cast<u32>(cmp) << 1))
  {
  }
  DepthStencilSelector(DepthState state)
      : DepthStencilSelector(state.testenable ? state.updateenable : false,
                             state.testenable ? state.func : CompareMode::Always)
  {
  }

  bool UpdateEnable() const { return value & 1; }
  enum CompareMode CompareMode() const { return static_cast<enum CompareMode>(value >> 1); }

  bool operator==(const DepthStencilSelector& other) const { return value == other.value; }
  bool operator!=(const DepthStencilSelector& other) const { return !(*this == other); }
  static constexpr size_t N_VALUES = 1 << 4;
};

struct SamplerSelector
{
  u8 value;
  SamplerSelector() : value(0) {}
  SamplerSelector(SamplerState state)
  {
    value = (static_cast<u32>(state.tm0.min_filter.Value()) << 0) |
            (static_cast<u32>(state.tm0.mag_filter.Value()) << 1) |
            (static_cast<u32>(state.tm0.mipmap_filter.Value()) << 2) |
            (static_cast<u32>(state.tm0.anisotropic_filtering) << 3);
    value |= (static_cast<u32>(state.tm0.wrap_u.Value()) +
              3 * static_cast<u32>(state.tm0.wrap_v.Value()))
             << 4;
  }
  FilterMode MinFilter() const { return static_cast<FilterMode>(value & 1); }
  FilterMode MagFilter() const { return static_cast<FilterMode>((value >> 1) & 1); }
  FilterMode MipFilter() const { return static_cast<FilterMode>((value >> 2) & 1); }
  WrapMode WrapU() const { return static_cast<WrapMode>((value >> 4) % 3); }
  WrapMode WrapV() const { return static_cast<WrapMode>((value >> 4) / 3); }
  bool AnisotropicFiltering() const { return ((value >> 3) & 1); }

  bool operator==(const SamplerSelector& other) const { return value == other.value; }
  bool operator!=(const SamplerSelector& other) const { return !(*this == other); }
  static constexpr size_t N_VALUES = (1 << 4) * 9;
};

class ObjectCache
{
  ObjectCache();

public:
  ~ObjectCache();

  static void Initialize(MRCOwned<id<MTLDevice>> device);
  static void Shutdown();

  id<MTLDepthStencilState> GetDepthStencil(DepthStencilSelector sel) { return m_dss[sel.value]; }

  id<MTLSamplerState> GetSampler(SamplerSelector sel)
  {
    if (__builtin_expect(!m_samplers[sel.value], false))
      m_samplers[sel.value] = CreateSampler(sel);
    return m_samplers[sel.value];
  }

  id<MTLSamplerState> GetSampler(SamplerState state) { return GetSampler(SamplerSelector(state)); }

  void ReloadSamplers();

  std::unique_ptr<AbstractPipeline> CreatePipeline(const AbstractPipelineConfig& config);
  void ShaderDestroyed(const Shader* shader);

private:
  class Internal;
  std::unique_ptr<Internal> m_internal;
  MRCOwned<id<MTLSamplerState>> CreateSampler(SamplerSelector sel);
  MRCOwned<id<MTLDepthStencilState>> m_dss[DepthStencilSelector::N_VALUES];
  MRCOwned<id<MTLSamplerState>> m_samplers[SamplerSelector::N_VALUES];
};

extern std::unique_ptr<ObjectCache> g_object_cache;
}  // namespace Metal
