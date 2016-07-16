// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>

#include "Common/Hash.h"
#include "Common/LinearDiskCache.h"

#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VertexFormat.h"

namespace Vulkan
{
std::unique_ptr<ObjectCache> g_object_cache;

ObjectCache::ObjectCache(VkInstance instance, VkPhysicalDevice physical_device, VkDevice device,
                         const VkPhysicalDeviceProperties& properties,
                         const VkPhysicalDeviceFeatures& features)
    : m_instance(instance), m_physical_device(physical_device), m_device(device),
      m_device_features(features), m_device_properties(properties), m_vs_cache(device),
      m_gs_cache(device), m_ps_cache(device)
{
  // Read device physical memory properties, we need it for allocating buffers
  vkGetPhysicalDeviceMemoryProperties(physical_device, &m_device_memory_properties);

  // Would any drivers be this silly? I hope not...
  m_device_properties.limits.minUniformBufferOffsetAlignment = std::max(
      m_device_properties.limits.minUniformBufferOffsetAlignment, static_cast<VkDeviceSize>(1));
  m_device_properties.limits.minTexelBufferOffsetAlignment = std::max(
      m_device_properties.limits.minTexelBufferOffsetAlignment, static_cast<VkDeviceSize>(1));
  m_device_properties.limits.optimalBufferCopyOffsetAlignment = std::max(
      m_device_properties.limits.optimalBufferCopyOffsetAlignment, static_cast<VkDeviceSize>(1));
  m_device_properties.limits.optimalBufferCopyRowPitchAlignment = std::max(
      m_device_properties.limits.optimalBufferCopyRowPitchAlignment, static_cast<VkDeviceSize>(1));
}

ObjectCache::~ObjectCache()
{
  if (m_point_sampler != VK_NULL_HANDLE)
    vkDestroySampler(m_device, m_point_sampler, nullptr);

  if (m_linear_sampler != VK_NULL_HANDLE)
    vkDestroySampler(m_device, m_linear_sampler, nullptr);

  if (m_standard_pipeline_layout != VK_NULL_HANDLE)
    vkDestroyPipelineLayout(m_device, m_standard_pipeline_layout, nullptr);
  if (m_bbox_pipeline_layout != VK_NULL_HANDLE)
    vkDestroyPipelineLayout(m_device, m_bbox_pipeline_layout, nullptr);
  if (m_push_constant_pipeline_layout != VK_NULL_HANDLE)
    vkDestroyPipelineLayout(m_device, m_push_constant_pipeline_layout, nullptr);

  for (VkDescriptorSetLayout layout : m_descriptor_set_layouts)
  {
    if (layout != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
  }
}

bool ObjectCache::Initialize()
{
  if (!CreatePipelineCache(true))
    return false;

  if (!CreateDescriptorSetLayouts())
    return false;

  if (!CreatePipelineLayout())
    return false;

  if (!CreateUtilityShaderVertexFormat())
    return false;

  if (!CreateStaticSamplers())
    return false;

  if (!CompileSharedShaders())
    return false;

  m_utility_shader_vertex_buffer =
      StreamBuffer::Create(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 1024 * 1024, 4 * 1024 * 1024);
  m_utility_shader_uniform_buffer =
      StreamBuffer::Create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 1024, 4 * 1024 * 1024);
  if (!m_utility_shader_vertex_buffer || !m_utility_shader_uniform_buffer)
    return false;

  return true;
}

void ObjectCache::Shutdown()
{
  // We have to destroy these objects here since their destructors access g_object_cache,
  // which is null at the time ObjectCache::~ObjectCache is called.
  m_utility_shader_uniform_buffer.reset();
  m_utility_shader_vertex_buffer.reset();
  m_utility_shader_vertex_format.reset();
  ClearPipelineCache();
  ClearSamplerCache();
  DestroySharedShaders();
}

bool ObjectCache::GetMemoryType(u32 bits, VkMemoryPropertyFlags properties, u32* out_type_index)
{
  for (u32 i = 0; i < VK_MAX_MEMORY_TYPES; i++)
  {
    if ((bits & (1 << i)) != 0)
    {
      u32 supported = m_device_memory_properties.memoryTypes[i].propertyFlags & properties;
      if (supported == properties)
      {
        *out_type_index = i;
        return true;
      }
    }
  }

  return false;
}

u32 ObjectCache::GetMemoryType(u32 bits, VkMemoryPropertyFlags properties)
{
  u32 type_index = VK_MAX_MEMORY_TYPES;
  if (!GetMemoryType(bits, properties, &type_index))
    PanicAlert("Unable to find memory type for %x:%x", bits, properties);

  return type_index;
}

u32 ObjectCache::GetUploadMemoryType(u32 bits, bool* is_coherent)
{
  // Try for coherent memory first.
  VkMemoryPropertyFlags flags =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

  u32 type_index;
  if (!GetMemoryType(bits, flags, &type_index))
  {
    WARN_LOG(
        VIDEO,
        "Vulkan: Failed to find a coherent memory type for uploads, this will affect performance.");

    // Try non-coherent memory.
    flags &= ~VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (!GetMemoryType(bits, flags, &type_index))
    {
      // We shouldn't have any memory types that aren't host-visible.
      PanicAlert("Unable to get memory type for upload.");
      type_index = 0;
    }
  }

  if (is_coherent)
    *is_coherent = ((flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0);

  return type_index;
}

u32 ObjectCache::GetReadbackMemoryType(u32 bits, bool* is_coherent, bool* is_cached)
{
  // Try for cached and coherent memory first.
  VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                VK_MEMORY_PROPERTY_HOST_CACHED_BIT |
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

  u32 type_index;
  if (!GetMemoryType(bits, flags, &type_index))
  {
    // For readbacks, caching is more important than coherency.
    flags &= ~VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    if (!GetMemoryType(bits, flags, &type_index))
    {
      WARN_LOG(VIDEO, "Vulkan: Failed to find a cached memory type for readbacks, this will affect "
                      "performance.");

      // Remove the cached bit as well.
      flags &= ~VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
      if (!GetMemoryType(bits, flags, &type_index))
      {
        // We shouldn't have any memory types that aren't host-visible.
        PanicAlert("Unable to get memory type for upload.");
        type_index = 0;
      }
    }
  }

  if (is_coherent)
    *is_coherent = ((flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0);
  if (is_cached)
    *is_cached = ((flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0);

  return type_index;
}

static VkPipelineRasterizationStateCreateInfo
GetVulkanRasterizationState(const RasterizationState& state)
{
  return {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,                  // const void*                               pNext
      0,                        // VkPipelineRasterizationStateCreateFlags   flags
      VK_FALSE,                 // VkBool32                                  depthClampEnable
      VK_FALSE,                 // VkBool32                                  rasterizerDiscardEnable
      VK_POLYGON_MODE_FILL,     // VkPolygonMode                             polygonMode
      state.cull_mode,          // VkCullModeFlags                           cullMode
      VK_FRONT_FACE_CLOCKWISE,  // VkFrontFace                               frontFace
      VK_FALSE,                 // VkBool32                                  depthBiasEnable
      0.0f,                     // float                                     depthBiasConstantFactor
      0.0f,                     // float                                     depthBiasClamp
      0.0f,                     // float                                     depthBiasSlopeFactor
      1.0f                      // float                                     lineWidth
  };
}

static VkPipelineMultisampleStateCreateInfo
GetVulkanMultisampleState(const RasterizationState& rs_state)
{
  return {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,                      // const void*                              pNext
      0,                            // VkPipelineMultisampleStateCreateFlags    flags
      rs_state.samples,             // VkSampleCountFlagBits                    rasterizationSamples
      rs_state.per_sample_shading,  // VkBool32                                 sampleShadingEnable
      1.0f,                         // float                                    minSampleShading
      nullptr,                      // const VkSampleMask*                      pSampleMask;
      VK_FALSE,  // VkBool32                                 alphaToCoverageEnable
      VK_FALSE   // VkBool32                                 alphaToOneEnable
  };
}

static VkPipelineDepthStencilStateCreateInfo
GetVulkanDepthStencilState(const DepthStencilState& state)
{
  return {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,             // const void*                               pNext
      0,                   // VkPipelineDepthStencilStateCreateFlags    flags
      state.test_enable,   // VkBool32                                  depthTestEnable
      state.write_enable,  // VkBool32                                  depthWriteEnable
      state.compare_op,    // VkCompareOp                               depthCompareOp
      VK_FALSE,            // VkBool32                                  depthBoundsTestEnable
      VK_FALSE,            // VkBool32                                  stencilTestEnable
      {},                  // VkStencilOpState                          front
      {},                  // VkStencilOpState                          back
      0.0f,                // float                                     minDepthBounds
      1.0f                 // float                                     maxDepthBounds
  };
}

static VkPipelineColorBlendAttachmentState GetVulkanAttachmentBlendState(const BlendState& state)
{
  VkPipelineColorBlendAttachmentState vk_state = {
      state.blend_enable,  // VkBool32                                  blendEnable
      state.src_blend,     // VkBlendFactor                             srcColorBlendFactor
      state.dst_blend,     // VkBlendFactor                             dstColorBlendFactor
      state.blend_op,      // VkBlendOp                                 colorBlendOp
      state.src_blend,     // VkBlendFactor                             srcAlphaBlendFactor
      state.dst_blend,     // VkBlendFactor                             dstAlphaBlendFactor
      state.blend_op,      // VkBlendOp                                 alphaBlendOp
      state.write_mask     // VkColorComponentFlags                     colorWriteMask
  };

  switch (vk_state.srcAlphaBlendFactor)
  {
  case VK_BLEND_FACTOR_SRC_COLOR:
    vk_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    break;
  case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
    vk_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    break;
  case VK_BLEND_FACTOR_DST_COLOR:
    vk_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    break;
  case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
    vk_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    break;
  default:
    break;
  }

  switch (vk_state.dstAlphaBlendFactor)
  {
  case VK_BLEND_FACTOR_SRC_COLOR:
    vk_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    break;
  case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
    vk_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    break;
  case VK_BLEND_FACTOR_DST_COLOR:
    vk_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    break;
  case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
    vk_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    break;
  default:
    break;
  }

  if (state.use_dst_alpha)
  {
    // Colors should blend against SRC1_ALPHA
    if (vk_state.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA)
      vk_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA;
    else if (vk_state.srcColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
      vk_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;

    // Colors should blend against SRC1_ALPHA
    if (vk_state.dstColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA)
      vk_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC1_ALPHA;
    else if (vk_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
      vk_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;

    vk_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    vk_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    vk_state.alphaBlendOp = VK_BLEND_OP_ADD;
  }

  return vk_state;
}

static VkPipelineColorBlendStateCreateInfo
GetVulkanColorBlendState(const BlendState& state,
                         const VkPipelineColorBlendAttachmentState* attachments,
                         uint32_t num_attachments)
{
  VkPipelineColorBlendStateCreateInfo vk_state = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,                  // const void*                                   pNext
      0,                        // VkPipelineColorBlendStateCreateFlags          flags
      state.logic_op_enable,    // VkBool32                                      logicOpEnable
      state.logic_op,           // VkLogicOp                                     logicOp
      num_attachments,          // uint32_t                                      attachmentCount
      attachments,              // const VkPipelineColorBlendAttachmentState*    pAttachments
      {1.0f, 1.0f, 1.0f, 1.0f}  // float                                         blendConstants[4]
  };

  return vk_state;
}

VkPipeline ObjectCache::GetPipeline(const PipelineInfo& info)
{
  auto iter = m_pipeline_objects.find(info);
  if (iter != m_pipeline_objects.end())
    return iter->second;

  // Declare descriptors for empty vertex buffers/attributes
  static const VkPipelineVertexInputStateCreateInfo empty_vertex_input_state = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,  // const void*                                pNext
      0,        // VkPipelineVertexInputStateCreateFlags       flags
      0,        // uint32_t                                    vertexBindingDescriptionCount
      nullptr,  // const VkVertexInputBindingDescription*      pVertexBindingDescriptions
      0,        // uint32_t                                    vertexAttributeDescriptionCount
      nullptr   // const VkVertexInputAttributeDescription*    pVertexAttributeDescriptions
  };

  // Vertex inputs
  const VkPipelineVertexInputStateCreateInfo& vertex_input_state =
      (info.vertex_format) ? info.vertex_format->GetVertexInputStateInfo() :
                             empty_vertex_input_state;

  // Input assembly
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,                  // const void*                                pNext
      0,                        // VkPipelineInputAssemblyStateCreateFlags    flags
      info.primitive_topology,  // VkPrimitiveTopology                        topology
      VK_TRUE                   // VkBool32                                   primitiveRestartEnable
  };

  // Shaders to stages
  VkPipelineShaderStageCreateInfo shader_stages[3];
  uint32_t num_shader_stages = 0;
  if (info.vs != VK_NULL_HANDLE)
    shader_stages[num_shader_stages++] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                          nullptr,
                                          0,
                                          VK_SHADER_STAGE_VERTEX_BIT,
                                          info.vs,
                                          "main"};
  if (info.gs != VK_NULL_HANDLE)
    shader_stages[num_shader_stages++] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                          nullptr,
                                          0,
                                          VK_SHADER_STAGE_GEOMETRY_BIT,
                                          info.gs,
                                          "main"};
  if (info.ps != VK_NULL_HANDLE)
    shader_stages[num_shader_stages++] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                          nullptr,
                                          0,
                                          VK_SHADER_STAGE_FRAGMENT_BIT,
                                          info.ps,
                                          "main"};

