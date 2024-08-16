// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/VKPipeline.h"

#include <array>

#include "Common/Assert.h"
#include "Common/EnumMap.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/VKShader.h"
#include "VideoBackends/Vulkan/VKTexture.h"
#include "VideoBackends/Vulkan/VKVertexFormat.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/DriverDetails.h"

namespace Vulkan
{
VKPipeline::VKPipeline(const AbstractPipelineConfig& config, VkPipeline pipeline,
                       VkPipelineLayout pipeline_layout, AbstractPipelineUsage usage)
    : AbstractPipeline(config), m_pipeline(pipeline), m_pipeline_layout(pipeline_layout),
      m_usage(usage)
{
}

VKPipeline::~VKPipeline()
{
  vkDestroyPipeline(g_vulkan_context->GetDevice(), m_pipeline, nullptr);
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
      cull_modes[u32(state.cullmode.Value())],  // VkCullModeFlags        cullMode
      VK_FRONT_FACE_CLOCKWISE,                  // VkFrontFace            frontFace
      VK_FALSE,  // VkBool32                                              depthBiasEnable
      0.0f,      // float                                                 depthBiasConstantFactor
      0.0f,      // float                                                 depthBiasClamp
      0.0f,      // float                                                 depthBiasSlopeFactor
      1.0f       // float                                                 lineWidth
  };
}

static VkPipelineMultisampleStateCreateInfo GetVulkanMultisampleState(const FramebufferState& state)
{
  return {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,  // const void*                              pNext
      0,        // VkPipelineMultisampleStateCreateFlags    flags
      static_cast<VkSampleCountFlagBits>(
          state.samples.Value()),  // VkSampleCountFlagBits                    rasterizationSamples
      static_cast<bool>(state.per_sample_shading),  // VkBool32 sampleShadingEnable
      1.0f,      // float                                    minSampleShading
      nullptr,   // const VkSampleMask*                      pSampleMask;
      VK_FALSE,  // VkBool32                                 alphaToCoverageEnable
      VK_FALSE   // VkBool32                                 alphaToOneEnable
  };
}

static VkPipelineDepthStencilStateCreateInfo GetVulkanDepthStencilState(const DepthState& state)
{
  // Less/greater are swapped due to inverted depth.
  VkCompareOp compare_op;
  bool inverted_depth = !g_ActiveConfig.backend_info.bSupportsReversedDepthRange;
  switch (state.func)
  {
  case CompareMode::Never:
    compare_op = VK_COMPARE_OP_NEVER;
    break;
  case CompareMode::Less:
    compare_op = inverted_depth ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS;
    break;
  case CompareMode::Equal:
    compare_op = VK_COMPARE_OP_EQUAL;
    break;
  case CompareMode::LEqual:
    compare_op = inverted_depth ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
    break;
  case CompareMode::Greater:
    compare_op = inverted_depth ? VK_COMPARE_OP_LESS : VK_COMPARE_OP_GREATER;
    break;
  case CompareMode::NEqual:
    compare_op = VK_COMPARE_OP_NOT_EQUAL;
    break;
  case CompareMode::GEqual:
    compare_op = inverted_depth ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_GREATER_OR_EQUAL;
    break;
  case CompareMode::Always:
    compare_op = VK_COMPARE_OP_ALWAYS;
    break;
  default:
    PanicAlertFmt("Invalid compare mode {}", state.func);
    compare_op = VK_COMPARE_OP_ALWAYS;
    break;
  }

  return {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,             // const void*                               pNext
      0,                   // VkPipelineDepthStencilStateCreateFlags    flags
      state.testenable,    // VkBool32                                  depthTestEnable
      state.updateenable,  // VkBool32                                  depthWriteEnable
      compare_op,          // VkCompareOp                               depthCompareOp
      VK_FALSE,            // VkBool32                                  depthBoundsTestEnable
      VK_FALSE,            // VkBool32                                  stencilTestEnable
      {},                  // VkStencilOpState                          front
      {},                  // VkStencilOpState                          back
      0.0f,                // float                                     minDepthBounds
      1.0f                 // float                                     maxDepthBounds
  };
}

