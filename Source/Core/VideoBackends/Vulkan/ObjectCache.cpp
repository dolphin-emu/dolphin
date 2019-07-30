// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/ObjectCache.h"

#include <algorithm>
#include <array>
#include <type_traits>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/LinearDiskCache.h"
#include "Common/MsgHandler.h"

#include "Core/ConfigManager.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ShaderCompiler.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VertexFormat.h"
#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoCommon/Statistics.h"

namespace Vulkan
{
std::unique_ptr<ObjectCache> g_object_cache;

ObjectCache::ObjectCache() = default;

ObjectCache::~ObjectCache()
{
  DestroyPipelineCache();
  DestroySamplers();
  DestroyPipelineLayouts();
  DestroyDescriptorSetLayouts();
  DestroyRenderPassCache();
}

bool ObjectCache::Initialize()
{
  if (!CreateDescriptorSetLayouts())
    return false;

  if (!CreatePipelineLayouts())
    return false;

  if (!CreateStaticSamplers())
    return false;

  m_texture_upload_buffer =
      StreamBuffer::Create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, TEXTURE_UPLOAD_BUFFER_SIZE);
  if (!m_texture_upload_buffer)
  {
    PanicAlert("Failed to create texture upload buffer");
    return false;
  }

  if (g_ActiveConfig.bShaderCache)
  {
    if (!LoadPipelineCache())
      return false;
  }
  else
  {
    if (!CreatePipelineCache())
      return false;
  }

  return true;
}

void ObjectCache::Shutdown()
{
  if (g_ActiveConfig.bShaderCache && m_pipeline_cache != VK_NULL_HANDLE)
    SavePipelineCache();
}

void ObjectCache::ClearSamplerCache()
{
  for (const auto& it : m_sampler_cache)
  {
    if (it.second != VK_NULL_HANDLE)
      vkDestroySampler(g_vulkan_context->GetDevice(), it.second, nullptr);
  }
  m_sampler_cache.clear();
}

void ObjectCache::DestroySamplers()
{
  ClearSamplerCache();

  if (m_point_sampler != VK_NULL_HANDLE)
  {
    vkDestroySampler(g_vulkan_context->GetDevice(), m_point_sampler, nullptr);
    m_point_sampler = VK_NULL_HANDLE;
  }

  if (m_linear_sampler != VK_NULL_HANDLE)
  {
    vkDestroySampler(g_vulkan_context->GetDevice(), m_linear_sampler, nullptr);
    m_linear_sampler = VK_NULL_HANDLE;
  }
}

