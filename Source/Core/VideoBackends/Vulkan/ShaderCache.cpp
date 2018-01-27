// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/ShaderCache.h"

#include <algorithm>
#include <sstream>
#include <type_traits>
#include <xxhash.h>

#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/LinearDiskCache.h"
#include "Common/MsgHandler.h"

#include "Core/ConfigManager.h"
#include "Core/Host.h"

#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ShaderCompiler.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VertexFormat.h"
#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoCommon/AsyncShaderCompiler.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/UberShaderPixel.h"
#include "VideoCommon/UberShaderVertex.h"
#include "VideoCommon/VertexLoaderManager.h"

namespace Vulkan
{
std::unique_ptr<ShaderCache> g_shader_cache;

ShaderCache::ShaderCache()
{
}

ShaderCache::~ShaderCache()
{
  DestroyPipelineCache();
  DestroyShaderCaches();
  DestroySharedShaders();
}

bool ShaderCache::Initialize()
{
  if (g_ActiveConfig.bShaderCache)
  {
    LoadShaderCaches();
    if (!LoadPipelineCache())
      return false;
  }
  else
  {
    if (!CreatePipelineCache())
      return false;
  }

  if (!CompileSharedShaders())
    return false;

  m_async_shader_compiler = std::make_unique<VideoCommon::AsyncShaderCompiler>();
  m_async_shader_compiler->ResizeWorkerThreads(g_ActiveConfig.CanPrecompileUberShaders() ?
                                                   g_ActiveConfig.GetShaderPrecompilerThreads() :
                                                   g_ActiveConfig.GetShaderCompilerThreads());
  return true;
}

void ShaderCache::Shutdown()
{
  if (m_async_shader_compiler)
  {
    m_async_shader_compiler->StopWorkerThreads();
    m_async_shader_compiler->RetrieveWorkItems();
  }
}

static bool IsStripPrimitiveTopology(VkPrimitiveTopology topology)
{
  return topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP ||
         topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP ||
         topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY ||
         topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
}

static VkPipelineRasterizationStateCreateInfo
GetVulkanRasterizationState(const RasterizationState& state)
{
  static constexpr std::array<VkCullModeFlags, 4> cull_modes = {
      {VK_CULL_MODE_NONE, VK_CULL_MODE_BACK_BIT, VK_CULL_MODE_FRONT_BIT,
       VK_CULL_MODE_FRONT_AND_BACK}};

  bool depth_clamp = g_ActiveConfig.backend_info.bSupportsDepthClamp;

  return {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,               // const void*                               pNext
      0,                     // VkPipelineRasterizationStateCreateFlags   flags
      depth_clamp,           // VkBool32                                  depthClampEnable
      VK_FALSE,              // VkBool32                                  rasterizerDiscardEnable
      VK_POLYGON_MODE_FILL,  // VkPolygonMode                             polygonMode
      cull_modes[state.cullmode],  // VkCullModeFlags                           cullMode
      VK_FRONT_FACE_CLOCKWISE,     // VkFrontFace                               frontFace
      VK_FALSE,                    // VkBool32                                  depthBiasEnable
      0.0f,  // float                                     depthBiasConstantFactor
      0.0f,  // float                                     depthBiasClamp
      0.0f,  // float                                     depthBiasSlopeFactor
      1.0f   // float                                     lineWidth
  };
}

static VkPipelineMultisampleStateCreateInfo
GetVulkanMultisampleState(const MultisamplingState& state)
{
  return {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,  // const void*                              pNext
      0,        // VkPipelineMultisampleStateCreateFlags    flags
      static_cast<VkSampleCountFlagBits>(
          state.samples.Value()),  // VkSampleCountFlagBits                    rasterizationSamples
      state.per_sample_shading,    // VkBool32                                 sampleShadingEnable
      1.0f,                        // float                                    minSampleShading
      nullptr,                     // const VkSampleMask*                      pSampleMask;
      VK_FALSE,                    // VkBool32                                 alphaToCoverageEnable
      VK_FALSE                     // VkBool32                                 alphaToOneEnable
  };
}

static VkPipelineDepthStencilStateCreateInfo GetVulkanDepthStencilState(const DepthState& state)
{
  // Less/greater are swapped due to inverted depth.
  static constexpr std::array<VkCompareOp, 8> funcs = {
      {VK_COMPARE_OP_NEVER, VK_COMPARE_OP_GREATER, VK_COMPARE_OP_EQUAL,
       VK_COMPARE_OP_GREATER_OR_EQUAL, VK_COMPARE_OP_LESS, VK_COMPARE_OP_NOT_EQUAL,
       VK_COMPARE_OP_LESS_OR_EQUAL, VK_COMPARE_OP_ALWAYS}};

  return {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,             // const void*                               pNext
      0,                   // VkPipelineDepthStencilStateCreateFlags    flags
      state.testenable,    // VkBool32                                  depthTestEnable
      state.updateenable,  // VkBool32                                  depthWriteEnable
      funcs[state.func],   // VkCompareOp                               depthCompareOp
      VK_FALSE,            // VkBool32                                  depthBoundsTestEnable
      VK_FALSE,            // VkBool32                                  stencilTestEnable
      {},                  // VkStencilOpState                          front
      {},                  // VkStencilOpState                          back
      0.0f,                // float                                     minDepthBounds
      1.0f                 // float                                     maxDepthBounds
  };
}

static VkPipelineColorBlendAttachmentState GetVulkanAttachmentBlendState(const BlendingState& state)
{
  VkPipelineColorBlendAttachmentState vk_state = {};
  vk_state.blendEnable = static_cast<VkBool32>(state.blendenable);
  vk_state.colorBlendOp = state.subtract ? VK_BLEND_OP_REVERSE_SUBTRACT : VK_BLEND_OP_ADD;
  vk_state.alphaBlendOp = state.subtractAlpha ? VK_BLEND_OP_REVERSE_SUBTRACT : VK_BLEND_OP_ADD;

  if (state.usedualsrc && g_vulkan_context->SupportsDualSourceBlend())
  {
    static constexpr std::array<VkBlendFactor, 8> src_factors = {
        {VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_DST_COLOR,
         VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_SRC1_ALPHA,
         VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA, VK_BLEND_FACTOR_DST_ALPHA,
         VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA}};
    static constexpr std::array<VkBlendFactor, 8> dst_factors = {
        {VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_COLOR,
         VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, VK_BLEND_FACTOR_SRC1_ALPHA,
         VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA, VK_BLEND_FACTOR_DST_ALPHA,
         VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA}};

    vk_state.srcColorBlendFactor = src_factors[state.srcfactor];
    vk_state.srcAlphaBlendFactor = src_factors[state.srcfactoralpha];
    vk_state.dstColorBlendFactor = dst_factors[state.dstfactor];
    vk_state.dstAlphaBlendFactor = dst_factors[state.dstfactoralpha];
  }
  else
  {
    static constexpr std::array<VkBlendFactor, 8> src_factors = {
        {VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_DST_COLOR,
         VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR, VK_BLEND_FACTOR_SRC_ALPHA,
         VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_ALPHA,
         VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA}};

    static constexpr std::array<VkBlendFactor, 8> dst_factors = {
        {VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_SRC_COLOR,
         VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR, VK_BLEND_FACTOR_SRC_ALPHA,
         VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_DST_ALPHA,
         VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA}};

    vk_state.srcColorBlendFactor = src_factors[state.srcfactor];
    vk_state.srcAlphaBlendFactor = src_factors[state.srcfactoralpha];
    vk_state.dstColorBlendFactor = dst_factors[state.dstfactor];
    vk_state.dstAlphaBlendFactor = dst_factors[state.dstfactoralpha];
  }

  if (state.colorupdate)
  {
    vk_state.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
  }
  else
  {
    vk_state.colorWriteMask = 0;
  }

  if (state.alphaupdate)
    vk_state.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;

  return vk_state;
}

static VkPipelineColorBlendStateCreateInfo
GetVulkanColorBlendState(const BlendingState& state,
                         const VkPipelineColorBlendAttachmentState* attachments,
                         uint32_t num_attachments)
{
  static constexpr std::array<VkLogicOp, 16> vk_logic_ops = {
      {VK_LOGIC_OP_CLEAR, VK_LOGIC_OP_AND, VK_LOGIC_OP_AND_REVERSE, VK_LOGIC_OP_COPY,
       VK_LOGIC_OP_AND_INVERTED, VK_LOGIC_OP_NO_OP, VK_LOGIC_OP_XOR, VK_LOGIC_OP_OR,
       VK_LOGIC_OP_NOR, VK_LOGIC_OP_EQUIVALENT, VK_LOGIC_OP_INVERT, VK_LOGIC_OP_OR_REVERSE,
       VK_LOGIC_OP_COPY_INVERTED, VK_LOGIC_OP_OR_INVERTED, VK_LOGIC_OP_NAND, VK_LOGIC_OP_SET}};

  VkBool32 vk_logic_op_enable = static_cast<VkBool32>(state.logicopenable);
  if (vk_logic_op_enable && !g_vulkan_context->SupportsLogicOps())
  {
    // At the time of writing, Adreno and Mali drivers didn't support logic ops.
    // The "emulation" through blending path has been removed, so just disable it completely.
    // These drivers don't support dual-source blend either, so issues are to be expected.
    vk_logic_op_enable = VK_FALSE;
  }

  VkLogicOp vk_logic_op = vk_logic_op_enable ? vk_logic_ops[state.logicmode] : VK_LOGIC_OP_CLEAR;

  VkPipelineColorBlendStateCreateInfo vk_state = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,                  // const void*                                   pNext
      0,                        // VkPipelineColorBlendStateCreateFlags          flags
      vk_logic_op_enable,       // VkBool32                                      logicOpEnable
      vk_logic_op,              // VkLogicOp                                     logicOp
      num_attachments,          // uint32_t                                      attachmentCount
      attachments,              // const VkPipelineColorBlendAttachmentState*    pAttachments
      {1.0f, 1.0f, 1.0f, 1.0f}  // float                                         blendConstants[4]
  };

