// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cassert>
#include <cstring>

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/PaletteTextureConverter.h"
#include "VideoBackends/Vulkan/Renderer.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/Util.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
PaletteTextureConverter::PaletteTextureConverter(StateTracker* state_tracker)
    : m_state_tracker(state_tracker)
{
}

PaletteTextureConverter::~PaletteTextureConverter()
{
  for (const auto& it : m_pipelines)
  {
    if (it != VK_NULL_HANDLE)
      vkDestroyPipeline(g_object_cache->GetDevice(), it, nullptr);
  }

  for (const auto& it : m_shaders)
  {
    if (it != VK_NULL_HANDLE)
      vkDestroyShaderModule(g_object_cache->GetDevice(), it, nullptr);
  }

  if (m_palette_buffer_view != VK_NULL_HANDLE)
    vkDestroyBufferView(g_object_cache->GetDevice(), m_palette_buffer_view, nullptr);

  if (m_render_pass != VK_NULL_HANDLE)
    vkDestroyRenderPass(g_object_cache->GetDevice(), m_render_pass, nullptr);

  if (m_set_layout != VK_NULL_HANDLE)
    vkDestroyDescriptorSetLayout(g_object_cache->GetDevice(), m_set_layout, nullptr);
}

bool PaletteTextureConverter::Initialize()
{
  if (!CreateBuffers())
    return false;

  if (!CompileShaders())
    return false;

  if (!CreateRenderPass())
    return false;

  if (!CreateDescriptorLayout())
    return false;

  if (!CreatePipelines())
    return false;

  return true;
}

