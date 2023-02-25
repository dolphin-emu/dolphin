// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

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
//   - GX Uber
//       - Same as GX, plus one VS SSBO for vertices if dynamic vertex loading is enabled
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
  GXUber,
  Utility
};

struct AbstractPipelineConfig
{
  const NativeVertexFormat* vertex_format = nullptr;
  const AbstractShader* vertex_shader = nullptr;
  const AbstractShader* geometry_shader = nullptr;
  const AbstractShader* pixel_shader = nullptr;
  RasterizationState rasterization_state;
  DepthState depth_state;
  BlendingState blending_state;
  FramebufferState framebuffer_state;

  AbstractPipelineUsage usage = AbstractPipelineUsage::GX;

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
  explicit AbstractPipeline(const AbstractPipelineConfig& config) : m_config(config) {}
  virtual ~AbstractPipeline() = default;

  AbstractPipelineConfig m_config;

  // "Cache data" can be used to assist a driver with creating pipelines by using previously
  // compiled shader ISA. The abstract shaders and creation struct are still required to create
  // pipeline objects, the cache is optionally used by the driver to speed up compilation.
  using CacheData = std::vector<u8>;
  virtual CacheData GetCacheData() const { return {}; }
};