  return vk_state;
}

VkPipeline ShaderCache::CreatePipeline(const PipelineInfo& info)
{
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
      info.vertex_format ? info.vertex_format->GetVertexInputStateInfo() : empty_vertex_input_state;

  // Input assembly
  static constexpr std::array<VkPrimitiveTopology, 4> vk_primitive_topologies = {
      {VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP}};
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
      vk_primitive_topologies[static_cast<u32>(info.rasterization_state.primitive.Value())],
      VK_FALSE};

  // See Vulkan spec, section 19:
  // If topology is VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
  // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY,
  // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY or VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
  // primitiveRestartEnable must be VK_FALSE
  if (g_ActiveConfig.backend_info.bSupportsPrimitiveRestart &&
      IsStripPrimitiveTopology(input_assembly_state.topology))
  {
    input_assembly_state.primitiveRestartEnable = VK_TRUE;
  }

  // Shaders to stages
  VkPipelineShaderStageCreateInfo shader_stages[3];
  uint32_t num_shader_stages = 0;
  if (info.vs != VK_NULL_HANDLE)
  {
    shader_stages[num_shader_stages++] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                          nullptr,
                                          0,
                                          VK_SHADER_STAGE_VERTEX_BIT,
                                          info.vs,
                                          "main"};
  }
  if (info.gs != VK_NULL_HANDLE)
  {
    shader_stages[num_shader_stages++] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                          nullptr,
                                          0,
                                          VK_SHADER_STAGE_GEOMETRY_BIT,
                                          info.gs,
                                          "main"};
  }
  if (info.ps != VK_NULL_HANDLE)
  {
    shader_stages[num_shader_stages++] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                          nullptr,
                                          0,
                                          VK_SHADER_STAGE_FRAGMENT_BIT,
                                          info.ps,
                                          "main"};
  }

  // Fill in Vulkan descriptor structs from our state structures.
  VkPipelineRasterizationStateCreateInfo rasterization_state =
      GetVulkanRasterizationState(info.rasterization_state);
  VkPipelineMultisampleStateCreateInfo multisample_state =
      GetVulkanMultisampleState(info.multisampling_state);
  VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
      GetVulkanDepthStencilState(info.depth_state);
  VkPipelineColorBlendAttachmentState blend_attachment_state =
      GetVulkanAttachmentBlendState(info.blend_state);
  VkPipelineColorBlendStateCreateInfo blend_state =
      GetVulkanColorBlendState(info.blend_state, &blend_attachment_state, 1);

  // This viewport isn't used, but needs to be specified anyway.
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

  // Set viewport and scissor dynamic state so we can change it elsewhere.
  static const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                                  VK_DYNAMIC_STATE_SCISSOR};
  static const VkPipelineDynamicStateCreateInfo dynamic_state = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr,
      0,                                            // VkPipelineDynamicStateCreateFlags    flags
      static_cast<u32>(ArraySize(dynamic_states)),  // uint32_t dynamicStateCount
      dynamic_states  // const VkDynamicState*                pDynamicStates
  };

  // Combine to full pipeline info structure.
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

  VkPipeline pipeline;
  VkResult res = vkCreateGraphicsPipelines(g_vulkan_context->GetDevice(), m_pipeline_cache, 1,
                                           &pipeline_info, nullptr, &pipeline);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateGraphicsPipelines failed: ");
    return VK_NULL_HANDLE;
  }

  return pipeline;
}