static VkPipelineColorBlendAttachmentState
GetVulkanAttachmentBlendState(const BlendingState& state, AbstractPipelineUsage usage)
{
  VkPipelineColorBlendAttachmentState vk_state = {};

  bool use_dual_source = state.usedualsrc;

  vk_state.blendEnable = static_cast<VkBool32>(state.blendenable);
  vk_state.colorBlendOp = state.subtract ? VK_BLEND_OP_REVERSE_SUBTRACT : VK_BLEND_OP_ADD;
  vk_state.alphaBlendOp = state.subtractAlpha ? VK_BLEND_OP_REVERSE_SUBTRACT : VK_BLEND_OP_ADD;

  if (use_dual_source)
  {
    static constexpr Common::EnumMap<VkBlendFactor, SrcBlendFactor::InvDstAlpha> src_factors{
        VK_BLEND_FACTOR_ZERO,       VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_DST_COLOR,  VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
        VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
        VK_BLEND_FACTOR_DST_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    };
    static constexpr Common::EnumMap<VkBlendFactor, DstBlendFactor::InvDstAlpha> dst_factors{
        VK_BLEND_FACTOR_ZERO,       VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_SRC_COLOR,  VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
        VK_BLEND_FACTOR_SRC1_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA,
        VK_BLEND_FACTOR_DST_ALPHA,  VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    };

    vk_state.srcColorBlendFactor = src_factors[state.srcfactor];
    vk_state.srcAlphaBlendFactor = src_factors[state.srcfactoralpha];
    vk_state.dstColorBlendFactor = dst_factors[state.dstfactor];
    vk_state.dstAlphaBlendFactor = dst_factors[state.dstfactoralpha];
  }
  else
  {
    static constexpr Common::EnumMap<VkBlendFactor, SrcBlendFactor::InvDstAlpha> src_factors{
        VK_BLEND_FACTOR_ZERO,      VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
        VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    };

    static constexpr Common::EnumMap<VkBlendFactor, DstBlendFactor::InvDstAlpha> dst_factors{
        VK_BLEND_FACTOR_ZERO,      VK_BLEND_FACTOR_ONE,
        VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
        VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
    };

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
  if (vk_logic_op_enable && !g_ActiveConfig.backend_info.bSupportsLogicOp)
  {
    // At the time of writing, Adreno and Mali drivers didn't support logic ops.
    // The "emulation" through blending path has been removed, so just disable it completely.
    // These drivers don't support dual-source blend either, so issues are to be expected.
    vk_logic_op_enable = VK_FALSE;
  }

  VkLogicOp vk_logic_op =
      vk_logic_op_enable ? vk_logic_ops[u32(state.logicmode.Value())] : VK_LOGIC_OP_CLEAR;

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

std::unique_ptr<VKPipeline> VKPipeline::Create(const AbstractPipelineConfig& config)
{
  DEBUG_ASSERT(config.vertex_shader && config.pixel_shader);

  // Get render pass for config.
  VkRenderPass render_pass = g_object_cache->GetRenderPass(
      VKTexture::GetVkFormatForHostTextureFormat(config.framebuffer_state.color_texture_format),
      VKTexture::GetVkFormatForHostTextureFormat(config.framebuffer_state.depth_texture_format),
      config.framebuffer_state.samples, VK_ATTACHMENT_LOAD_OP_LOAD,
      config.framebuffer_state.additional_color_attachment_count);

  if (render_pass == VK_NULL_HANDLE)
  {
    PanicAlertFmt("Failed to get render pass");
    return nullptr;
  }

  // Get pipeline layout.
  VkPipelineLayout pipeline_layout;
  switch (config.usage)
  {
  case AbstractPipelineUsage::GX:
    pipeline_layout = g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_STANDARD);
    break;
  case AbstractPipelineUsage::GXUber:
    pipeline_layout = g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_UBER);
    break;
  case AbstractPipelineUsage::Utility:
    pipeline_layout = g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_UTILITY);
    break;
  default:
    PanicAlertFmt("Unknown pipeline layout.");
    return nullptr;
  }

  // Declare descriptors for empty vertex buffers/attributes
  static constexpr VkPipelineVertexInputStateCreateInfo empty_vertex_input_state = {
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
      config.vertex_format ?
          static_cast<const VertexFormat*>(config.vertex_format)->GetVertexInputStateInfo() :
          empty_vertex_input_state;

  // Input assembly
  static constexpr std::array<VkPrimitiveTopology, 4> vk_primitive_topologies = {
      {VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
       VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP}};
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
      vk_primitive_topologies[static_cast<u32>(config.rasterization_state.primitive.Value())],
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
  if (config.vertex_shader)
  {
    shader_stages[num_shader_stages++] = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        0,
        VK_SHADER_STAGE_VERTEX_BIT,
        static_cast<const VKShader*>(config.vertex_shader)->GetShaderModule(),
        "main"};
  }
  if (config.geometry_shader)
  {
    shader_stages[num_shader_stages++] = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        0,
        VK_SHADER_STAGE_GEOMETRY_BIT,
        static_cast<const VKShader*>(config.geometry_shader)->GetShaderModule(),
        "main"};
  }
  if (config.pixel_shader)
  {
    shader_stages[num_shader_stages++] = {
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        nullptr,
        0,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        static_cast<const VKShader*>(config.pixel_shader)->GetShaderModule(),
        "main"};
  }

  // Fill in Vulkan descriptor structs from our state structures.
  VkPipelineRasterizationStateCreateInfo rasterization_state =
      GetVulkanRasterizationState(config.rasterization_state);
  VkPipelineMultisampleStateCreateInfo multisample_state =
      GetVulkanMultisampleState(config.framebuffer_state);
  VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
      GetVulkanDepthStencilState(config.depth_state);
  VkPipelineColorBlendAttachmentState blend_attachment_state =
      GetVulkanAttachmentBlendState(config.blending_state, config.usage);

  std::vector<VkPipelineColorBlendAttachmentState> blend_attachment_states;
  blend_attachment_states.push_back(blend_attachment_state);
  // Right now all our attachments have the same state
  for (u8 i = 0; i < static_cast<u8>(config.framebuffer_state.additional_color_attachment_count);
       i++)
  {
    blend_attachment_states.push_back(blend_attachment_state);
  }
  VkPipelineColorBlendStateCreateInfo blend_state =
      GetVulkanColorBlendState(config.blending_state, blend_attachment_states.data(),
                               static_cast<uint32_t>(blend_attachment_states.size()));

  // This viewport isn't used, but needs to be specified anyway.
  static constexpr VkViewport viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
  static constexpr VkRect2D scissor = {{0, 0}, {1, 1}};
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
  static constexpr std::array<VkDynamicState, 2> dynamic_states{
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };
  static const VkPipelineDynamicStateCreateInfo dynamic_state = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr,
      0,                                        // VkPipelineDynamicStateCreateFlags    flags
      static_cast<u32>(dynamic_states.size()),  // uint32_t dynamicStateCount
      dynamic_states.data()  // const VkDynamicState*                pDynamicStates
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
      pipeline_layout,       // VkPipelineLayout                                 layout
      render_pass,           // VkRenderPass                                     renderPass
      0,                     // uint32_t                                         subpass
      VK_NULL_HANDLE,        // VkPipeline                                       basePipelineHandle
      -1                     // int32_t                                          basePipelineIndex
  };

  VkPipeline pipeline;
  VkResult res =
      vkCreateGraphicsPipelines(g_vulkan_context->GetDevice(), g_object_cache->GetPipelineCache(),
                                1, &pipeline_info, nullptr, &pipeline);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateGraphicsPipelines failed: ");
    return VK_NULL_HANDLE;
  }

  return std::make_unique<VKPipeline>(config, pipeline, pipeline_layout, config.usage);
}
}  // namespace Vulkan
