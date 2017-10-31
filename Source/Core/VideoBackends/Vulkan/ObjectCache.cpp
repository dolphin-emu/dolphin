// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/ObjectCache.h"

#include <algorithm>
#include <sstream>
#include <type_traits>
#include <xxhash.h>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/LinearDiskCache.h"
#include "Common/MsgHandler.h"

#include "Core/ConfigManager.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ShaderCompiler.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VertexFormat.h"
#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoCommon/Statistics.h"

namespace Vulkan
{
std::unique_ptr<ObjectCache> g_object_cache;

ObjectCache::ObjectCache()
{
}

ObjectCache::~ObjectCache()
{
  DestroySamplers();
  DestroyPipelineLayouts();
  DestroyDescriptorSetLayouts();
}

bool ObjectCache::Initialize()
{
  if (!CreateDescriptorSetLayouts())
    return false;

  if (!CreatePipelineLayouts())
    return false;

  if (!CreateUtilityShaderVertexFormat())
    return false;

  if (!CreateStaticSamplers())
    return false;

  m_utility_shader_vertex_buffer =
      StreamBuffer::Create(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 1024 * 1024, 4 * 1024 * 1024);
  m_utility_shader_uniform_buffer =
      StreamBuffer::Create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 1024, 4 * 1024 * 1024);
  if (!m_utility_shader_vertex_buffer || !m_utility_shader_uniform_buffer)
    return false;

  m_dummy_texture = Texture2D::Create(1, 1, 1, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                                      VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_TILING_OPTIMAL,
                                      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
  m_dummy_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  VkClearColorValue clear_color = {};
  VkImageSubresourceRange clear_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  vkCmdClearColorImage(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                       m_dummy_texture->GetImage(), m_dummy_texture->GetLayout(), &clear_color, 1,
                       &clear_range);
  m_dummy_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentInitCommandBuffer(),
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  return true;
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
  static const VkDescriptorSetLayoutBinding ubo_set_bindings[] = {
      {UBO_DESCRIPTOR_SET_BINDING_PS, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
       VK_SHADER_STAGE_FRAGMENT_BIT},
      {UBO_DESCRIPTOR_SET_BINDING_VS, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
      {UBO_DESCRIPTOR_SET_BINDING_GS, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
       VK_SHADER_STAGE_GEOMETRY_BIT}};

  static const VkDescriptorSetLayoutBinding sampler_set_bindings[] = {
      {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<u32>(NUM_PIXEL_SHADER_SAMPLERS),
       VK_SHADER_STAGE_FRAGMENT_BIT}};

  static const VkDescriptorSetLayoutBinding ssbo_set_bindings[] = {
      {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}};

  static const VkDescriptorSetLayoutBinding texel_buffer_set_bindings[] = {
      {0, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
  };

  static const VkDescriptorSetLayoutBinding compute_set_bindings[] = {
      {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {5, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {6, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
      {7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
  };

  static const VkDescriptorSetLayoutCreateInfo create_infos[NUM_DESCRIPTOR_SET_LAYOUTS] = {
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(ArraySize(ubo_set_bindings)), ubo_set_bindings},
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(ArraySize(sampler_set_bindings)), sampler_set_bindings},
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(ArraySize(ssbo_set_bindings)), ssbo_set_bindings},
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(ArraySize(texel_buffer_set_bindings)), texel_buffer_set_bindings},
      {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(ArraySize(compute_set_bindings)), compute_set_bindings}};

  for (size_t i = 0; i < NUM_DESCRIPTOR_SET_LAYOUTS; i++)
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
  VkResult res;

  // Descriptor sets for each pipeline layout
  VkDescriptorSetLayout standard_sets[] = {
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_UNIFORM_BUFFERS],
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_PIXEL_SHADER_SAMPLERS]};
  VkDescriptorSetLayout bbox_sets[] = {
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_UNIFORM_BUFFERS],
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_PIXEL_SHADER_SAMPLERS],
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_SHADER_STORAGE_BUFFERS]};
  VkDescriptorSetLayout texture_conversion_sets[] = {
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_UNIFORM_BUFFERS],
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_PIXEL_SHADER_SAMPLERS],
      m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_TEXEL_BUFFERS]};
  VkDescriptorSetLayout compute_sets[] = {m_descriptor_set_layouts[DESCRIPTOR_SET_LAYOUT_COMPUTE]};
  VkPushConstantRange push_constant_range = {
      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, PUSH_CONSTANT_BUFFER_SIZE};
  VkPushConstantRange compute_push_constant_range = {VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                                     PUSH_CONSTANT_BUFFER_SIZE};

  // Info for each pipeline layout
  VkPipelineLayoutCreateInfo pipeline_layout_info[NUM_PIPELINE_LAYOUTS] = {
      // Standard
      {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(ArraySize(standard_sets)), standard_sets, 0, nullptr},

      // BBox
      {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(ArraySize(bbox_sets)), bbox_sets, 0, nullptr},

      // Push Constant
      {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(ArraySize(standard_sets)), standard_sets, 1, &push_constant_range},

      // Texture Conversion
      {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(ArraySize(texture_conversion_sets)), texture_conversion_sets, 1,
       &push_constant_range},

      // Compute
      {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0,
       static_cast<u32>(ArraySize(compute_sets)), compute_sets, 1, &compute_push_constant_range}};

  for (size_t i = 0; i < NUM_PIPELINE_LAYOUTS; i++)
  {
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
  if (info.anisotropic_filtering && g_vulkan_context->SupportsAnisotropicFiltering())
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
}