VkPipeline ShaderCache::GetPipeline(const PipelineInfo& info)
{
  return GetPipelineWithCacheResult(info).first;
}

std::pair<VkPipeline, bool> ShaderCache::GetPipelineWithCacheResult(const PipelineInfo& info)
{
  auto iter = m_pipeline_objects.find(info);
  if (iter != m_pipeline_objects.end())
  {
    // If it's background compiling, ignore it, and recompile it synchronously.
    if (!iter->second.second)
      return std::make_pair(iter->second.first, true);
    else
      m_pipeline_objects.erase(iter);
  }

  VkPipeline pipeline = CreatePipeline(info);
  m_pipeline_objects.emplace(info, std::make_pair(pipeline, false));
  _assert_(pipeline != VK_NULL_HANDLE);
  return {pipeline, false};
}

std::pair<std::pair<VkPipeline, bool>, bool>
ShaderCache::GetPipelineWithCacheResultAsync(const PipelineInfo& info)
{
  auto iter = m_pipeline_objects.find(info);
  if (iter != m_pipeline_objects.end())
    return std::make_pair(iter->second, true);

  // Kick a job off.
  m_async_shader_compiler->QueueWorkItem(
      m_async_shader_compiler->CreateWorkItem<PipelineCompilerWorkItem>(info));
  m_pipeline_objects.emplace(info, std::make_pair(static_cast<VkPipeline>(VK_NULL_HANDLE), true));
  return std::make_pair(std::make_pair(static_cast<VkPipeline>(VK_NULL_HANDLE), true), false);
}

VkPipeline ShaderCache::CreateComputePipeline(const ComputePipelineInfo& info)
{
  VkComputePipelineCreateInfo pipeline_info = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                               nullptr,
                                               0,
                                               {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, info.cs,
                                                "main", nullptr},
                                               info.pipeline_layout,
                                               VK_NULL_HANDLE,
                                               -1};

  VkPipeline pipeline;
  VkResult res = vkCreateComputePipelines(g_vulkan_context->GetDevice(), VK_NULL_HANDLE, 1,
                                          &pipeline_info, nullptr, &pipeline);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateComputePipelines failed: ");
    return VK_NULL_HANDLE;
  }

  return pipeline;
}

VkPipeline ShaderCache::GetComputePipeline(const ComputePipelineInfo& info)
{
  auto iter = m_compute_pipeline_objects.find(info);
  if (iter != m_compute_pipeline_objects.end())
    return iter->second;

  VkPipeline pipeline = CreateComputePipeline(info);
  m_compute_pipeline_objects.emplace(info, pipeline);
  return pipeline;
}