  // Fill in full descriptor structs
  VkPipelineRasterizationStateCreateInfo rasterization_state =
      GetVulkanRasterizationState(info.rasterization_state);
  VkPipelineMultisampleStateCreateInfo multisample_state =
      GetVulkanMultisampleState(info.rasterization_state);
  VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
      GetVulkanDepthStencilState(info.depth_stencil_state);
  VkPipelineColorBlendAttachmentState blend_attachment_state =
      GetVulkanAttachmentBlendState(info.blend_state);
  VkPipelineColorBlendStateCreateInfo blend_state =
      GetVulkanColorBlendState(info.blend_state, &blend_attachment_state, 1);

  // Viewport is used with dynamic state
  static const VkViewport viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
  static const VkRect2D scissor = {{0, 0}, {1, 1}};
  static const VkPipelineViewportStateCreateInfo viewport_state = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      nullptr,
      0,          // VkPipelineViewportStateCreateFlags    flags;
      1,          // uint32_t                              viewportCount
      &viewport,  // const VkViewport*                     pViewports
      1,          // uint32_t                              scissorCount
      &scissor    // const VkRect2D*                       pScissors
  };

  // Set viewport and scissor dynamic state so we can change it elsewhere
  static const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                                  VK_DYNAMIC_STATE_SCISSOR};
  static const VkPipelineDynamicStateCreateInfo dynamic_state = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr,
      0,                          // VkPipelineDynamicStateCreateFlags    flags
      ARRAYSIZE(dynamic_states),  // uint32_t                             dynamicStateCount
      dynamic_states              // const VkDynamicState*                pDynamicStates
  };

  // Combine to pipeline info
  VkGraphicsPipelineCreateInfo pipeline_info = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      nullptr,                // VkStructureType sType
      0,                      // VkPipelineCreateFlags                            flags
      num_shader_stages,      // uint32_t                                         stageCount
      shader_stages,          // const VkPipelineShaderStageCreateInfo*           pStages
      &vertex_input_state,    // const VkPipelineVertexInputStateCreateInfo*      pVertexInputState
      &input_assembly_state,  // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState
      nullptr,                // const VkPipelineTessellationStateCreateInfo*     pTessellationState
      &viewport_state,        // const VkPipelineViewportStateCreateInfo*         pViewportState
      &rasterization_state,  // const VkPipelineRasterizationStateCreateInfo*    pRasterizationState
      &multisample_state,    // const VkPipelineMultisampleStateCreateInfo*      pMultisampleState
      &depth_stencil_state,  // const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState
      &blend_state,          // const VkPipelineColorBlendStateCreateInfo*       pColorBlendState
      &dynamic_state,        // const VkPipelineDynamicStateCreateInfo*          pDynamicState
      info.pipeline_layout,  // VkPipelineLayout                                 layout
      info.render_pass,      // VkRenderPass                                     renderPass
      0,                     // uint32_t                                         subpass
      VK_NULL_HANDLE,        // VkPipeline                                       basePipelineHandle
      -1                     // int32_t                                          basePipelineIndex
  };

  VkPipeline pipeline = VK_NULL_HANDLE;
  VkResult res =
      vkCreateGraphicsPipelines(m_device, m_pipeline_cache, 1, &pipeline_info, nullptr, &pipeline);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkCreateGraphicsPipelines failed: ");

  m_pipeline_objects.emplace(info, pipeline);
  return pipeline;
}

