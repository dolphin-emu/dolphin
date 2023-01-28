// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <d3d12.h>
#include <memory>

#include "VideoCommon/AbstractPipeline.h"

namespace DX12
{
class DXPipeline final : public AbstractPipeline
{
public:
  DXPipeline(const AbstractPipelineConfig& config, ID3D12PipelineState* pipeline,
             ID3D12RootSignature* root_signature, AbstractPipelineUsage usage,
             D3D12_PRIMITIVE_TOPOLOGY primitive_topology, bool use_integer_rtv);
  ~DXPipeline() override;

  static std::unique_ptr<DXPipeline> Create(const AbstractPipelineConfig& config,
                                            const void* cache_data, size_t cache_data_size);

  ID3D12PipelineState* GetPipeline() const { return m_pipeline; }
  ID3D12RootSignature* GetRootSignature() const { return m_root_signature; }
  AbstractPipelineUsage GetUsage() const { return m_usage; }
  D3D12_PRIMITIVE_TOPOLOGY GetPrimitiveTopology() const { return m_primitive_topology; }
  bool UseIntegerRTV() const { return m_use_integer_rtv; }

  CacheData GetCacheData() const override;

private:
  ID3D12PipelineState* m_pipeline;
  ID3D12RootSignature* m_root_signature;
  AbstractPipelineUsage m_usage;
  D3D12_PRIMITIVE_TOPOLOGY m_primitive_topology;
  bool m_use_integer_rtv;
};

}  // namespace DX12