bool ObjectCache::CreateDescriptorSetLayouts()
{
  // The geometry shader buffer must be last in this binding set, as we don't include it
  // if geometry shaders are not supported by the device. See the decrement below.
  static const std::array<VkDescriptorSetLayoutBinding, 3> standard_ubo_bindings{{
      {UBO_DESCRIPTOR_SET_BINDING_PS, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
       VK_SHADER_STAGE_FRAGMENT_BIT},
      {UBO_DESCRIPTOR_SET_BINDING_VS, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
      {UBO_DESCRIPTOR_SET_BINDING_GS, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
       VK_SHADER_STAGE_GEOMETRY_BIT},
  }};

  static const std::array<VkDescriptorSetLayoutBinding, 1> standard_sampler_bindings{{
      {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<u32>(NUM_PIXEL_SHADER_SAMPLERS),
       VK_SHADER_STAGE_FRAGMENT_BIT},
  }};

  static const std::array<VkDescriptorSetLayoutBinding, 1> standard_ssbo_bindings{{
      {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
  }};

  static const std::array<VkDescriptorSetLayoutBinding, 1> utility_ubo_bindings{{
      {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
  }};

  // Utility samplers aren't dynamically indexed.
  static const std::array<VkDescriptorSetLayoutBinding, 9> utility_sampler_bindings{{
      {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {8, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
  }};

  static const std::array<VkDescriptorSetLayoutBinding, 6> compute_set_bindings{{
      {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {3, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {4, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
  }};

  std::array<VkDescriptorSetLayoutCreateInfo, NUM_DESCRIPTOR_SET_LAYOUTS> create_infos{{
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(standard_ubo_bindings.size()), standard_ubo_bindings.data()},
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(standard_sampler_bindings.size()), standard_sampler_bindings.data()},
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(standard_ssbo_bindings.size()), standard_ssbo_bindings.data()},
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(utility_ubo_bindings.size()), utility_ubo_bindings.data()},
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(utility_sampler_bindings.size()), utility_sampler_bindings.data()},
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(compute_set_bindings.size()), compute_set_bindings.data()},
  }};

  // Don't set the GS bit if geometry shaders aren't available.
  if (!g_ActiveConfig.backend_info.bSupportsGeometryShaders)
    create_infos[DESCRIPTOR_SET_LAYOUT_STANDARD_UNIFORM_BUFFERS].bindingCount--;

  for (size_t i = 0; i < create_infos.size(); i++)
  {
    VkResult res = vkCreateDescriptorSetLayout(g_vulkan_context->GetDevice(), &create_infos[i],
                                               nullptr, &m_descriptor_set_layouts[i]);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateDescriptorSetLayout failed: ");
      return false;
    }
  }

  return true;
}

void ObjectCache::DestroyDescriptorSetLayouts()
{
  for (VkDescriptorSetLayout layout : m_descriptor_set_layouts)
  {
    if (layout != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(g_vulkan_context->GetDevice(), layout, nullptr);
  }
}

bool ObjectCache::CreatePipelineLayouts()
{
  // Descriptor sets for each pipeline layout.
  // In the standard set, the SSBO must be the last descriptor, as we do not include it
  // when fragment stores and atomics are not supported by the device.
  const std::array<VkDescriptorSetLayout, 3> standard_sets{
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_STANDARD_UNIFORM_BUFFERS],
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_STANDARD_SAMPLERS],
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_STANDARD_SHADER_STORAGE_BUFFERS],
  };
  const std::array<VkDescriptorSetLayout, 2> utility_sets{
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_UTILITY_UNIFORM_BUFFER],
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_UTILITY_SAMPLERS],
  };
  const std::array<VkDescriptorSetLayout, 1> compute_sets{
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_COMPUTE],
  };

  // Info for each pipeline layout
  std::array<VkPipelineLayoutCreateInfo, NUM_PIPELINE_LAYOUTS> pipeline_layout_info{{
      // Standard
      {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(standard_sets.size()), standard_sets.data(), 0, nullptr},

      // Utility
      {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(utility_sets.size()), utility_sets.data(), 0, nullptr},

      // Compute
      {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(compute_sets.size()), compute_sets.data(), 0, nullptr},
  }};

  // If bounding box is unsupported, don't bother with the SSBO descriptor set.
  if (!g_ActiveConfig.backend_info.bSupportsBBox)
    pipeline_layout_info[PIPELINE_LAYOUT_STANDARD].setLayoutCount--;

  for (size_t i = 0; i < pipeline_layout_info.size(); i++)
  {
    VkResult res;
    if ((res = vkCreatePipelineLayout(g_vulkan_context->GetDevice(), &pipeline_layout_info[i],
                                      nullptr, &m_pipeline_layouts[i])) != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreatePipelineLayout failed: ");
      return false;
    }
  }

  return true;
}

