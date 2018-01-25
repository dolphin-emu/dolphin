// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "Common/CommonTypes.h"
#include "Common/LinearDiskCache.h"

#include "VideoBackends/Vulkan/Constants.h"
#include "VideoBackends/Vulkan/Texture2D.h"

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/RenderState.h"
#include "VideoCommon/VertexShaderGen.h"

namespace Vulkan
{
class CommandBufferManager;
class VertexFormat;
class StreamBuffer;

class ObjectCache
{
public:
  ObjectCache();
  ~ObjectCache();

  // Descriptor set layout accessor. Used for allocating descriptor sets.
  VkDescriptorSetLayout GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT layout) const
  {
    return m_descriptor_set_layouts[layout];
  }
  // Pipeline layout accessor. Used to fill in required field in PipelineInfo.
  VkPipelineLayout GetPipelineLayout(PIPELINE_LAYOUT layout) const
  {
    return m_pipeline_layouts[layout];
  }
  // Shared utility shader resources
  VertexFormat* GetUtilityShaderVertexFormat() const
  {
    return m_utility_shader_vertex_format.get();
  }
  StreamBuffer* GetUtilityShaderVertexBuffer() const
  {
    return m_utility_shader_vertex_buffer.get();
  }
  StreamBuffer* GetUtilityShaderUniformBuffer() const
  {
    return m_utility_shader_uniform_buffer.get();
  }

  // Static samplers
  VkSampler GetPointSampler() const { return m_point_sampler; }
  VkSampler GetLinearSampler() const { return m_linear_sampler; }
  VkSampler GetSampler(const SamplerState& info);

  // Dummy image for samplers that are unbound
  Texture2D* GetDummyImage() const { return m_dummy_texture.get(); }
  VkImageView GetDummyImageView() const { return m_dummy_texture->GetView(); }
  // Perform at startup, create descriptor layouts, compiles all static shaders.
  bool Initialize();

  // Clear sampler cache, use when anisotropy mode changes
  // WARNING: Ensure none of the objects from here are in use when calling
  void ClearSamplerCache();

private:
  bool CreateDescriptorSetLayouts();
  void DestroyDescriptorSetLayouts();
  bool CreatePipelineLayouts();
  void DestroyPipelineLayouts();
  bool CreateUtilityShaderVertexFormat();
  bool CreateStaticSamplers();
  void DestroySamplers();

  std::array<VkDescriptorSetLayout, NUM_DESCRIPTOR_SET_LAYOUTS> m_descriptor_set_layouts = {};
  std::array<VkPipelineLayout, NUM_PIPELINE_LAYOUTS> m_pipeline_layouts = {};

  std::unique_ptr<VertexFormat> m_utility_shader_vertex_format;
  std::unique_ptr<StreamBuffer> m_utility_shader_vertex_buffer;
  std::unique_ptr<StreamBuffer> m_utility_shader_uniform_buffer;

  VkSampler m_point_sampler = VK_NULL_HANDLE;
  VkSampler m_linear_sampler = VK_NULL_HANDLE;

  std::map<SamplerState, VkSampler> m_sampler_cache;

  // Dummy image for samplers that are unbound
  std::unique_ptr<Texture2D> m_dummy_texture;
};

extern std::unique_ptr<ObjectCache> g_object_cache;

}  // namespace Vulkan