void ObjectCache::ClearPipelineCache()
{
  // Care here to ensure that pipelines are destroyed before shader modules
  for (const auto& it : m_pipeline_objects)
  {
    if (it.second != VK_NULL_HANDLE)
      vkDestroyPipeline(m_device, it.second, nullptr);
  }
  m_pipeline_objects.clear();

  // Reallocate the pipeline cache object, so it starts fresh and we don't
  // save old pipelines to disk. TODO: Maybe we don't want to do this?
  vkDestroyPipelineCache(m_device, m_pipeline_cache, nullptr);
  m_pipeline_cache = VK_NULL_HANDLE;
  if (!CreatePipelineCache(false))
    PanicAlert("Failed to re-create pipeline cache");
}

class PipelineCacheReadCallback : public LinearDiskCacheReader<u32, u8>
{
public:
  PipelineCacheReadCallback(std::vector<u8>* data) : m_data(data) {}
  virtual void Read(const u32& key, const u8* value, u32 value_size) override
  {
    m_data->resize(value_size);
    if (value_size > 0)
      memcpy(m_data->data(), value, value_size);
  }

private:
  std::vector<u8>* m_data;
};

class PipelineCacheReadIgnoreCallback : public LinearDiskCacheReader<u32, u8>
{
public:
  virtual void Read(const u32& key, const u8* value, u32 value_size) override {}
};

