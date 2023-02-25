// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.h>

#include "VideoBackends/Metal/MRCHelpers.h"
#include "VideoBackends/Metal/MTLObjectCache.h"
#include "VideoBackends/Metal/MTLShader.h"

#include "VideoCommon/AbstractPipeline.h"
#include "VideoCommon/AbstractShader.h"

namespace Metal
{
struct PipelineReflection
{
  u32 textures = 0;
  u32 samplers = 0;
  u32 vertex_buffers = 0;
  u32 fragment_buffers = 0;
  PipelineReflection() = default;
  explicit PipelineReflection(MTLRenderPipelineReflection* reflection);
};

class Pipeline final : public AbstractPipeline
{
public:
  explicit Pipeline(const AbstractPipelineConfig& config,
                    MRCOwned<id<MTLRenderPipelineState>> pipeline,
                    const PipelineReflection& reflection, MTLPrimitiveType prim, MTLCullMode cull,
                    DepthState depth, AbstractPipelineUsage usage);

  id<MTLRenderPipelineState> Get() const { return m_pipeline; }
  MTLPrimitiveType Prim() const { return m_prim; }
  MTLCullMode Cull() const { return m_cull; }
  DepthStencilSelector DepthStencil() const { return m_depth_stencil; }
  AbstractPipelineUsage Usage() const { return m_usage; }
  u32 GetTextures() const { return m_reflection.textures; }
  u32 GetSamplers() const { return m_reflection.samplers; }
  u32 GetVertexBuffers() const { return m_reflection.vertex_buffers; }
  u32 GetFragmentBuffers() const { return m_reflection.fragment_buffers; }
  bool UsesVertexBuffer(u32 index) const { return m_reflection.vertex_buffers & (1 << index); }
  bool UsesFragmentBuffer(u32 index) const { return m_reflection.fragment_buffers & (1 << index); }

private:
  MRCOwned<id<MTLRenderPipelineState>> m_pipeline;
  MTLPrimitiveType m_prim;
  MTLCullMode m_cull;
  DepthStencilSelector m_depth_stencil;
  AbstractPipelineUsage m_usage;
  PipelineReflection m_reflection;
};

class ComputePipeline : public Shader
{
public:
  explicit ComputePipeline(ShaderStage stage, MTLComputePipelineReflection* reflection,
                           std::string msl, MRCOwned<id<MTLFunction>> shader,
                           MRCOwned<id<MTLComputePipelineState>> pipeline);

  id<MTLComputePipelineState> GetComputePipeline() const { return m_compute_pipeline; }
  bool UsesTexture(u32 index) const { return m_textures & (1 << index); }
  bool UsesBuffer(u32 index) const { return m_buffers & (1 << index); }

private:
  MRCOwned<id<MTLComputePipelineState>> m_compute_pipeline;
  u32 m_textures = 0;
  u32 m_buffers = 0;
};
}  // namespace Metal
