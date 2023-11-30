// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Metal/MTLPipeline.h"

#include "Common/MsgHandler.h"

static void MarkAsUsed(u32* list, u32 start, u32 length)
{
  for (u32 i = start; i < start + length; ++i)
    *list |= 1 << i;
}

static void GetArguments(NSArray<MTLArgument*>* arguments, u32* textures, u32* samplers,
                         u32* buffers)
{
  for (MTLArgument* argument in arguments)
  {
    const u32 idx = [argument index];
    const u32 length = [argument arrayLength];
    if (idx + length > 32)
    {
      PanicAlertFmt("Making a MTLPipeline with high argument index {:d}..<{:d} for {:s}",  //
                    idx, idx + length, [[argument name] UTF8String]);
      continue;
    }
    switch ([argument type])
    {
    case MTLArgumentTypeTexture:
      if (textures)
        MarkAsUsed(textures, idx, length);
      else
        PanicAlertFmt("Vertex function wants a texture!");
      break;
    case MTLArgumentTypeSampler:
      if (samplers)
        MarkAsUsed(samplers, idx, length);
      else
        PanicAlertFmt("Vertex function wants a sampler!");
      break;
    case MTLArgumentTypeBuffer:
      MarkAsUsed(buffers, idx, length);
      break;
    default:
      break;
    }
  }
}

Metal::PipelineReflection::PipelineReflection(MTLRenderPipelineReflection* reflection)
{
  GetArguments([reflection vertexArguments], nullptr, nullptr, &vertex_buffers);
  GetArguments([reflection fragmentArguments], &textures, &samplers, &fragment_buffers);
}

Metal::Pipeline::Pipeline(const AbstractPipelineConfig& config,
                          MRCOwned<id<MTLRenderPipelineState>> pipeline,
                          const PipelineReflection& reflection, MTLPrimitiveType prim,
                          MTLCullMode cull, DepthState depth, AbstractPipelineUsage usage)
    : AbstractPipeline(config), m_pipeline(std::move(pipeline)), m_prim(prim), m_cull(cull),
      m_depth_stencil(depth), m_usage(usage), m_reflection(reflection)
{
}

Metal::ComputePipeline::ComputePipeline(ShaderStage stage, MTLComputePipelineReflection* reflection,
                                        std::string msl, MRCOwned<id<MTLFunction>> shader,
                                        MRCOwned<id<MTLComputePipelineState>> pipeline)
    : Shader(stage, std::move(msl), std::move(shader)), m_compute_pipeline(std::move(pipeline))
{
  GetArguments([reflection arguments], &m_textures, &m_samplers, &m_buffers);
}