void ShaderCache::ClearPipelineCache()
{
  // TODO: Stop any async compiling happening.
  for (const auto& it : m_pipeline_objects)
  {
    if (it.second.first != VK_NULL_HANDLE)
      vkDestroyPipeline(g_vulkan_context->GetDevice(), it.second.first, nullptr);
  }
  m_pipeline_objects.clear();

  for (const auto& it : m_compute_pipeline_objects)
  {
    if (it.second != VK_NULL_HANDLE)
      vkDestroyPipeline(g_vulkan_context->GetDevice(), it.second, nullptr);
  }
  m_compute_pipeline_objects.clear();
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

bool ShaderCache::CreatePipelineCache()
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

bool ShaderCache::LoadPipelineCache()
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
// TODO: Remove the #if here when GCC 5 is a minimum build requirement.
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 5
static_assert(std::has_trivial_copy_constructor<VK_PIPELINE_CACHE_HEADER>::value,
              "VK_PIPELINE_CACHE_HEADER must be trivially copyable");
#else
static_assert(std::is_trivially_copyable<VK_PIPELINE_CACHE_HEADER>::value,
              "VK_PIPELINE_CACHE_HEADER must be trivially copyable");
#endif

bool ShaderCache::ValidatePipelineCache(const u8* data, size_t data_length)
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

void ShaderCache::DestroyPipelineCache()
{
  ClearPipelineCache();
  vkDestroyPipelineCache(g_vulkan_context->GetDevice(), m_pipeline_cache, nullptr);
  m_pipeline_cache = VK_NULL_HANDLE;
}

void ShaderCache::SavePipelineCache()
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

// Cache inserter that is called back when reading from the file
template <typename Uid>
struct ShaderCacheReader : public LinearDiskCacheReader<Uid, u32>
{
  ShaderCacheReader(std::map<Uid, std::pair<VkShaderModule, bool>>& shader_map)
      : m_shader_map(shader_map)
  {
  }
  void Read(const Uid& key, const u32* value, u32 value_size) override
  {
    // We don't insert null modules into the shader map since creation could succeed later on.
    // e.g. we're generating bad code, but fix this in a later version, and for some reason
    // the cache is not invalidated.
    VkShaderModule module = Util::CreateShaderModule(value, value_size);
    if (module == VK_NULL_HANDLE)
      return;

    m_shader_map.emplace(key, std::make_pair(module, false));
  }

  std::map<Uid, std::pair<VkShaderModule, bool>>& m_shader_map;
};

void ShaderCache::LoadShaderCaches()
{
  ShaderCacheReader<VertexShaderUid> vs_reader(m_vs_cache.shader_map);
  m_vs_cache.disk_cache.OpenAndRead(GetDiskShaderCacheFileName(APIType::Vulkan, "VS", true, true),
                                    vs_reader);

  ShaderCacheReader<PixelShaderUid> ps_reader(m_ps_cache.shader_map);
  m_ps_cache.disk_cache.OpenAndRead(GetDiskShaderCacheFileName(APIType::Vulkan, "PS", true, true),
                                    ps_reader);

  if (g_vulkan_context->SupportsGeometryShaders())
  {
    ShaderCacheReader<GeometryShaderUid> gs_reader(m_gs_cache.shader_map);
    m_gs_cache.disk_cache.OpenAndRead(GetDiskShaderCacheFileName(APIType::Vulkan, "GS", true, true),
                                      gs_reader);
  }

  ShaderCacheReader<UberShader::VertexShaderUid> uber_vs_reader(m_uber_vs_cache.shader_map);
  m_uber_vs_cache.disk_cache.OpenAndRead(
      GetDiskShaderCacheFileName(APIType::Vulkan, "UberVS", false, true), uber_vs_reader);
  ShaderCacheReader<UberShader::PixelShaderUid> uber_ps_reader(m_uber_ps_cache.shader_map);
  m_uber_ps_cache.disk_cache.OpenAndRead(
      GetDiskShaderCacheFileName(APIType::Vulkan, "UberPS", false, true), uber_ps_reader);

  SETSTAT(stats.numPixelShadersCreated, static_cast<int>(m_ps_cache.shader_map.size()));
  SETSTAT(stats.numPixelShadersAlive, static_cast<int>(m_ps_cache.shader_map.size()));
  SETSTAT(stats.numVertexShadersCreated, static_cast<int>(m_vs_cache.shader_map.size()));
  SETSTAT(stats.numVertexShadersAlive, static_cast<int>(m_vs_cache.shader_map.size()));
}

template <typename T>
static void DestroyShaderCache(T& cache)
{
  cache.disk_cache.Sync();
  cache.disk_cache.Close();
  for (const auto& it : cache.shader_map)
  {
    if (it.second.first != VK_NULL_HANDLE)
      vkDestroyShaderModule(g_vulkan_context->GetDevice(), it.second.first, nullptr);
  }
  cache.shader_map.clear();
}

void ShaderCache::DestroyShaderCaches()
{
  DestroyShaderCache(m_vs_cache);
  DestroyShaderCache(m_ps_cache);

  if (g_vulkan_context->SupportsGeometryShaders())
    DestroyShaderCache(m_gs_cache);

  DestroyShaderCache(m_uber_vs_cache);
  DestroyShaderCache(m_uber_ps_cache);

  SETSTAT(stats.numPixelShadersCreated, 0);
  SETSTAT(stats.numPixelShadersAlive, 0);
  SETSTAT(stats.numVertexShadersCreated, 0);
  SETSTAT(stats.numVertexShadersAlive, 0);
}

VkShaderModule ShaderCache::GetVertexShaderForUid(const VertexShaderUid& uid)
{
  auto it = m_vs_cache.shader_map.find(uid);
  if (it != m_vs_cache.shader_map.end())
  {
    // If it's pending, compile it synchronously.
    if (!it->second.second)
      return it->second.first;
    else
      m_vs_cache.shader_map.erase(it);
  }

  // Not in the cache, so compile the shader.
  ShaderCompiler::SPIRVCodeVector spv;
  VkShaderModule module = VK_NULL_HANDLE;
  ShaderCode source_code =
      GenerateVertexShaderCode(APIType::Vulkan, ShaderHostConfig::GetCurrent(), uid.GetUidData());
  if (ShaderCompiler::CompileVertexShader(&spv, source_code.GetBuffer().c_str(),
                                          source_code.GetBuffer().length()))
  {
    module = Util::CreateShaderModule(spv.data(), spv.size());

    // Append to shader cache if it created successfully.
    if (module != VK_NULL_HANDLE)
    {
      m_vs_cache.disk_cache.Append(uid, spv.data(), static_cast<u32>(spv.size()));
      INCSTAT(stats.numVertexShadersCreated);
      INCSTAT(stats.numVertexShadersAlive);
    }
  }

  // We still insert null entries to prevent further compilation attempts.
  m_vs_cache.shader_map.emplace(uid, std::make_pair(module, false));
  return module;
}

VkShaderModule ShaderCache::GetGeometryShaderForUid(const GeometryShaderUid& uid)
{
  _assert_(g_vulkan_context->SupportsGeometryShaders());
  auto it = m_gs_cache.shader_map.find(uid);
  if (it != m_gs_cache.shader_map.end())
  {
    // If it's pending, compile it synchronously.
    if (!it->second.second)
      return it->second.first;
    else
      m_gs_cache.shader_map.erase(it);
  }

  // Not in the cache, so compile the shader.
  ShaderCompiler::SPIRVCodeVector spv;
  VkShaderModule module = VK_NULL_HANDLE;
  ShaderCode source_code =
      GenerateGeometryShaderCode(APIType::Vulkan, ShaderHostConfig::GetCurrent(), uid.GetUidData());
  if (ShaderCompiler::CompileGeometryShader(&spv, source_code.GetBuffer().c_str(),
                                            source_code.GetBuffer().length()))
  {
    module = Util::CreateShaderModule(spv.data(), spv.size());

    // Append to shader cache if it created successfully.
    if (module != VK_NULL_HANDLE)
      m_gs_cache.disk_cache.Append(uid, spv.data(), static_cast<u32>(spv.size()));
  }

  // We still insert null entries to prevent further compilation attempts.
  m_gs_cache.shader_map.emplace(uid, std::make_pair(module, false));
  return module;
}

VkShaderModule ShaderCache::GetPixelShaderForUid(const PixelShaderUid& uid)
{
  auto it = m_ps_cache.shader_map.find(uid);
  if (it != m_ps_cache.shader_map.end())
  {
    // If it's pending, compile it synchronously.
    if (!it->second.second)
      return it->second.first;
    else
      m_ps_cache.shader_map.erase(it);
  }

  // Not in the cache, so compile the shader.
  ShaderCompiler::SPIRVCodeVector spv;
  VkShaderModule module = VK_NULL_HANDLE;
  ShaderCode source_code =
      GeneratePixelShaderCode(APIType::Vulkan, ShaderHostConfig::GetCurrent(), uid.GetUidData());
  if (ShaderCompiler::CompileFragmentShader(&spv, source_code.GetBuffer().c_str(),
                                            source_code.GetBuffer().length()))
  {
    module = Util::CreateShaderModule(spv.data(), spv.size());

    // Append to shader cache if it created successfully.
    if (module != VK_NULL_HANDLE)
    {
      m_ps_cache.disk_cache.Append(uid, spv.data(), static_cast<u32>(spv.size()));
      INCSTAT(stats.numPixelShadersCreated);
      INCSTAT(stats.numPixelShadersAlive);
    }
  }

  // We still insert null entries to prevent further compilation attempts.
  m_ps_cache.shader_map.emplace(uid, std::make_pair(module, false));
  return module;
}

VkShaderModule ShaderCache::GetVertexUberShaderForUid(const UberShader::VertexShaderUid& uid)
{
  auto it = m_uber_vs_cache.shader_map.find(uid);
  if (it != m_uber_vs_cache.shader_map.end())
  {
    // If it's pending, compile it synchronously.
    if (!it->second.second)
      return it->second.first;
    else
      m_uber_vs_cache.shader_map.erase(it);
  }

  // Not in the cache, so compile the shader.
  ShaderCompiler::SPIRVCodeVector spv;
  VkShaderModule module = VK_NULL_HANDLE;
  ShaderCode source_code = UberShader::GenVertexShader(
      APIType::Vulkan, ShaderHostConfig::GetCurrent(), uid.GetUidData());
  if (ShaderCompiler::CompileVertexShader(&spv, source_code.GetBuffer().c_str(),
                                          source_code.GetBuffer().length()))
  {
    module = Util::CreateShaderModule(spv.data(), spv.size());

    // Append to shader cache if it created successfully.
    if (module != VK_NULL_HANDLE)
    {
      m_uber_vs_cache.disk_cache.Append(uid, spv.data(), static_cast<u32>(spv.size()));
      INCSTAT(stats.numVertexShadersCreated);
      INCSTAT(stats.numVertexShadersAlive);
    }
  }

  // We still insert null entries to prevent further compilation attempts.
  m_uber_vs_cache.shader_map.emplace(uid, std::make_pair(module, false));
  return module;
}

VkShaderModule ShaderCache::GetPixelUberShaderForUid(const UberShader::PixelShaderUid& uid)
{
  auto it = m_uber_ps_cache.shader_map.find(uid);
  if (it != m_uber_ps_cache.shader_map.end())
  {
    // If it's pending, compile it synchronously.
    if (!it->second.second)
      return it->second.first;
    else
      m_uber_ps_cache.shader_map.erase(it);
  }

  // Not in the cache, so compile the shader.
  ShaderCompiler::SPIRVCodeVector spv;
  VkShaderModule module = VK_NULL_HANDLE;
  ShaderCode source_code =
      UberShader::GenPixelShader(APIType::Vulkan, ShaderHostConfig::GetCurrent(), uid.GetUidData());
  if (ShaderCompiler::CompileFragmentShader(&spv, source_code.GetBuffer().c_str(),
                                            source_code.GetBuffer().length()))
  {
    module = Util::CreateShaderModule(spv.data(), spv.size());

    // Append to shader cache if it created successfully.
    if (module != VK_NULL_HANDLE)
    {
      m_uber_ps_cache.disk_cache.Append(uid, spv.data(), static_cast<u32>(spv.size()));
      INCSTAT(stats.numPixelShadersCreated);
      INCSTAT(stats.numPixelShadersAlive);
    }
  }

  // We still insert null entries to prevent further compilation attempts.
  m_uber_ps_cache.shader_map.emplace(uid, std::make_pair(module, false));
  return module;
}

void ShaderCache::RecompileSharedShaders()
{
  DestroySharedShaders();
  if (!CompileSharedShaders())
    PanicAlert("Failed to recompile shared shaders.");
}

void ShaderCache::ReloadShaderAndPipelineCaches()
{
  m_async_shader_compiler->WaitUntilCompletion();
  m_async_shader_compiler->RetrieveWorkItems();

  SavePipelineCache();
  DestroyShaderCaches();
  DestroyPipelineCache();

  if (g_ActiveConfig.bShaderCache)
  {
    LoadShaderCaches();
    LoadPipelineCache();
  }
  else
  {
    CreatePipelineCache();
  }

  if (g_ActiveConfig.CanPrecompileUberShaders())
    PrecompileUberShaders();
}

std::string ShaderCache::GetUtilityShaderHeader() const
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

std::size_t PipelineInfoHash::operator()(const PipelineInfo& key) const
{
  return static_cast<std::size_t>(XXH64(&key, sizeof(key), 0));
}

bool operator==(const PipelineInfo& lhs, const PipelineInfo& rhs)
{
  return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

bool operator!=(const PipelineInfo& lhs, const PipelineInfo& rhs)
{
  return !operator==(lhs, rhs);
}

bool operator<(const PipelineInfo& lhs, const PipelineInfo& rhs)
{
  return std::memcmp(&lhs, &rhs, sizeof(lhs)) < 0;
}

bool operator>(const PipelineInfo& lhs, const PipelineInfo& rhs)
{
  return std::memcmp(&lhs, &rhs, sizeof(lhs)) > 0;
}

std::size_t ComputePipelineInfoHash::operator()(const ComputePipelineInfo& key) const
{
  return static_cast<std::size_t>(XXH64(&key, sizeof(key), 0));
}

bool operator==(const ComputePipelineInfo& lhs, const ComputePipelineInfo& rhs)
{
  return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

bool operator!=(const ComputePipelineInfo& lhs, const ComputePipelineInfo& rhs)
{
  return !operator==(lhs, rhs);
}

bool operator<(const ComputePipelineInfo& lhs, const ComputePipelineInfo& rhs)
{
  return std::memcmp(&lhs, &rhs, sizeof(lhs)) < 0;
}

bool operator>(const ComputePipelineInfo& lhs, const ComputePipelineInfo& rhs)
{
  return std::memcmp(&lhs, &rhs, sizeof(lhs)) > 0;
}

bool ShaderCache::CompileSharedShaders()
{
  static const char PASSTHROUGH_VERTEX_SHADER_SOURCE[] = R"(
    layout(location = 0) in vec4 ipos;
    layout(location = 5) in vec4 icol0;
    layout(location = 8) in vec3 itex0;

    layout(location = 0) out vec3 uv0;
    layout(location = 1) out vec4 col0;

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

    layout(location = 0) in vec3 in_uv0[];
    layout(location = 1) in vec4 in_col0[];

    layout(location = 0) out vec3 out_uv0;
    layout(location = 1) out vec4 out_col0;

    void main()
    {
      for (int j = 0; j < EFB_LAYERS; j++)
      {
        for (int i = 0; i < 3; i++)
        {
          gl_Layer = j;
          gl_Position = gl_in[i].gl_Position;
          out_uv0 = vec3(in_uv0[i].xy, float(j));
          out_col0 = in_col0[i];
          EmitVertex();
        }
        EndPrimitive();
      }
    }
  )";

  static const char SCREEN_QUAD_VERTEX_SHADER_SOURCE[] = R"(
    layout(location = 0) out vec3 uv0;

    void main()
    {
        /*
         * id   &1    &2   clamp(*2-1)
         * 0    0,0   0,0  -1,-1      TL
         * 1    1,0   1,0  1,-1       TR
         * 2    0,2   0,1  -1,1       BL
         * 3    1,2   1,1  1,1        BR
         */
        vec2 rawpos = vec2(float(gl_VertexID & 1), clamp(float(gl_VertexID & 2), 0.0f, 1.0f));
        gl_Position = vec4(rawpos * 2.0f - 1.0f, 0.0f, 1.0f);
        uv0 = vec3(rawpos, 0.0f);
    }
  )";

  static const char SCREEN_QUAD_GEOMETRY_SHADER_SOURCE[] = R"(
    layout(triangles) in;
    layout(triangle_strip, max_vertices = EFB_LAYERS * 3) out;

    layout(location = 0) in vec3 in_uv0[];

    layout(location = 0) out vec3 out_uv0;

    void main()
    {
      for (int j = 0; j < EFB_LAYERS; j++)
      {
        for (int i = 0; i < 3; i++)
        {
          gl_Layer = j;
          gl_Position = gl_in[i].gl_Position;
          out_uv0 = vec3(in_uv0[i].xy, float(j));
          EmitVertex();
        }
        EndPrimitive();
      }
    }
  )";

  std::string header = GetUtilityShaderHeader();

  m_screen_quad_vertex_shader =
      Util::CompileAndCreateVertexShader(header + SCREEN_QUAD_VERTEX_SHADER_SOURCE);
  m_passthrough_vertex_shader =
      Util::CompileAndCreateVertexShader(header + PASSTHROUGH_VERTEX_SHADER_SOURCE);
  if (m_screen_quad_vertex_shader == VK_NULL_HANDLE ||
      m_passthrough_vertex_shader == VK_NULL_HANDLE)
  {
    return false;
  }

  if (g_ActiveConfig.iStereoMode != STEREO_OFF && g_vulkan_context->SupportsGeometryShaders())
  {
    m_screen_quad_geometry_shader =
        Util::CompileAndCreateGeometryShader(header + SCREEN_QUAD_GEOMETRY_SHADER_SOURCE);
    m_passthrough_geometry_shader =
        Util::CompileAndCreateGeometryShader(header + PASSTHROUGH_GEOMETRY_SHADER_SOURCE);
    if (m_screen_quad_geometry_shader == VK_NULL_HANDLE ||
        m_passthrough_geometry_shader == VK_NULL_HANDLE)
    {
      return false;
    }
  }

  return true;
}