void ObjectCache::SavePipelineCache()
{
  size_t data_size;
  VkResult res = vkGetPipelineCacheData(m_device, m_pipeline_cache, &data_size, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkGetPipelineCacheData failed: ");
    return;
  }

  std::vector<u8> data(data_size);
  res = vkGetPipelineCacheData(m_device, m_pipeline_cache, &data_size, data.data());
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkGetPipelineCacheData failed: ");
    return;
  }

  // Delete the old cache and re-create. TODO: Improve this.
  File::Delete(GetPipelineDiskCacheFileName());

  // We write a single key of 1, with the entire pipeline cache data.
  LinearDiskCache<u32, u8> disk_cache;
  PipelineCacheReadIgnoreCallback callback;
  disk_cache.OpenAndRead(GetPipelineDiskCacheFileName(), callback);
  disk_cache.Append(1, data.data(), static_cast<u32>(data.size()));
  disk_cache.Close();
}

bool ObjectCache::CreatePipelineCache(bool load_from_disk)
{
  std::vector<u8> disk_data;
  if (load_from_disk)
  {
    LinearDiskCache<u32, u8> disk_cache;
    PipelineCacheReadCallback read_callback(&disk_data);
    if (disk_cache.OpenAndRead(GetPipelineDiskCacheFileName(), read_callback) != 1)
      disk_data.clear();
  }

  VkPipelineCacheCreateInfo info = {
      VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,  // VkStructureType            sType
      nullptr,                                       // const void*                pNext
      0,                                             // VkPipelineCacheCreateFlags flags
      disk_data.size(),                              // size_t                     initialDataSize
      (!disk_data.empty()) ? disk_data.data() : nullptr,  // const void*                pInitialData
  };

  VkResult res = vkCreatePipelineCache(m_device, &info, nullptr, &m_pipeline_cache);
  if (res == VK_SUCCESS)
    return true;

  // Failed to create pipeline cache, try with it empty.
  LOG_VULKAN_ERROR(res, "vkCreatePipelineCache failed, trying empty cache: ");
  info.initialDataSize = 0;
  info.pInitialData = nullptr;
  res = vkCreatePipelineCache(m_device, &info, nullptr, &m_pipeline_cache);
  if (res == VK_SUCCESS)
    return true;

  LOG_VULKAN_ERROR(res, "vkCreatePipelineCache failed: ");
  return false;
}