void PaletteTextureConverter::ConvertTexture(Texture2D* dst_texture, VkFramebuffer dst_framebuffer,
                                             Texture2D* src_texture, u32 width, u32 height,
                                             void* palette, TlutFormat format)
{
  struct PSUniformBlock
  {
    float multiplier;
    int texel_buffer_offset;
    int pad[2];
  };

  _assert_(static_cast<u32>(format) < NUM_PALETTE_CONVERSION_SHADERS);

  size_t palette_size = ((format & 0xF) == GX_TF_I4) ? 32 : 512;
  StreamBuffer* uniform_buffer = g_object_cache->GetUtilityShaderUniformBuffer();
  VkDescriptorSet descriptor_set;

  // Allocate memory for the palette, and descriptor sets for the buffer.
  // If any of these fail, execute a command buffer, and try again.
  if (!uniform_buffer->ReserveMemory(sizeof(PSUniformBlock),
                                     g_object_cache->GetUniformBufferAlignment()) ||
      !m_palette_stream_buffer->ReserveMemory(palette_size,
                                              g_object_cache->GetTexelBufferAlignment()) ||
      (descriptor_set = g_command_buffer_mgr->AllocateDescriptorSet(m_set_layout)) ==
          VK_NULL_HANDLE)
  {
    WARN_LOG(VIDEO, "Executing command list while waiting for space in palette buffer");
    Util::ExecuteCurrentCommandsAndRestoreState(m_state_tracker, false);
    if (!uniform_buffer->ReserveMemory(sizeof(PSUniformBlock),
                                       g_object_cache->GetUniformBufferAlignment()) ||
        !m_palette_stream_buffer->ReserveMemory(palette_size,
                                                g_object_cache->GetTexelBufferAlignment()) ||
        (descriptor_set = g_command_buffer_mgr->AllocateDescriptorSet(m_set_layout)) ==
            VK_NULL_HANDLE)
    {
      PanicAlert("Failed to allocate space for texture conversion");
      return;
    }
  }

  // Update descriptor set
  VkDescriptorBufferInfo uniform_buffer_info = {
      g_object_cache->GetUtilityShaderUniformBuffer()->GetBuffer(), 0, sizeof(PSUniformBlock)};
  VkDescriptorImageInfo sampler_info = {g_object_cache->GetPointSampler(), src_texture->GetView(),
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  VkWriteDescriptorSet set_writes[] = {
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 0, 0, 1,
       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr, &uniform_buffer_info, nullptr},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 0, 1, 1,
       VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &sampler_info, nullptr, nullptr},
      {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptor_set, 0, 1, 2,
       VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, nullptr, nullptr, &m_palette_buffer_view}};
  vkUpdateDescriptorSets(g_object_cache->GetDevice(), ARRAYSIZE(set_writes), set_writes, 0,
                         nullptr);

  // Copy in uniforms
  u32 uniforms_offset = static_cast<u32>(uniform_buffer->GetCurrentOffset());
  u32 palette_offset = static_cast<u32>(m_palette_stream_buffer->GetCurrentOffset());
  PSUniformBlock uniforms = {};
  uniforms.multiplier = ((format & 0xF)) == GX_TF_I4 ? 15.0f : 255.0f;
  uniforms.texel_buffer_offset = static_cast<int>(palette_offset / sizeof(u16));
  memcpy(uniform_buffer->GetCurrentHostPointer(), &uniforms, sizeof(uniforms));
  memcpy(m_palette_stream_buffer->GetCurrentHostPointer(), palette, palette_size);
  uniform_buffer->CommitMemory(sizeof(PSUniformBlock));
  m_palette_stream_buffer->CommitMemory(palette_size);

  // Make sure we're not in a render pass
  m_state_tracker->EndRenderPass();
  m_state_tracker->SetPendingRebind();

  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();

  // Transition resource states
  dst_texture->OverrideImageLayout(VK_IMAGE_LAYOUT_UNDEFINED);
  dst_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  Util::BufferMemoryBarrier(command_buffer, m_palette_stream_buffer->GetBuffer(),
                            VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, palette_offset,
                            palette_size, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

  // Invoke the shader
  VkRenderPassBeginInfo render_pass_begin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                             nullptr,
                                             0,
                                             dst_framebuffer,
                                             {{0, 0}, {width, height}},
                                             0,
                                             nullptr};
  vkCmdBeginRenderPass(command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelines[format]);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
                          &descriptor_set, 1, &uniforms_offset);
  Util::SetViewportAndScissor(command_buffer, 0, 0, width, height);
  vkCmdDraw(command_buffer, 4, 1, 0, 0);
  vkCmdEndRenderPass(command_buffer);

  // Transition resources before re-use
  dst_texture->TransitionToLayout(command_buffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  Util::BufferMemoryBarrier(command_buffer, m_palette_stream_buffer->GetBuffer(),
                            VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_HOST_WRITE_BIT, palette_offset,
                            palette_size, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
}

bool PaletteTextureConverter::CreateBuffers()
{
  // TODO: Check against maximum size
  static const size_t BUFFER_SIZE = 1024 * 1024;

  m_palette_stream_buffer = StreamBuffer::Create(VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT,
                                                 BUFFER_SIZE,
                                                 BUFFER_SIZE);
  if (!m_palette_stream_buffer)
    return false;

  // Create a view of the whole buffer, we'll offset our texel load into it
  VkBufferViewCreateInfo view_info = {
      VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO,  // VkStructureType            sType
      nullptr,                                    // const void*                pNext
      0,                                          // VkBufferViewCreateFlags    flags
      m_palette_stream_buffer->GetBuffer(),       // VkBuffer                   buffer
      VK_FORMAT_R16_UINT,                         // VkFormat                   format
      0,                                          // VkDeviceSize               offset
      BUFFER_SIZE                                 // VkDeviceSize               range
  };

  VkResult res =
      vkCreateBufferView(g_object_cache->GetDevice(), &view_info, nullptr, &m_palette_buffer_view);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateBufferView failed: ");
    return false;
  }

  return true;
}