void ShaderCache::DestroySharedShaders()
{
  auto DestroyShader = [this](VkShaderModule& shader) {
    if (shader != VK_NULL_HANDLE)
    {
      vkDestroyShaderModule(g_vulkan_context->GetDevice(), shader, nullptr);
      shader = VK_NULL_HANDLE;
    }
  };

  DestroyShader(m_screen_quad_vertex_shader);
  DestroyShader(m_passthrough_vertex_shader);
  DestroyShader(m_screen_quad_geometry_shader);
  DestroyShader(m_passthrough_geometry_shader);
}

void ShaderCache::CreateDummyPipeline(const UberShader::VertexShaderUid& vuid,
                                      const GeometryShaderUid& guid,
                                      const UberShader::PixelShaderUid& puid)
{
  PortableVertexDeclaration vertex_decl;
  std::memset(&vertex_decl, 0, sizeof(vertex_decl));

  PipelineInfo pinfo;
  pinfo.vertex_format =
      static_cast<const VertexFormat*>(VertexLoaderManager::GetUberVertexFormat(vertex_decl));
  pinfo.pipeline_layout = g_object_cache->GetPipelineLayout(
      g_ActiveConfig.bBBoxEnable && g_ActiveConfig.BBoxUseFragmentShaderImplementation() ?
          PIPELINE_LAYOUT_BBOX :
          PIPELINE_LAYOUT_STANDARD);
  pinfo.vs = GetVertexUberShaderForUid(vuid);
  pinfo.gs = (!guid.GetUidData()->IsPassthrough() && g_vulkan_context->SupportsGeometryShaders()) ?
                 GetGeometryShaderForUid(guid) :
                 VK_NULL_HANDLE;
  pinfo.ps = GetPixelUberShaderForUid(puid);
  pinfo.render_pass = FramebufferManager::GetInstance()->GetEFBLoadRenderPass();
  pinfo.rasterization_state.hex = RenderState::GetNoCullRasterizationState().hex;
  pinfo.depth_state.hex = RenderState::GetNoDepthTestingDepthStencilState().hex;
  pinfo.blend_state.hex = RenderState::GetNoBlendingBlendState().hex;
  pinfo.multisampling_state.hex = FramebufferManager::GetInstance()->GetEFBMultisamplingState().hex;
  pinfo.rasterization_state.primitive =
      static_cast<PrimitiveType>(guid.GetUidData()->primitive_type);
  GetPipelineWithCacheResultAsync(pinfo);
}