void ObjectCache::ClearSamplerCache()
{
  for (const auto& it : m_sampler_cache)
  {
    if (it.second != VK_NULL_HANDLE)
      vkDestroySampler(m_device, it.second, nullptr);
  }
  m_sampler_cache.clear();
}

void ObjectCache::RecompileSharedShaders()
{
  DestroySharedShaders();
  if (!CompileSharedShaders())
    PanicAlert("Failed to recompile shared shaders.");
}

bool ObjectCache::CreateDescriptorSetLayouts()
{
  static const VkDescriptorSetLayoutBinding ubo_set_bindings[] = {
      {UBO_DESCRIPTOR_SET_BINDING_VS, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
      {UBO_DESCRIPTOR_SET_BINDING_GS, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
       VK_SHADER_STAGE_GEOMETRY_BIT},
      {UBO_DESCRIPTOR_SET_BINDING_PS, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
       VK_SHADER_STAGE_FRAGMENT_BIT}};

  // Annoying these have to be split, apparently we can't partially update an array without the
  // debug layer crying.
  static const VkDescriptorSetLayoutBinding sampler_set_bindings[] = {
      {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}};

  static const VkDescriptorSetLayoutBinding ssbo_set_bindings[] = {
      {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}};

  static const VkDescriptorSetLayoutCreateInfo create_infos[NUM_DESCRIPTOR_SETS] = {
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, ARRAYSIZE(ubo_set_bindings),
       ubo_set_bindings},
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
       ARRAYSIZE(sampler_set_bindings), sampler_set_bindings},
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
       ARRAYSIZE(ssbo_set_bindings), ssbo_set_bindings}};

  for (size_t i = 0; i < NUM_DESCRIPTOR_SETS; i++)
  {
    VkResult res = vkCreateDescriptorSetLayout(m_device, &create_infos[i], nullptr,
                                               &m_descriptor_set_layouts[i]);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateDescriptorSetLayout failed: ");
      return false;
    }
  }

  return true;
}