bool PaletteTextureConverter::CompileShaders()
{
  static const char PALETTE_CONVERSION_FRAGMENT_SHADER_SOURCE[] = R"(
    layout(std140, set = 0, binding = 0) uniform PSBlock
    {
      float multiplier;
      int texture_buffer_offset;
    };

    layout(set = 0, binding = 1) uniform sampler2DArray samp0;
    layout(set = 0, binding = 2) uniform usamplerBuffer samp1;

    layout(location = 0) in vec3 f_uv0;
    layout(location = 0) out vec4 ocol0;

    int Convert3To8(int v)
    {
	    // Swizzle bits: 00000123 -> 12312312
	    return (v << 5) | (v << 2) | (v >> 1);
    }
    int Convert4To8(int v)
    {
	    // Swizzle bits: 00001234 -> 12341234
	    return (v << 4) | v;
    }
    int Convert5To8(int v)
    {
	    // Swizzle bits: 00012345 -> 12345123
	    return (v << 3) | (v >> 2);
    }
    int Convert6To8(int v)
    {
	    // Swizzle bits: 00123456 -> 12345612
	    return (v << 2) | (v >> 4);
    }
    float4 DecodePixel_RGB5A3(int val)
    {
	    int r,g,b,a;
	    if ((val&0x8000) > 0)
	    {
		    r=Convert5To8((val>>10) & 0x1f);
		    g=Convert5To8((val>>5 ) & 0x1f);
		    b=Convert5To8((val    ) & 0x1f);
		    a=0xFF;
	    }
	    else
	    {
		    a=Convert3To8((val>>12) & 0x7);
		    r=Convert4To8((val>>8 ) & 0xf);
		    g=Convert4To8((val>>4 ) & 0xf);
		    b=Convert4To8((val    ) & 0xf);
	    }
	    return float4(r, g, b, a) / 255.0;
    }
    float4 DecodePixel_RGB565(int val)
    {
	    int r, g, b, a;
	    r = Convert5To8((val >> 11) & 0x1f);
	    g = Convert6To8((val >> 5) & 0x3f);
	    b = Convert5To8((val) & 0x1f);
	    a = 0xFF;
	    return float4(r, g, b, a) / 255.0;
    }
    float4 DecodePixel_IA8(int val)
    {
	    int i = val & 0xFF;
	    int a = val >> 8;
	    return float4(i, i, i, a) / 255.0;
    }
    void main()
    {
	    int src = int(round(texture(samp0, f_uv0).r * multiplier));
	    src = int(texelFetch(samp1, src + texture_buffer_offset).r);
	    src = ((src << 8) & 0xFF00) | (src >> 8);
	    ocol0 = DECODE(src);
    }

  )";

  std::string palette_ia8_program = StringFromFormat("%s\n%s", "#define DECODE DecodePixel_IA8",
                                                     PALETTE_CONVERSION_FRAGMENT_SHADER_SOURCE);
  std::string palette_rgb565_program = StringFromFormat(
      "%s\n%s", "#define DECODE DecodePixel_RGB565", PALETTE_CONVERSION_FRAGMENT_SHADER_SOURCE);
  std::string palette_rgb5a3_program = StringFromFormat(
      "%s\n%s", "#define DECODE DecodePixel_RGB5A3", PALETTE_CONVERSION_FRAGMENT_SHADER_SOURCE);
  if ((m_shaders[GX_TL_IA8] = g_object_cache->GetPixelShaderCache().CompileAndCreateShader(
           palette_ia8_program)) == nullptr ||
      (m_shaders[GX_TL_RGB565] = g_object_cache->GetPixelShaderCache().CompileAndCreateShader(
           palette_rgb565_program)) == nullptr ||
      (m_shaders[GX_TL_RGB5A3] = g_object_cache->GetPixelShaderCache().CompileAndCreateShader(
           palette_rgb5a3_program)) == nullptr)
  {
    return false;
  }

  return true;
}

bool PaletteTextureConverter::CreateRenderPass()
{
  VkAttachmentDescription attachments[] = {
      {0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_LOAD_OP_DONT_CARE,
       VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkAttachmentReference color_attachment_references[] = {
      {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

  VkSubpassDescription subpass_descriptions[] = {{0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1,
                                                  color_attachment_references, nullptr, nullptr, 0,
                                                  nullptr}};

  VkRenderPassCreateInfo pass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                      nullptr,
                                      0,
                                      ARRAYSIZE(attachments),
                                      attachments,
                                      ARRAYSIZE(subpass_descriptions),
                                      subpass_descriptions,
                                      0,
                                      nullptr};

  VkResult res =
      vkCreateRenderPass(g_object_cache->GetDevice(), &pass_info, nullptr, &m_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass failed: ");
    return false;
  }

  return true;
}

bool PaletteTextureConverter::CreateDescriptorLayout()
{
  static const VkDescriptorSetLayoutBinding set_bindings[] = {
      {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
      {2, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT},
  };
  static const VkDescriptorSetLayoutCreateInfo set_info = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, ARRAYSIZE(set_bindings),
      set_bindings};

  VkResult res =
      vkCreateDescriptorSetLayout(g_object_cache->GetDevice(), &set_info, nullptr, &m_set_layout);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateDescriptorSetLayout failed: ");
    return false;
  }

  VkPipelineLayoutCreateInfo pipeline_layout_info = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &m_set_layout, 0, nullptr};

  res = vkCreatePipelineLayout(g_object_cache->GetDevice(), &pipeline_layout_info, nullptr,
                               &m_pipeline_layout);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreatePipelineLayout failed: ");
    return false;
  }

  return true;
}