void ObjectCache::DestroyPipelineLayouts()
{
  for (VkPipelineLayout layout : m_pipeline_layouts)
  {
    if (layout != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(g_vulkan_context->GetDevice(), layout, nullptr);
  }
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

  VkResult res =
      vkCreateSampler(g_vulkan_context->GetDevice(), &create_info, nullptr, &m_point_sampler);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateSampler failed: ");
    return false;
  }

  // Most fields are shared across point<->linear samplers, so only change those necessary.
  create_info.minFilter = VK_FILTER_LINEAR;
  create_info.magFilter = VK_FILTER_LINEAR;
  create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  res = vkCreateSampler(g_vulkan_context->GetDevice(), &create_info, nullptr, &m_linear_sampler);
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

  static constexpr std::array<VkFilter, 4> filters = {{VK_FILTER_NEAREST, VK_FILTER_LINEAR}};
  static constexpr std::array<VkSamplerMipmapMode, 2> mipmap_modes = {
      {VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR}};
  static constexpr std::array<VkSamplerAddressMode, 4> address_modes = {
      {VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_REPEAT,
       VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT}};

  VkSamplerCreateInfo create_info = {
      VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,               // VkStructureType         sType
      nullptr,                                             // const void*             pNext
      0,                                                   // VkSamplerCreateFlags    flags
      filters[static_cast<u32>(info.mag_filter.Value())],  // VkFilter                magFilter
      filters[static_cast<u32>(info.min_filter.Value())],  // VkFilter                minFilter
      mipmap_modes[static_cast<u32>(info.mipmap_filter.Value())],  // VkSamplerMipmapMode mipmapMode
      address_modes[static_cast<u32>(info.wrap_u.Value())],  // VkSamplerAddressMode    addressModeU
      address_modes[static_cast<u32>(info.wrap_v.Value())],  // VkSamplerAddressMode    addressModeV
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,                 // VkSamplerAddressMode    addressModeW
      info.lod_bias / 256.0f,                                // float                   mipLodBias
      VK_FALSE,                                 // VkBool32                anisotropyEnable
      0.0f,                                     // float                   maxAnisotropy
      VK_FALSE,                                 // VkBool32                compareEnable
      VK_COMPARE_OP_ALWAYS,                     // VkCompareOp             compareOp
      info.min_lod / 16.0f,                     // float                   minLod
      info.max_lod / 16.0f,                     // float                   maxLod
      VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,  // VkBorderColor           borderColor
      VK_FALSE                                  // VkBool32                unnormalizedCoordinates
  };

  // Can we use anisotropic filtering with this sampler?
  if (g_ActiveConfig.iMaxAnisotropy > 0 &&
      info.anisotropic_filtering && g_vulkan_context->SupportsAnisotropicFiltering())
  {
    // Cap anisotropy to device limits.
    create_info.anisotropyEnable = VK_TRUE;
    create_info.maxAnisotropy = std::min(static_cast<float>(1 << g_ActiveConfig.iMaxAnisotropy),
                                         g_vulkan_context->GetMaxSamplerAnisotropy());
  }

  VkSampler sampler = VK_NULL_HANDLE;
  VkResult res = vkCreateSampler(g_vulkan_context->GetDevice(), &create_info, nullptr, &sampler);
  if (res != VK_SUCCESS)
    LOG_VULKAN_ERROR(res, "vkCreateSampler failed: ");

  // Store it even if it failed
  m_sampler_cache.emplace(info, sampler);
  return sampler;
}