bool ObjectCache::CreatePipelineLayout()
{
  VkResult res;

  // Descriptor sets for each pipeline layout
  VkDescriptorSetLayout standard_sets[] = {
      m_descriptor_set_layouts[DESCRIPTOR_SET_UNIFORM_BUFFERS],
      m_descriptor_set_layouts[DESCRIPTOR_SET_PIXEL_SHADER_SAMPLERS]};
  VkDescriptorSetLayout bbox_sets[] = {
      m_descriptor_set_layouts[DESCRIPTOR_SET_UNIFORM_BUFFERS],
      m_descriptor_set_layouts[DESCRIPTOR_SET_PIXEL_SHADER_SAMPLERS],
      m_descriptor_set_layouts[DESCRIPTOR_SET_SHADER_STORAGE_BUFFERS]};
  VkPushConstantRange push_constant_range = {
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128};

  // Info for each pipeline layout
  VkPipelineLayoutCreateInfo standard_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                              nullptr,
                                              0,
                                              ARRAYSIZE(standard_sets),
                                              standard_sets,
                                              0,
                                              nullptr};
  VkPipelineLayoutCreateInfo bbox_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                          nullptr,
                                          0,
                                          ARRAYSIZE(bbox_sets),
                                          bbox_sets,
                                          0,
                                          nullptr};
  VkPipelineLayoutCreateInfo push_constant_info = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                   nullptr,
                                                   0,
                                                   ARRAYSIZE(standard_sets),
                                                   standard_sets,
                                                   1,
                                                   &push_constant_range};

  if ((res = vkCreatePipelineLayout(m_device, &standard_info, nullptr,
                                    &m_standard_pipeline_layout)) != VK_SUCCESS ||
      (res = vkCreatePipelineLayout(m_device, &bbox_info, nullptr, &m_bbox_pipeline_layout)) !=
          VK_SUCCESS ||
      (res = vkCreatePipelineLayout(m_device, &push_constant_info, nullptr,
                                    &m_push_constant_pipeline_layout)))
  {
    LOG_VULKAN_ERROR(res, "vkCreatePipelineLayout failed: ");
    return false;
  }

  return true;
}

bool ObjectCache::CreateUtilityShaderVertexFormat()
{
  PortableVertexDeclaration vtx_decl = {};
  vtx_decl.position.enable = true;
  vtx_decl.position.type = VAR_FLOAT;
  vtx_decl.position.components = 4;
  vtx_decl.position.integer = false;
  vtx_decl.position.offset = offsetof(UtilityShaderVertex, Position);
  vtx_decl.texcoords[0].enable = true;
  vtx_decl.texcoords[0].type = VAR_FLOAT;
  vtx_decl.texcoords[0].components = 4;
  vtx_decl.texcoords[0].integer = false;
  vtx_decl.texcoords[0].offset = offsetof(UtilityShaderVertex, TexCoord);
  vtx_decl.colors[0].enable = true;
  vtx_decl.colors[0].type = VAR_UNSIGNED_BYTE;
  vtx_decl.colors[0].components = 4;
  vtx_decl.colors[0].integer = false;
  vtx_decl.colors[0].offset = offsetof(UtilityShaderVertex, Color);
  vtx_decl.stride = sizeof(UtilityShaderVertex);

  m_utility_shader_vertex_format = std::make_unique<VertexFormat>(vtx_decl);
  return true;
}

bool ObjectCache::CreateStaticSamplers()
{
  VkSamplerCreateInfo create_info = {
      VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,    // VkStructureType         sType
      nullptr,                                  // const void*             pNext
      0,                                        // VkSamplerCreateFlags    flags
      VK_FILTER_NEAREST,                        // VkFilter                magFilter
      VK_FILTER_NEAREST,                        // VkFilter                minFilter
      VK_SAMPLER_MIPMAP_MODE_NEAREST,           // VkSamplerMipmapMode     mipmapMode
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,  // VkSamplerAddressMode    addressModeU
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,  // VkSamplerAddressMode    addressModeV
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,    // VkSamplerAddressMode    addressModeW
      0.0f,                                     // float                   mipLodBias
      VK_FALSE,                                 // VkBool32                anisotropyEnable
      1.0f,                                     // float                   maxAnisotropy
      VK_FALSE,                                 // VkBool32                compareEnable
      VK_COMPARE_OP_ALWAYS,                     // VkCompareOp             compareOp
      std::numeric_limits<float>::min(),        // float                   minLod
      std::numeric_limits<float>::max(),        // float                   maxLod
      VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,  // VkBorderColor           borderColor
      VK_FALSE                                  // VkBool32                unnormalizedCoordinates
  };

  VkResult res = vkCreateSampler(m_device, &create_info, nullptr, &m_point_sampler);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateSampler failed: ");
    return false;
  }

  // change for linear
  create_info.minFilter = VK_FILTER_LINEAR;
  create_info.magFilter = VK_FILTER_LINEAR;
  create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  res = vkCreateSampler(m_device, &create_info, nullptr, &m_linear_sampler);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateSampler failed: ");
    return false;
  }

  return true;
}

