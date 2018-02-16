// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoBackends/Metal/Common.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/VideoConfig.h"

namespace Metal
{
class StreamBuffer;

class MetalContext
{
public:
  MetalContext(mtlpp::Device device);
  ~MetalContext();

  // Populates backend/video config.
  // These are public so that the backend info can be populated without creating a context.
  static void PopulateBackendInfo(VideoConfig* config);
  static void PopulateBackendInfoAdapters(VideoConfig* config,
                                          const ns::Array<mtlpp::Device>& gpu_list);

  mtlpp::Device GetDevice() const { return m_device; }
  StreamBuffer* GetUniformStreamBuffer() const { return m_uniform_stream_buffer.get(); }
  StreamBuffer* GetTextureStreamBuffer() const { return m_texture_stream_buffer.get(); }
  bool CreateStreamingBuffers();

  // Submit the current command buffer, and invalidate any cached state.
  void SubmitCommandBuffer(bool wait_for_completion = false);

  // Find a sampler state by the specified description, if not found, creates it.
  mtlpp::SamplerState& GetSamplerState(const SamplerState& config);
  mtlpp::SamplerState& GetLinearSamplerState() { return m_linear_sampler; }
  mtlpp::SamplerState& GetPointSamplerState() { return m_point_sampler; }
  // Recreate sampler states.
  void ResetSamplerStates();

  // Gets a depth-stencil state matching the specified description.
  mtlpp::DepthStencilState& GetDepthState(const DepthState& ds);

private:
  bool CreateSamplerStates();

  mtlpp::Device m_device;

  std::unique_ptr<StreamBuffer> m_uniform_stream_buffer;
  std::unique_ptr<StreamBuffer> m_texture_stream_buffer;

  // Sampler objects. TODO: Make a hash map.
  std::map<SamplerState, mtlpp::SamplerState> m_sampler_states;
  mtlpp::SamplerState m_point_sampler;
  mtlpp::SamplerState m_linear_sampler;

  std::map<DepthState, mtlpp::DepthStencilState> m_depth_states;
  std::mutex m_depth_state_mutex;
};

extern std::unique_ptr<MetalContext> g_metal_context;

}  // namespace Metal