VkRenderPass ObjectCache::GetRenderPass(VkFormat color_format, VkFormat depth_format,
                                        u32 multisamples, VkAttachmentLoadOp load_op)
{
  auto key = std::tie(color_format, depth_format, multisamples, load_op);
  auto it = m_render_pass_cache.find(key);
  if (it != m_render_pass_cache.end())
    return it->second;

  VkAttachmentReference color_reference;
  VkAttachmentReference* color_reference_ptr = nullptr;
  VkAttachmentReference depth_reference;
  VkAttachmentReference* depth_reference_ptr = nullptr;
  std::array<VkAttachmentDescription, 2> attachments;
  u32 num_attachments = 0;
  if (color_format != VK_FORMAT_UNDEFINED)
  {
    attachments[num_attachments] = {0,
                                    color_format,
                                    static_cast<VkSampleCountFlagBits>(multisamples),
                                    load_op,
                                    VK_ATTACHMENT_STORE_OP_STORE,
                                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    color_reference.attachment = num_attachments;
    color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_reference_ptr = &color_reference;
    num_attachments++;
  }
  if (depth_format != VK_FORMAT_UNDEFINED)
  {
    attachments[num_attachments] = {0,
                                    depth_format,
                                    static_cast<VkSampleCountFlagBits>(multisamples),
                                    load_op,
                                    VK_ATTACHMENT_STORE_OP_STORE,
                                    VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                    VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    depth_reference.attachment = num_attachments;
    depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_reference_ptr = &depth_reference;
    num_attachments++;
  }

  VkSubpassDescription subpass = {0,
                                  VK_PIPELINE_BIND_POINT_GRAPHICS,
                                  0,
                                  nullptr,
                                  color_reference_ptr ? 1u : 0u,
                                  color_reference_ptr ? color_reference_ptr : nullptr,
                                  nullptr,
                                  depth_reference_ptr,
                                  0,
                                  nullptr};
  VkRenderPassCreateInfo pass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                      nullptr,
                                      0,
                                      num_attachments,
                                      attachments.data(),
                                      1,
                                      &subpass,
                                      0,
                                      nullptr};

  VkRenderPass pass;
  VkResult res = vkCreateRenderPass(g_vulkan_context->GetDevice(), &pass_info, nullptr, &pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass failed: ");
    return VK_NULL_HANDLE;
  }

  m_render_pass_cache.emplace(key, pass);
  return pass;
}

void ObjectCache::DestroyRenderPassCache()
{
  for (auto& it : m_render_pass_cache)
    vkDestroyRenderPass(g_vulkan_context->GetDevice(), it.second, nullptr);
  m_render_pass_cache.clear();
}

class PipelineCacheReadCallback : public LinearDiskCacheReader<u32, u8>
{
public:
  PipelineCacheReadCallback(std::vector<u8>* data) : m_data(data) {}
  void Read(const u32& key, const u8* value, u32 value_size) override
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
  void Read(const u32& key, const u8* value, u32 value_size) override {}
};

bool ObjectCache::CreatePipelineCache()
{
  // Vulkan pipeline caches can be shared between games for shader compile time reduction.
  // This assumes that drivers don't create all pipelines in the cache on load time, only
  // when a lookup occurs that matches a pipeline (or pipeline data) in the cache.
  m_pipeline_cache_filename = GetDiskShaderCacheFileName(APIType::Vulkan, "Pipeline", false, true);

  VkPipelineCacheCreateInfo info = {
      VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,  // VkStructureType            sType
      nullptr,                                       // const void*                pNext
      0,                                             // VkPipelineCacheCreateFlags flags
      0,                                             // size_t                     initialDataSize
      nullptr                                        // const void*                pInitialData
  };

  VkResult res =
      vkCreatePipelineCache(g_vulkan_context->GetDevice(), &info, nullptr, &m_pipeline_cache);
  if (res == VK_SUCCESS)
    return true;

  LOG_VULKAN_ERROR(res, "vkCreatePipelineCache failed: ");
  return false;
}

bool ObjectCache::LoadPipelineCache()
{
  // We have to keep the pipeline cache file name around since when we save it
  // we delete the old one, by which time the game's unique ID is already cleared.
  m_pipeline_cache_filename = GetDiskShaderCacheFileName(APIType::Vulkan, "Pipeline", false, true);

  std::vector<u8> disk_data;
  LinearDiskCache<u32, u8> disk_cache;
  PipelineCacheReadCallback read_callback(&disk_data);
  if (disk_cache.OpenAndRead(m_pipeline_cache_filename, read_callback) != 1)
    disk_data.clear();

  if (!disk_data.empty() && !ValidatePipelineCache(disk_data.data(), disk_data.size()))
  {
    // Don't use this data. In fact, we should delete it to prevent it from being used next time.
    File::Delete(m_pipeline_cache_filename);
    return CreatePipelineCache();
  }

  VkPipelineCacheCreateInfo info = {
      VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,  // VkStructureType            sType
      nullptr,                                       // const void*                pNext
      0,                                             // VkPipelineCacheCreateFlags flags
      disk_data.size(),                              // size_t                     initialDataSize
      disk_data.data()                               // const void*                pInitialData
  };

  VkResult res =
      vkCreatePipelineCache(g_vulkan_context->GetDevice(), &info, nullptr, &m_pipeline_cache);
  if (res == VK_SUCCESS)
    return true;

  // Failed to create pipeline cache, try with it empty.
  LOG_VULKAN_ERROR(res, "vkCreatePipelineCache failed, trying empty cache: ");
  return CreatePipelineCache();
}

// Based on Vulkan 1.0 specification,
// Table 9.1. Layout for pipeline cache header version VK_PIPELINE_CACHE_HEADER_VERSION_ONE
// NOTE: This data is assumed to be in little-endian format.
#pragma pack(push, 4)
struct VK_PIPELINE_CACHE_HEADER
{
  u32 header_length;
  u32 header_version;
  u32 vendor_id;
  u32 device_id;
  u8 uuid[VK_UUID_SIZE];
};
#pragma pack(pop)
static_assert(std::is_trivially_copyable<VK_PIPELINE_CACHE_HEADER>::value,
              "VK_PIPELINE_CACHE_HEADER must be trivially copyable");

bool ObjectCache::ValidatePipelineCache(const u8* data, size_t data_length)
{
  if (data_length < sizeof(VK_PIPELINE_CACHE_HEADER))
  {
    ERROR_LOG(VIDEO, "Pipeline cache failed validation: Invalid header");
    return false;
  }

  VK_PIPELINE_CACHE_HEADER header;
  std::memcpy(&header, data, sizeof(header));
  if (header.header_length < sizeof(VK_PIPELINE_CACHE_HEADER))
  {
    ERROR_LOG(VIDEO, "Pipeline cache failed validation: Invalid header length");
    return false;
  }

  if (header.header_version != VK_PIPELINE_CACHE_HEADER_VERSION_ONE)
  {
    ERROR_LOG(VIDEO, "Pipeline cache failed validation: Invalid header version");
    return false;
  }

  if (header.vendor_id != g_vulkan_context->GetDeviceProperties().vendorID)
  {
    ERROR_LOG(VIDEO,
              "Pipeline cache failed validation: Incorrect vendor ID (file: 0x%X, device: 0x%X)",
              header.vendor_id, g_vulkan_context->GetDeviceProperties().vendorID);
    return false;
  }

  if (header.device_id != g_vulkan_context->GetDeviceProperties().deviceID)
  {
    ERROR_LOG(VIDEO,
              "Pipeline cache failed validation: Incorrect device ID (file: 0x%X, device: 0x%X)",
              header.device_id, g_vulkan_context->GetDeviceProperties().deviceID);
    return false;
  }

  if (std::memcmp(header.uuid, g_vulkan_context->GetDeviceProperties().pipelineCacheUUID,
                  VK_UUID_SIZE) != 0)
  {
    ERROR_LOG(VIDEO, "Pipeline cache failed validation: Incorrect UUID");
    return false;
  }

  return true;
}

void ObjectCache::DestroyPipelineCache()
{
  vkDestroyPipelineCache(g_vulkan_context->GetDevice(), m_pipeline_cache, nullptr);
  m_pipeline_cache = VK_NULL_HANDLE;
}

void ObjectCache::SavePipelineCache()
{
  size_t data_size;
  VkResult res =
      vkGetPipelineCacheData(g_vulkan_context->GetDevice(), m_pipeline_cache, &data_size, nullptr);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkGetPipelineCacheData failed: ");
    return;
  }

  std::vector<u8> data(data_size);
  res = vkGetPipelineCacheData(g_vulkan_context->GetDevice(), m_pipeline_cache, &data_size,
                               data.data());
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkGetPipelineCacheData failed: ");
    return;
  }

  // Delete the old cache and re-create.
  File::Delete(m_pipeline_cache_filename);

  // We write a single key of 1, with the entire pipeline cache data.
  // Not ideal, but our disk cache class does not support just writing a single blob
  // of data without specifying a key.
  LinearDiskCache<u32, u8> disk_cache;
  PipelineCacheReadIgnoreCallback callback;
  disk_cache.OpenAndRead(m_pipeline_cache_filename, callback);
  disk_cache.Append(1, data.data(), static_cast<u32>(data.size()));
  disk_cache.Close();
}

void ObjectCache::ReloadPipelineCache()
{
  SavePipelineCache();

  if (g_ActiveConfig.bShaderCache)
    LoadPipelineCache();
  else
    CreatePipelineCache();
}
}  // namespace Vulkan