void ShaderCache::PrecompileUberShaders()
{
  UberShader::EnumerateVertexShaderUids([&](const UberShader::VertexShaderUid& vuid) {
    UberShader::EnumeratePixelShaderUids([&](const UberShader::PixelShaderUid& puid) {
      // UIDs must have compatible texgens, a mismatching combination will never be queried.
      if (vuid.GetUidData()->num_texgens != puid.GetUidData()->num_texgens)
        return;

      EnumerateGeometryShaderUids([&](const GeometryShaderUid& guid) {
        if (guid.GetUidData()->numTexGens != vuid.GetUidData()->num_texgens)
          return;

        CreateDummyPipeline(vuid, guid, puid);
      });
    });
  });

  WaitForBackgroundCompilesToComplete();

  // Switch to the runtime/background thread config.
  m_async_shader_compiler->ResizeWorkerThreads(g_ActiveConfig.GetShaderCompilerThreads());
}

void ShaderCache::WaitForBackgroundCompilesToComplete()
{
  m_async_shader_compiler->WaitUntilCompletion([](size_t completed, size_t total) {
    Host_UpdateProgressDialog(GetStringT("Compiling shaders...").c_str(),
                              static_cast<int>(completed), static_cast<int>(total));
  });
  m_async_shader_compiler->RetrieveWorkItems();
  Host_UpdateProgressDialog("", -1, -1);
}

