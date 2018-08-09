// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/TextureConfig.h"

class AbstractShader;
class NativeVertexFormat;

// We use three pipeline usages:
//   - GX
//       - Per-stage UBO (VS/GS/PS, VS constants accessible from PS)
//       - 8 combined image samplers (accessible from PS)
//       - 1 SSBO, accessible from PS if bounding box is enabled
//   - Utility
//       - Single UBO, accessible from all stages [set=0, binding=1]
//       - 8 combined image samplers (accessible from PS) [set=1, binding=0-7]
//       - 1 texel buffer, accessible from PS [set=2, binding=0]
//   - Compute
//       - 1 uniform buffer [set=0, binding=1]
//       - 8 combined image samplers [set=1, binding=0-7]
//       - 1 texel buffer [set=2, binding=0]
//       - 1 storage image [set=3, binding=0]
enum class AbstractPipelineUsage
{
  GX,
  Utility
};

struct AbstractPipelineConfig
{
  const NativeVertexFormat* vertex_format;
  const AbstractShader* vertex_shader;
  const AbstractShader* geometry_shader;
  const AbstractShader* pixel_shader;
  RasterizationState rasterization_state;
  DepthState depth_state;
  BlendingState blending_state;

  union FramebufferState
  {
    BitField<0, 8, AbstractTextureFormat> color_texture_format;
    BitField<8, 8, AbstractTextureFormat> depth_texture_format;
    BitField<16, 8, u32> samples;
    BitField<24, 1, u32> per_sample_shading;

    bool operator==(const FramebufferState& rhs) const { return hex == rhs.hex; }
    bool operator!=(const FramebufferState& rhs) const { return hex != rhs.hex; }
    FramebufferState& operator=(const FramebufferState& rhs)
    {
      hex = rhs.hex;
      return *this;
    }

    u32 hex;
  } framebuffer_state;

  AbstractPipelineUsage usage;

  bool operator==(const AbstractPipelineConfig& rhs) const
  {
    return std::tie(vertex_format, vertex_shader, geometry_shader, pixel_shader,
                    rasterization_state.hex, depth_state.hex, blending_state.hex,
                    framebuffer_state.hex, usage) ==
           std::tie(rhs.vertex_format, rhs.vertex_shader, rhs.geometry_shader, rhs.pixel_shader,
                    rhs.rasterization_state.hex, rhs.depth_state.hex, rhs.blending_state.hex,
                    rhs.framebuffer_state.hex, rhs.usage);
  }
  bool operator!=(const AbstractPipelineConfig& rhs) const { return !operator==(rhs); }
  bool operator<(const AbstractPipelineConfig& rhs) const
  {
    return std::tie(vertex_format, vertex_shader, geometry_shader, pixel_shader,
                    rasterization_state.hex, depth_state.hex, blending_state.hex,
                    framebuffer_state.hex, usage) <
           std::tie(rhs.vertex_format, rhs.vertex_shader, rhs.geometry_shader, rhs.pixel_shader,
                    rhs.rasterization_state.hex, rhs.depth_state.hex, rhs.blending_state.hex,
                    rhs.framebuffer_state.hex, rhs.usage);
  }
};

class AbstractPipeline
{
public:
  AbstractPipeline() = default;
  virtual ~AbstractPipeline() = default;
};