VkSampler ObjectCache::GetSampler(const SamplerState& info)
{
  auto iter = m_sampler_cache.find(info);
  if (iter != m_sampler_cache.end())
    return iter->second;

  // Cap anisotropy to device limits.
  VkBool32 anisotropy_enable = (info.anisotropy != 0) ? VK_TRUE : VK_FALSE;
  float max_anisotropy = std::min(static_cast<float>(1 << info.anisotropy),
                                  m_device_properties.limits.maxSamplerAnisotropy);

  VkSamplerCreateInfo create_info = {
      VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,      // VkStructureType         sType
      nullptr,                                    // const void*             pNext
      0,                                          // VkSamplerCreateFlags    flags
      info.mag_filter,                            // VkFilter                magFilter
      info.min_filter,                            // VkFilter                minFilter
      info.mipmap_mode,                           // VkSamplerMipmapMode     mipmapMode
      info.wrap_u,                                // VkSamplerAddressMode    addressModeU
      info.wrap_v,                                // VkSamplerAddressMode    addressModeV
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,      // VkSamplerAddressMode    addressModeW
      static_cast<float>(info.lod_bias.Value()),  // float                   mipLodBias
      anisotropy_enable,                          // VkBool32                anisotropyEnable
      max_anisotropy,                             // float                   maxAnisotropy
      VK_FALSE,                                   // VkBool32                compareEnable
      VK_COMPARE_OP_ALWAYS,                       // VkCompareOp             compareOp
      static_cast<float>(info.min_lod.Value()),   // float                   minLod
      static_cast<float>(info.max_lod.Value()),   // float                   maxLod
      VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,    // VkBorderColor           borderColor
      VK_FALSE                                    // VkBool32                unnormalizedCoordinates
  };

  VkSampler sampler = VK_NULL_HANDLE;
  VkResult res = vkCreateSampler(m_device, &create_info, nullptr, &sampler);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkCreateSampler failed: ");

  // Store it even if it failed
  m_sampler_cache.emplace(info, sampler);
  return sampler;
}

std::string ObjectCache::GetUtilityShaderHeader() const
{
  std::stringstream ss;
  if (g_ActiveConfig.iMultisamples > 1)
  {
    ss << "#define MSAA_ENABLED 1" << std::endl;
    ss << "#define MSAA_SAMPLES " << g_ActiveConfig.iMultisamples << std::endl;
    if (g_ActiveConfig.bSSAA)
      ss << "#define SSAA_ENABLED 1" << std::endl;
  }

  u32 efb_layers = (g_ActiveConfig.iStereoMode != STEREO_OFF) ? 2 : 1;
  ss << "#define EFB_LAYERS " << efb_layers << std::endl;

  return ss.str();
}

// Comparison operators for PipelineInfos
// since these all boil down to POD types, we can just memcmp the entire thing for speed
std::size_t PipelineInfoHash::operator()(const PipelineInfo& key) const
{
  // TODO: Is this a good choice?
  return static_cast<size_t>(GetMurmurHash3(reinterpret_cast<const u8*>(&key), sizeof(key), 0));
}