void ShaderCache::RetrieveAsyncShaders()
{
  m_async_shader_compiler->RetrieveWorkItems();
}

std::pair<VkShaderModule, bool> ShaderCache::GetVertexShaderForUidAsync(const VertexShaderUid& uid)
{
  auto it = m_vs_cache.shader_map.find(uid);
  if (it != m_vs_cache.shader_map.end())
    return it->second;

  // Kick a compile job off.
  m_async_shader_compiler->QueueWorkItem(
      m_async_shader_compiler->CreateWorkItem<VertexShaderCompilerWorkItem>(uid));
  m_vs_cache.shader_map.emplace(uid,
                                std::make_pair(static_cast<VkShaderModule>(VK_NULL_HANDLE), true));
  return std::make_pair<VkShaderModule, bool>(VK_NULL_HANDLE, true);
}

std::pair<VkShaderModule, bool> ShaderCache::GetPixelShaderForUidAsync(const PixelShaderUid& uid)
{
  auto it = m_ps_cache.shader_map.find(uid);
  if (it != m_ps_cache.shader_map.end())
    return it->second;

  // Kick a compile job off.
  m_async_shader_compiler->QueueWorkItem(
      m_async_shader_compiler->CreateWorkItem<PixelShaderCompilerWorkItem>(uid));
  m_ps_cache.shader_map.emplace(uid,
                                std::make_pair(static_cast<VkShaderModule>(VK_NULL_HANDLE), true));
  return std::make_pair<VkShaderModule, bool>(VK_NULL_HANDLE, true);
}