bool PaletteTextureConverter::CreatePipelines()
{
  static const VkPipelineVertexInputStateCreateInfo empty_vertex_input_state = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,  // const void*                               pNext
      0,        // VkPipelineVertexInputStateCreateFlags     flags
      0,        // uint32_t                                  vertexBindingDescriptionCount
      nullptr,  // const VkVertexInputBindingDescription*    pVertexBindingDescriptions
      0,        // uint32_t                                  vertexAttributeDescriptionCount
      nullptr   // const VkVertexInputAttributeDescription*  pVertexAttributeDescriptions
  };

  static const VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,                               // const void*                               pNext
      0,                                     // VkPipelineInputAssemblyStateCreateFlags   flags
      VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,  // VkPrimitiveTopology                       topology
      VK_TRUE  // VkBool32                                  primitiveRestartEnable
  };

  static const VkPipelineRasterizationStateCreateInfo rasterization_state = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,                  // const void*                               pNext
      0,                        // VkPipelineRasterizationStateCreateFlags   flags
      VK_FALSE,                 // VkBool32                                  depthClampEnable
      VK_FALSE,                 // VkBool32                                  rasterizerDiscardEnable
      VK_POLYGON_MODE_FILL,     // VkPolygonMode                             polygonMode
      VK_CULL_MODE_NONE,        // VkCullModeFlags                           cullMode
      VK_FRONT_FACE_CLOCKWISE,  // VkFrontFace                               frontFace
      VK_FALSE,                 // VkBool32                                  depthBiasEnable
      0.0f,                     // float                                     depthBiasConstantFactor
      0.0f,                     // float                                     depthBiasClamp
      0.0f,                     // float                                     depthBiasSlopeFactor
      1.0f                      // float                                     lineWidth
  };

  static const VkPipelineDepthStencilStateCreateInfo depthstencil_state = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,               // const void*                               pNext
      0,                     // VkPipelineDepthStencilStateCreateFlags    flags
      VK_FALSE,              // VkBool32                                  depthTestEnable
      VK_FALSE,              // VkBool32                                  depthWriteEnable
      VK_COMPARE_OP_ALWAYS,  // VkCompareOp                               depthCompareOp
      VK_TRUE,               // VkBool32                                  depthBoundsTestEnable
      VK_FALSE,              // VkBool32                                  stencilTestEnable
      {},                    // VkStencilOpState                          front
      {},                    // VkStencilOpState                          back
      0.0f,                  // float                                     minDepthBounds
      1.0f                   // float                                     maxDepthBounds
  };

  static const VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
      VK_FALSE,              // VkBool32                                  blendEnable
      VK_BLEND_FACTOR_ONE,   // VkBlendFactor                             srcColorBlendFactor
      VK_BLEND_FACTOR_ZERO,  // VkBlendFactor                             dstColorBlendFactor
      VK_BLEND_OP_ADD,       // VkBlendOp                                 colorBlendOp
      VK_BLEND_FACTOR_ONE,   // VkBlendFactor                             srcAlphaBlendFactor
      VK_BLEND_FACTOR_ZERO,  // VkBlendFactor                             dstAlphaBlendFactor
      VK_BLEND_OP_ADD,       // VkBlendOp                                 alphaBlendOp
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
          VK_COLOR_COMPONENT_A_BIT  // VkColorComponentFlags                     colorWriteMask
  };

  static const VkPipelineColorBlendStateCreateInfo color_blend_state = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,                        // const void*                               pNext
      0,                              // VkPipelineColorBlendStateCreateFlags      flags
      VK_FALSE,                       // VkBool32                                  logicOpEnable
      VK_LOGIC_OP_CLEAR,              // VkLogicOp                                 logicOp
      1,                              // uint32_t                                  attachmentCount
      &color_blend_attachment_state,  // const VkPipelineColorBlendAttachmentState*pAttachments
      {1.0f, 1.0f, 1.0f, 1.0f}        // float                                     blendConstants[4]
  };

  VkPipelineMultisampleStateCreateInfo multisample_state = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,                // const void*                               pNext
      0,                      // VkPipelineMultisampleStateCreateFlags     flags
      VK_SAMPLE_COUNT_1_BIT,  // VkSampleCountFlagBits                     rasterizationSamples
      VK_FALSE,               // VkBool32                                  sampleShadingEnable
      1.0f,                   // float                                     minSampleShading
      nullptr,                // const VkSampleMask*                       pSampleMask;
      VK_FALSE,               // VkBool32                                  alphaToCoverageEnable
      VK_FALSE                // VkBool32                                  alphaToOneEnable
  };

  // Viewport is used with dynamic state
  static const VkViewport viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
  static const VkRect2D scissor = {{0, 0}, {1, 1}};
  static const VkPipelineViewportStateCreateInfo viewport_state = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,    // const void*                               pNext
      0,          // VkPipelineViewportStateCreateFlags        flags;
      1,          // uint32_t                                  viewportCount
      &viewport,  // const VkViewport*                         pViewports
      1,          // uint32_t                                  scissorCount
      &scissor    // const VkRect2D*                           pScissors
  };

  // Set viewport and scissor dynamic state so we can change it elsewhere
  static const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                                  VK_DYNAMIC_STATE_SCISSOR};
  static const VkPipelineDynamicStateCreateInfo dynamic_state = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,  // VkStructureType sType
      nullptr,                    // const void*                               pNext
      0,                          // VkPipelineDynamicStateCreateFlags         flags
      ARRAYSIZE(dynamic_states),  // uint32_t                                  dynamicStateCount
      dynamic_states              // const VkDynamicState*                     pDynamicStates
  };

  for (u32 i = 0; i < NUM_PALETTE_CONVERSION_SHADERS; i++)
  {
    VkPipelineShaderStageCreateInfo stage_create_info[] = {
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,  // VkStructureType sType
            nullptr,                     // const void*                               pNext
            0,                           // VkPipelineShaderStageCreateFlags          flags
            VK_SHADER_STAGE_VERTEX_BIT,  // VkShaderStageFlagBits                     stage
            g_object_cache->GetSharedShaderCache()
                .GetScreenQuadVertexShader(),  // VkShaderModule                  module
            "main",                            // const char*                               pName
            nullptr  // const VkSpecializationInfo*               pSpecializationInfo
        },
        {
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,  // VkStructureType sType
            nullptr,                       // const void*                               pNext
            0,                             // VkPipelineShaderStageCreateFlags          flags
            VK_SHADER_STAGE_FRAGMENT_BIT,  // VkShaderStageFlagBits                     stage
            m_shaders[i],                  // VkShaderModule                            module
            "main",                        // const char*                               pName
            nullptr  // const VkSpecializationInfo*               pSpecializationInfo
        }};

    VkGraphicsPipelineCreateInfo pipeline_info = {
        VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,  // VkStructureType sType
        nullptr,                       // const void*                                      pNext
        0,                             // VkPipelineCreateFlags                            flags
        ARRAYSIZE(stage_create_info),  // uint32_t stageCount
        stage_create_info,             // const VkPipelineShaderStageCreateInfo*           pStages
        &empty_vertex_input_state,  // const VkPipelineVertexInputStateCreateInfo* pVertexInputState
        &input_assembly_state,  // const VkPipelineInputAssemblyStateCreateInfo* pInputAssemblyState
        nullptr,          // const VkPipelineTessellationStateCreateInfo*     pTessellationState
        &viewport_state,  // const VkPipelineViewportStateCreateInfo*         pViewportState
        &rasterization_state,  // const VkPipelineRasterizationStateCreateInfo* pRasterizationState
        &multisample_state,    // const VkPipelineMultisampleStateCreateInfo*      pMultisampleState
        &depthstencil_state,  // const VkPipelineDepthStencilStateCreateInfo*     pDepthStencilState
        &color_blend_state,   // const VkPipelineColorBlendStateCreateInfo*       pColorBlendState
        &dynamic_state,       // const VkPipelineDynamicStateCreateInfo*          pDynamicState
        m_pipeline_layout,    // VkPipelineLayout                                 layout
        m_render_pass,        // VkRenderPass                                     renderPass
        0,                    // uint32_t                                         subpass
        VK_NULL_HANDLE,       // VkPipeline                                       basePipelineHandle
        -1                    // int32_t                                          basePipelineIndex
    };

    VkResult res = vkCreateGraphicsPipelines(g_object_cache->GetDevice(), VK_NULL_HANDLE, 1,
                                             &pipeline_info, nullptr, &m_pipelines[i]);
    if (res != VK_SUCCESS)
    {
      LOG_VULKAN_ERROR(res, "vkCreateGraphicsPipelines failed: ");
      return false;
    }
  }

  return true;
}

}  // namespace Vulkan