bool operator==(const PipelineInfo& lhs, const PipelineInfo& rhs)
{
  return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

bool operator!=(const PipelineInfo& lhs, const PipelineInfo& rhs)
{
  return memcmp(&lhs, &rhs, sizeof(lhs)) != 0;
}

bool operator<(const PipelineInfo& lhs, const PipelineInfo& rhs)
{
  return memcmp(&lhs, &rhs, sizeof(lhs)) < 0;
}

bool operator>(const PipelineInfo& lhs, const PipelineInfo& rhs)
{
  return memcmp(&lhs, &rhs, sizeof(lhs)) > 0;
}

bool operator==(const SamplerState& lhs, const SamplerState& rhs)
{
  return lhs.hex == rhs.hex;
}

bool operator!=(const SamplerState& lhs, const SamplerState& rhs)
{
  return lhs.hex != rhs.hex;
}

bool operator>(const SamplerState& lhs, const SamplerState& rhs)
{
  return lhs.hex > rhs.hex;
}

bool operator<(const SamplerState& lhs, const SamplerState& rhs)
{
  return lhs.hex < rhs.hex;
}

bool ObjectCache::CompileSharedShaders()
{
  static const char PASSTHROUGH_VERTEX_SHADER_SOURCE[] = R"(
    layout(location = 0) in float4 ipos;
    layout(location = 5) in float4 icol0;
    layout(location = 8) in float3 itex0;

    layout(location = 0) out float3 uv0;
    layout(location = 1) out float4 col0;

    void main()
    {
	    gl_Position = ipos;
	    uv0 = itex0;
	    col0 = icol0;
    }
  )";

  static const char PASSTHROUGH_GEOMETRY_SHADER_SOURCE[] = R"(
    layout(triangles) in;
    layout(triangle_strip, max_vertices = EFB_LAYERS * 3) out;

    in VertexData
    {
	    float3 uv0;
	    float4 col0;
    } in_data[];

    out VertexData
    {
	    float3 uv0;
	    float4 col0;
    } out_data;

    void main()
    {
	    for (int j = 0; j < EFB_LAYERS; j++)
	    {
		    for (int i = 0; i < 3; i++)
		    {
			    gl_Layer = j;
			    gl_Position = gl_in[i].gl_Position;
			    out_data.uv0 = float3(in_data[i].uv0.xy, float(j));
			    out_data.col0 = in_data[i].col0;
			    EmitVertex();
		    }
		    EndPrimitive();
	    }
    }
  )";

  // TODO: Is clamp() faster than shifting right on most GPUs?
  static const char SCREEN_QUAD_VERTEX_SHADER_SOURCE[] = R"(
    layout(location = 0) out float3 uv0;

    void main()
    {
        /*
         * id	&1 &2	clamp	*2-1
         * 0	0 0	0 0	-1 -1	TL
         * 1	1 0	1 0	1 -1	TR
         * 2	0 2	0 1	-1 1	BL
         * 3	1 2	1 1	1 1	BR
         */
        vec2 rawpos = float2(float(gl_VertexID & 1), clamp(float(gl_VertexID & 2), 0.0f, 1.0f));
        gl_Position = float4(rawpos * 2.0f - 1.0f, 0.0f, 1.0f);
        uv0 = float3(rawpos, 0.0f);
    }
  )";

  static const char SCREEN_QUAD_GEOMETRY_SHADER_SOURCE[] = R"(
    layout(triangles) in;
    layout(triangle_strip, max_vertices = EFB_LAYERS * 3) out;

    in VertexData
    {
	    float3 uv0;
    } in_data[];

    out VertexData
    {
	    float3 uv0;
    } out_data;

    void main()
    {
	    for (int j = 0; j < EFB_LAYERS; j++)
	    {
		    for (int i = 0; i < 3; i++)
		    {
			    gl_Layer = j;
			    gl_Position = gl_in[i].gl_Position;
			    out_data.uv0 = float3(in_data[i].uv0.xy, float(j));
			    EmitVertex();
		    }
		    EndPrimitive();
	    }
    }
  )";

  std::string header = GetUtilityShaderHeader();

  m_screen_quad_vertex_shader =
      m_vs_cache.CompileAndCreateShader(header + SCREEN_QUAD_VERTEX_SHADER_SOURCE);
  m_passthrough_vertex_shader =
      m_vs_cache.CompileAndCreateShader(header + PASSTHROUGH_VERTEX_SHADER_SOURCE);
  if (m_screen_quad_vertex_shader == VK_NULL_HANDLE ||
      m_passthrough_vertex_shader == VK_NULL_HANDLE)
  {
    return false;
  }

  if (g_ActiveConfig.iStereoMode != STEREO_OFF && SupportsGeometryShaders())
  {
    m_screen_quad_geometry_shader =
        m_gs_cache.CompileAndCreateShader(header + SCREEN_QUAD_GEOMETRY_SHADER_SOURCE);
    m_passthrough_geometry_shader =
        m_gs_cache.CompileAndCreateShader(header + PASSTHROUGH_GEOMETRY_SHADER_SOURCE);
    if (m_screen_quad_geometry_shader == VK_NULL_HANDLE ||
        m_passthrough_geometry_shader == VK_NULL_HANDLE)
    {
      return false;
    }
  }

  return true;
}

void ObjectCache::DestroySharedShaders()
{
  auto DestroyShader = [this](VkShaderModule& shader) {
    if (shader != VK_NULL_HANDLE)
    {
      vkDestroyShaderModule(m_device, shader, nullptr);
      shader = VK_NULL_HANDLE;
    }
  };

  DestroyShader(m_screen_quad_vertex_shader);
  DestroyShader(m_passthrough_vertex_shader);
  DestroyShader(m_screen_quad_geometry_shader);
  DestroyShader(m_passthrough_geometry_shader);
}
}