bool ShaderCache::VertexShaderCompilerWorkItem::Compile()
{
  ShaderCode code =
      GenerateVertexShaderCode(APIType::Vulkan, ShaderHostConfig::GetCurrent(), m_uid.GetUidData());
  if (!ShaderCompiler::CompileVertexShader(&m_spirv, code.GetBuffer().c_str(),
                                           code.GetBuffer().length()))
    return true;

  m_module = Util::CreateShaderModule(m_spirv.data(), m_spirv.size());
  return true;
}

void ShaderCache::VertexShaderCompilerWorkItem::Retrieve()
{
  auto it = g_shader_cache->m_vs_cache.shader_map.find(m_uid);
  if (it == g_shader_cache->m_vs_cache.shader_map.end())
  {
    g_shader_cache->m_vs_cache.shader_map.emplace(m_uid, std::make_pair(m_module, false));
    g_shader_cache->m_vs_cache.disk_cache.Append(m_uid, m_spirv.data(),
                                                 static_cast<u32>(m_spirv.size()));
    return;
  }

  // The main thread may have also compiled this shader.
  if (!it->second.second)
  {
    if (m_module != VK_NULL_HANDLE)
      vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_module, nullptr);
    return;
  }

  // No longer pending.
  it->second.first = m_module;
  it->second.second = false;
  g_shader_cache->m_vs_cache.disk_cache.Append(m_uid, m_spirv.data(),
                                               static_cast<u32>(m_spirv.size()));
}

bool ShaderCache::PixelShaderCompilerWorkItem::Compile()
{
  ShaderCode code =
      GeneratePixelShaderCode(APIType::Vulkan, ShaderHostConfig::GetCurrent(), m_uid.GetUidData());
  if (!ShaderCompiler::CompileFragmentShader(&m_spirv, code.GetBuffer().c_str(),
                                             code.GetBuffer().length()))
    return true;

  m_module = Util::CreateShaderModule(m_spirv.data(), m_spirv.size());
  return true;
}

void ShaderCache::PixelShaderCompilerWorkItem::Retrieve()
{
  auto it = g_shader_cache->m_ps_cache.shader_map.find(m_uid);
  if (it == g_shader_cache->m_ps_cache.shader_map.end())
  {
    g_shader_cache->m_ps_cache.shader_map.emplace(m_uid, std::make_pair(m_module, false));
    g_shader_cache->m_ps_cache.disk_cache.Append(m_uid, m_spirv.data(),
                                                 static_cast<u32>(m_spirv.size()));
    return;
  }

  // The main thread may have also compiled this shader.
  if (!it->second.second)
  {
    if (m_module != VK_NULL_HANDLE)
      vkDestroyShaderModule(g_vulkan_context->GetDevice(), m_module, nullptr);
    return;
  }

  // No longer pending.
  it->second.first = m_module;
  it->second.second = false;
  g_shader_cache->m_ps_cache.disk_cache.Append(m_uid, m_spirv.data(),
                                               static_cast<u32>(m_spirv.size()));
}

bool ShaderCache::PipelineCompilerWorkItem::Compile()
{
  m_pipeline = g_shader_cache->CreatePipeline(m_info);
  return true;
}

void ShaderCache::PipelineCompilerWorkItem::Retrieve()
{
  auto it = g_shader_cache->m_pipeline_objects.find(m_info);
  if (it == g_shader_cache->m_pipeline_objects.end())
  {
    g_shader_cache->m_pipeline_objects.emplace(m_info, std::make_pair(m_pipeline, false));
    return;
  }

  // The main thread may have also compiled this shader.
  if (!it->second.second)
  {
    if (m_pipeline != VK_NULL_HANDLE)
      vkDestroyPipeline(g_vulkan_context->GetDevice(), m_pipeline, nullptr);
    return;
  }

  // No longer pending.
  it->second.first = m_pipeline;
  it->second.second = false;
}
}
