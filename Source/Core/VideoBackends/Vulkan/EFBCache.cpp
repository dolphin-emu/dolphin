// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/VideoConfig.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/EFBCache.h"
#include "VideoBackends/Vulkan/FramebufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/ShaderCache.h"
#include "VideoBackends/Vulkan/StagingTexture2D.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/Texture2D.h"
#include "VideoBackends/Vulkan/VertexFormat.h"

namespace Vulkan
{
// Maximum number of pixels poked in one batch * 6
static const size_t MAX_POKE_VERTICES = 8192;
static const size_t VERTEX_BUFFER_SIZE = 8 * 1024 * 1024;

EFBCache::EFBCache(FramebufferManager* framebuffer_mgr) : m_framebuffer_mgr(framebuffer_mgr)
{
}

EFBCache::~EFBCache()
{
  if (m_color_readback_texture && m_color_readback_texture->IsMapped())
    m_color_readback_texture->Unmap();
  if (m_depth_readback_texture && m_depth_readback_texture->IsMapped())
    m_depth_readback_texture->Unmap();

  if (m_copy_color_render_pass != VK_NULL_HANDLE)
    vkDestroyRenderPass(g_object_cache->GetDevice(), m_copy_color_render_pass, nullptr);
  if (m_copy_depth_render_pass != VK_NULL_HANDLE)
    vkDestroyRenderPass(g_object_cache->GetDevice(), m_copy_depth_render_pass, nullptr);
  if (m_color_copy_framebuffer != VK_NULL_HANDLE)
    vkDestroyFramebuffer(g_object_cache->GetDevice(), m_color_copy_framebuffer, nullptr);
  if (m_depth_copy_framebuffer != VK_NULL_HANDLE)
    vkDestroyFramebuffer(g_object_cache->GetDevice(), m_depth_copy_framebuffer, nullptr);
  if (m_copy_color_shader != VK_NULL_HANDLE)
    vkDestroyShaderModule(g_object_cache->GetDevice(), m_copy_color_shader, nullptr);
  if (m_copy_depth_shader != VK_NULL_HANDLE)
    vkDestroyShaderModule(g_object_cache->GetDevice(), m_copy_depth_shader, nullptr);
  if (m_poke_vertex_shader != VK_NULL_HANDLE)
    vkDestroyShaderModule(g_object_cache->GetDevice(), m_poke_vertex_shader, nullptr);
  if (m_poke_geometry_shader != VK_NULL_HANDLE)
    vkDestroyShaderModule(g_object_cache->GetDevice(), m_poke_geometry_shader, nullptr);
  if (m_poke_pixel_shader != VK_NULL_HANDLE)
    vkDestroyShaderModule(g_object_cache->GetDevice(), m_poke_pixel_shader, nullptr);
}

bool EFBCache::Initialize()
{
  if (!CreateRenderPasses())
    return false;

  if (!CompileShaders())
    return false;

  if (!CreateTextures())
    return false;

  CreatePokeVertexFormat();
  if (!CreatePokeVertexBuffer())
    return false;

  return true;
}

u32 EFBCache::PeekEFBColor(StateTracker* state_tracker, u32 x, u32 y)
{
  if (!m_color_readback_texture_valid && !PopulateColorReadbackTexture(state_tracker))
    return 0;

  u32 value;
  m_color_readback_texture->ReadTexel(x, y, &value, sizeof(value));
  return value;
}

float EFBCache::PeekEFBDepth(StateTracker* state_tracker, u32 x, u32 y)
{
  if (!m_depth_readback_texture_valid && !PopulateDepthReadbackTexture(state_tracker))
    return 0.0f;

  float value;
  m_depth_readback_texture->ReadTexel(x, y, &value, sizeof(value));
  return value;
}

void EFBCache::InvalidatePeekCache()
{
  m_color_readback_texture_valid = false;
  m_depth_readback_texture_valid = false;
}

void EFBCache::PokeEFBColor(StateTracker* state_tracker, u32 x, u32 y, u32 color)
{
  // Flush if we exceeded the number of vertices per batch.
  if ((m_color_poke_vertices.size() + 6) > MAX_POKE_VERTICES)
    FlushEFBPokes(state_tracker);

  CreatePokeVertices(&m_color_poke_vertices, x, y, 0.0f, color);

  // Update the peek cache if it's valid, since we know the color of the pixel now.
  if (m_color_readback_texture_valid)
    m_color_readback_texture->WriteTexel(x, y, &color, sizeof(color));
}

void EFBCache::PokeEFBDepth(StateTracker* state_tracker, u32 x, u32 y, float depth)
{
  // Flush if we exceeded the number of vertices per batch.
  if ((m_color_poke_vertices.size() + 6) > MAX_POKE_VERTICES)
    FlushEFBPokes(state_tracker);

  CreatePokeVertices(&m_depth_poke_vertices, x, y, depth, 0);

  // Update the peek cache if it's valid, since we know the color of the pixel now.
  if (m_depth_readback_texture_valid)
    m_depth_readback_texture->WriteTexel(x, y, &depth, sizeof(depth));
}

void EFBCache::CreatePokeVertices(std::vector<EFBPokeVertex>* destination_list, u32 x, u32 y,
                                  float z, u32 color)
{
  // Some devices don't support point sizes >1 (e.g. Adreno).
  if (m_poke_primitive_topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP)
  {
    // generate quad from the single point (clip-space coordinates)
    float x1 = float(x) * 2.0f / EFB_WIDTH - 1.0f;
    float y1 = float(y) * 2.0f / EFB_HEIGHT - 1.0f;
    float x2 = float(x + 1) * 2.0f / EFB_WIDTH - 1.0f;
    float y2 = float(y + 1) * 2.0f / EFB_HEIGHT - 1.0f;
    destination_list->push_back({{x1, y1, z, 1.0f}, color});
    destination_list->push_back({{x2, y1, z, 1.0f}, color});
    destination_list->push_back({{x1, y2, z, 1.0f}, color});
    destination_list->push_back({{x1, y2, z, 1.0f}, color});
    destination_list->push_back({{x2, y1, z, 1.0f}, color});
    destination_list->push_back({{x2, y2, z, 1.0f}, color});
  }
  else
  {
    // GPU will expand the point to a quad.
    float cs_x = float(x) * 2.0f / EFB_WIDTH - 1.0f;
    float cs_y = float(y) * 2.0f / EFB_HEIGHT - 1.0f;
    float point_size = m_framebuffer_mgr->GetEFBWidth() / static_cast<float>(EFB_WIDTH);
    destination_list->push_back({{cs_x, cs_y, z, point_size}, color});
  }
}

void EFBCache::FlushEFBPokes(StateTracker* state_tracker)
{
  if (!m_color_poke_vertices.empty())
  {
    DrawPokeVertices(state_tracker, m_color_poke_vertices.data(), m_color_poke_vertices.size(),
                     true, false);

    m_color_poke_vertices.clear();
  }

  if (!m_depth_poke_vertices.empty())
  {
    DrawPokeVertices(state_tracker, m_depth_poke_vertices.data(), m_depth_poke_vertices.size(),
                     false, true);

    m_depth_poke_vertices.clear();
  }
}

void EFBCache::DrawPokeVertices(StateTracker* state_tracker, const EFBPokeVertex* vertices,
                                size_t vertex_count, bool write_color, bool write_depth)
{
  // Relatively simple since we don't have any bindings.
  VkCommandBuffer command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();

  // We don't use the utility shader in order to keep the vertices compact.
  PipelineInfo pipeline_info = {};
  pipeline_info.vertex_format = m_poke_vertex_format.get();
  pipeline_info.pipeline_layout = g_object_cache->GetStandardPipelineLayout();
  pipeline_info.vs = m_poke_vertex_shader;
  pipeline_info.gs =
      (m_framebuffer_mgr->GetEFBLayers() > 1) ? m_poke_geometry_shader : VK_NULL_HANDLE;
  pipeline_info.ps = m_poke_pixel_shader;
  pipeline_info.render_pass = m_framebuffer_mgr->GetEFBRenderPass();
  pipeline_info.rasterization_state.hex = Util::GetNoCullRasterizationState().hex;
  pipeline_info.depth_stencil_state.hex = Util::GetNoDepthTestingDepthStencilState().hex;
  pipeline_info.blend_state.hex = Util::GetNoBlendingBlendState().hex;
  pipeline_info.primitive_topology = m_poke_primitive_topology;
  if (write_color)
  {
    pipeline_info.blend_state.write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                           VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  }
  else
  {
    pipeline_info.blend_state.write_mask = 0;
  }
  if (write_depth)
  {
    pipeline_info.depth_stencil_state.test_enable = VK_FALSE;
    pipeline_info.depth_stencil_state.compare_op = VK_COMPARE_OP_ALWAYS;
    pipeline_info.depth_stencil_state.write_enable = VK_TRUE;
  }
  else
  {
    pipeline_info.depth_stencil_state.test_enable = VK_FALSE;
    pipeline_info.depth_stencil_state.compare_op = VK_COMPARE_OP_ALWAYS;
    pipeline_info.depth_stencil_state.write_enable = VK_FALSE;
  }

  VkPipeline pipeline = g_object_cache->GetPipeline(pipeline_info);
  if (pipeline == VK_NULL_HANDLE)
  {
    PanicAlert("Failed to get pipeline for EFB poke draw");
    return;
  }

  // Populate vertex buffer.
  size_t vertices_size = sizeof(EFBPokeVertex) * m_color_poke_vertices.size();
  if (!m_poke_vertex_stream_buffer->ReserveMemory(vertices_size, sizeof(EfbPokeData), true, true,
                                                  false))
  {
    // Kick a command buffer first.
    WARN_LOG(VIDEO, "Kicking command buffer due to no EFB poke space.");
    Util::ExecuteCurrentCommandsAndRestoreState(state_tracker, true);
    command_buffer = g_command_buffer_mgr->GetCurrentCommandBuffer();

    if (!m_poke_vertex_stream_buffer->ReserveMemory(vertices_size, sizeof(EfbPokeData), true, true,
                                                    false))
    {
      PanicAlert("Failed to get space for EFB poke vertices");
      return;
    }
  }
  VkBuffer vb_buffer = m_poke_vertex_stream_buffer->GetBuffer();
  VkDeviceSize vb_offset = m_poke_vertex_stream_buffer->GetCurrentOffset();
  memcpy(m_poke_vertex_stream_buffer->GetCurrentHostPointer(), vertices, vertices_size);
  m_poke_vertex_stream_buffer->CommitMemory(vertices_size);

  // Set up state.
  state_tracker->BeginRenderPass();
  state_tracker->SetPendingRebind();
  Util::SetViewportAndScissor(command_buffer, 0, 0, m_framebuffer_mgr->GetEFBWidth(),
                              m_framebuffer_mgr->GetEFBHeight());
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &vb_buffer, &vb_offset);
  vkCmdDraw(command_buffer, static_cast<u32>(vertex_count), 1, 0, 0);
}

bool EFBCache::CreateRenderPasses()
{
  VkAttachmentDescription copy_attachment = {
      0,                                         // VkAttachmentDescriptionFlags    flags
      EFB_COLOR_TEXTURE_FORMAT,                  // VkFormat                        format
      VK_SAMPLE_COUNT_1_BIT,                     // VkSampleCountFlagBits           samples
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,           // VkAttachmentLoadOp              loadOp
      VK_ATTACHMENT_STORE_OP_STORE,              // VkAttachmentStoreOp             storeOp
      VK_ATTACHMENT_LOAD_OP_DONT_CARE,           // VkAttachmentLoadOp              stencilLoadOp
      VK_ATTACHMENT_STORE_OP_DONT_CARE,          // VkAttachmentStoreOp             stencilStoreOp
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,  // VkImageLayout                   initialLayout
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL   // VkImageLayout                   finalLayout
  };
  VkAttachmentReference copy_attachment_ref = {
      0,                                        // uint32_t         attachment
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL  // VkImageLayout    layout
  };
  VkSubpassDescription copy_subpass = {
      0,                                // VkSubpassDescriptionFlags       flags
      VK_PIPELINE_BIND_POINT_GRAPHICS,  // VkPipelineBindPoint             pipelineBindPoint
      0,                                // uint32_t                        inputAttachmentCount
      nullptr,                          // const VkAttachmentReference*    pInputAttachments
      1,                                // uint32_t                        colorAttachmentCount
      &copy_attachment_ref,             // const VkAttachmentReference*    pColorAttachments
      nullptr,                          // const VkAttachmentReference*    pResolveAttachments
      nullptr,                          // const VkAttachmentReference*    pDepthStencilAttachment
      0,                                // uint32_t                        preserveAttachmentCount
      nullptr                           // const uint32_t*                 pPreserveAttachments
  };
  VkRenderPassCreateInfo copy_pass = {
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,  // VkStructureType                   sType
      nullptr,                                    // const void*                       pNext
      0,                                          // VkRenderPassCreateFlags           flags
      1,                 // uint32_t                          attachmentCount
      &copy_attachment,  // const VkAttachmentDescription*    pAttachments
      1,                 // uint32_t                          subpassCount
      &copy_subpass,     // const VkSubpassDescription*       pSubpasses
      0,                 // uint32_t                          dependencyCount
      nullptr            // const VkSubpassDependency*        pDependencies
  };

  VkResult res = vkCreateRenderPass(g_object_cache->GetDevice(), &copy_pass, nullptr,
                                    &m_copy_color_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass failed: ");
    return false;
  }

  // Depth is similar to copy, just a different format.
  copy_attachment.format = EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT;
  res = vkCreateRenderPass(g_object_cache->GetDevice(), &copy_pass, nullptr,
                           &m_copy_depth_render_pass);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateRenderPass failed: ");
    return false;
  }

  // Some devices don't support point sizes >1 (e.g. Adreno).
  // If we can't use a point size above our maximum IR, use triangles instead.
  // This means a 6x increase in the size of the vertices, though.
  if (!g_object_cache->GetDeviceFeatures().largePoints ||
      g_object_cache->GetDeviceLimits().pointSizeGranularity > 1 ||
      g_object_cache->GetDeviceLimits().pointSizeRange[0] > 1 ||
      g_object_cache->GetDeviceLimits().pointSizeRange[1] < 16)
  {
    m_poke_primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  }
  else
  {
    // Points should be okay.
    m_poke_primitive_topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  }

  return true;
}

bool EFBCache::CompileShaders()
{
  std::string source;

  // TODO: Use input attachment here instead?
  // TODO: MSAA resolve in shader.
  static const char COPY_COLOR_SHADER_SOURCE[] = R"(
    SAMPLER_BINDING(0) uniform sampler2DArray samp0;
    layout(location = 0) in vec3 uv0;
    layout(location = 0) out vec4 ocol0;
    void main()
    {
      ocol0 = texture(samp0, uv0);
    }
  )";

  static const char COPY_DEPTH_SHADER_SOURCE[] = R"(
    SAMPLER_BINDING(0) uniform sampler2DArray samp0;
    layout(location = 0) in vec3 uv0;
    layout(location = 0) out float ocol0;
    void main()
    {
      ocol0 = texture(samp0, uv0).r;
    }
  )";

  static const char POKE_VERTEX_SHADER_SOURCE[] = R"(
    layout(location = 0) in vec4 ipos;
    layout(location = 5) in vec4 icol0;

    layout(location = 0) out vec4 col0;

    void main()
    {
	    gl_Position = vec4(ipos.xyz, 1.0f);
      #if USE_POINT_SIZE
        gl_PointSize = ipos.w;
      #endif
	    col0 = icol0;
    }

    )";

  static const char POKE_GEOMETRY_SHADER_SOURCE[] = R"(
    layout(triangles) in;
    layout(triangle_strip, max_vertices = EFB_LAYERS * 3) out;

    in VertexData
    {
	    vec4 col0;
    } in_data[];

    out VertexData
    {
	    vec4 col0;
    } out_data;

    void main()
    {
	    for (int j = 0; j < EFB_LAYERS; j++)
	    {
		    for (int i = 0; i < 3; i++)
		    {
			    gl_Layer = j;
			    gl_Position = gl_in[i].gl_Position;
			    out_data.col0 = in_data[i].col0;
			    EmitVertex();
		    }
		    EndPrimitive();
	    }
    }
  )";

  static const char POKE_PIXEL_SHADER_SOURCE[] = R"(
    layout(location = 0) in vec4 col0;
    layout(location = 0) out vec4 ocol0;
    void main()
    {
      ocol0 = col0;
    }
  )";

  source = g_object_cache->GetUtilityShaderHeader() + COPY_COLOR_SHADER_SOURCE;
  m_copy_color_shader = g_object_cache->GetPixelShaderCache().CompileAndCreateShader(source);

  source = g_object_cache->GetUtilityShaderHeader() + COPY_DEPTH_SHADER_SOURCE;
  m_copy_depth_shader = g_object_cache->GetPixelShaderCache().CompileAndCreateShader(source);

  source = g_object_cache->GetUtilityShaderHeader();
  if (m_poke_primitive_topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
    source += "#define USE_POINT_SIZE 1\n";
  source += POKE_VERTEX_SHADER_SOURCE;
  m_poke_vertex_shader = g_object_cache->GetVertexShaderCache().CompileAndCreateShader(source);

  if (g_object_cache->SupportsGeometryShaders())
  {
    source = g_object_cache->GetUtilityShaderHeader() + POKE_GEOMETRY_SHADER_SOURCE;
    m_poke_geometry_shader =
        g_object_cache->GetGeometryShaderCache().CompileAndCreateShader(source);
  }

  source = g_object_cache->GetUtilityShaderHeader() + POKE_PIXEL_SHADER_SOURCE;
  m_poke_pixel_shader = g_object_cache->GetPixelShaderCache().CompileAndCreateShader(source);

  if (m_copy_color_shader == VK_NULL_HANDLE || m_copy_depth_shader == VK_NULL_HANDLE ||
      (g_object_cache->SupportsGeometryShaders() && m_poke_geometry_shader == VK_NULL_HANDLE) ||
      m_poke_vertex_shader == VK_NULL_HANDLE || m_poke_pixel_shader == VK_NULL_HANDLE)
  {
    ERROR_LOG(VIDEO, "Failed to compile one or more shaders");
    return false;
  }

  return true;
}

bool EFBCache::CreateTextures()
{
  m_color_copy_texture =
      Texture2D::Create(EFB_WIDTH, EFB_HEIGHT, 1, 1, EFB_COLOR_TEXTURE_FORMAT,
                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

  m_color_readback_texture =
      StagingTexture2D::Create(EFB_WIDTH, EFB_HEIGHT, EFB_COLOR_TEXTURE_FORMAT);
  if (!m_color_copy_texture || !m_color_readback_texture)
  {
    ERROR_LOG(VIDEO, "Failed to create EFB color readback texture");
    return false;
  }

  m_depth_copy_texture =
      Texture2D::Create(EFB_WIDTH, EFB_HEIGHT, 1, 1, EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT,
                        VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

  // We can't copy to/from color<->depth formats, so using a linear texture is not an option here.
  // TODO: Investigate if vkCmdBlitImage can be used. The documentation isn't that clear.
  m_depth_readback_texture =
      // StagingTexture2D::Create(EFB_WIDTH, EFB_HEIGHT, EFB_DEPTH_AS_COLOR_TEXTURE_FORMAT);
      StagingTexture2DBuffer::Create(EFB_WIDTH, EFB_HEIGHT, EFB_DEPTH_TEXTURE_FORMAT);
  if (!m_depth_copy_texture || !m_depth_readback_texture)
  {
    ERROR_LOG(VIDEO, "Failed to create EFB depth readback texture");
    return false;
  }

  // With Vulkan, we can leave these textures mapped and use invalidate/flush calls instead.
  if (!m_color_readback_texture->Map() || !m_depth_readback_texture->Map())
  {
    ERROR_LOG(VIDEO, "Failed to map EFB readback textures");
    return false;
  }

  VkImageView framebuffer_attachment = m_color_copy_texture->GetView();
  VkFramebufferCreateInfo framebuffer_info = {
      VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,  // VkStructureType             sType
      nullptr,                                    // const void*                 pNext
      0,                                          // VkFramebufferCreateFlags    flags
      m_copy_color_render_pass,                   // VkRenderPass                renderPass
      1,                                          // uint32_t                    attachmentCount
      &framebuffer_attachment,                    // const VkImageView*          pAttachments
      EFB_WIDTH,                                  // uint32_t                    width
      EFB_HEIGHT,                                 // uint32_t                    height
      1                                           // uint32_t                    layers
  };
  VkResult res = vkCreateFramebuffer(g_object_cache->GetDevice(), &framebuffer_info, nullptr,
                                     &m_color_copy_framebuffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
    return false;
  }

  // Swap for depth
  framebuffer_info.renderPass = m_copy_depth_render_pass;
  framebuffer_attachment = m_depth_copy_texture->GetView();
  res = vkCreateFramebuffer(g_object_cache->GetDevice(), &framebuffer_info, nullptr,
                            &m_depth_copy_framebuffer);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateFramebuffer failed: ");
    return false;
  }

  return true;
}

void EFBCache::CreatePokeVertexFormat()
{
  PortableVertexDeclaration vtx_decl = {};
  vtx_decl.position.enable = true;
  vtx_decl.position.type = VAR_FLOAT;
  vtx_decl.position.components = 4;
  vtx_decl.position.integer = false;
  vtx_decl.position.offset = offsetof(EFBPokeVertex, position);
  vtx_decl.colors[0].enable = true;
  vtx_decl.colors[0].type = VAR_UNSIGNED_BYTE;
  vtx_decl.colors[0].components = 4;
  vtx_decl.colors[0].integer = false;
  vtx_decl.colors[0].offset = offsetof(EFBPokeVertex, color);
  vtx_decl.stride = sizeof(EFBPokeVertex);

  m_poke_vertex_format = std::make_unique<VertexFormat>(vtx_decl);
}

bool EFBCache::CreatePokeVertexBuffer()
{
  m_poke_vertex_stream_buffer = StreamBuffer::Create(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                     VERTEX_BUFFER_SIZE, VERTEX_BUFFER_SIZE);
  if (!m_poke_vertex_stream_buffer)
  {
    ERROR_LOG(VIDEO, "Failed to create EFB poke vertex buffer");
    return false;
  }

  return true;
}

bool EFBCache::PopulateColorReadbackTexture(StateTracker* state_tracker)
{
  // Can't be in our normal render pass.
  state_tracker->EndRenderPass();

  // Issue a copy from framebuffer -> copy texture if we have >1xIR or MSAA on.
  VkRect2D src_region = {{0, 0},
                         {m_framebuffer_mgr->GetEFBWidth(), m_framebuffer_mgr->GetEFBHeight()}};
  Texture2D* src_texture = m_framebuffer_mgr->ResolveEFBDepthTexture(state_tracker, src_region);
  VkImageAspectFlags src_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  if (m_framebuffer_mgr->GetEFBWidth() != EFB_WIDTH ||
      m_framebuffer_mgr->GetEFBHeight() != EFB_HEIGHT || g_ActiveConfig.iMultisamples > 1)
  {
    UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                           g_object_cache->GetStandardPipelineLayout(), m_copy_color_render_pass,
                           g_object_cache->GetSharedShaderCache().GetScreenQuadVertexShader(),
                           VK_NULL_HANDLE, m_copy_color_shader);

    VkRect2D rect = {{0, 0}, {EFB_WIDTH, EFB_HEIGHT}};
    draw.BeginRenderPass(m_color_copy_framebuffer, rect);

    // Transition EFB to shader read before drawing.
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    draw.SetPSSampler(0, src_texture->GetView(), g_object_cache->GetPointSampler());
    draw.SetViewportAndScissor(0, 0, EFB_WIDTH, EFB_HEIGHT);
    draw.DrawWithoutVertexBuffer(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 4);
    draw.EndRenderPass();

    // Use this as a source texture now.
    src_texture = m_color_copy_texture.get();
  }

  // Copy from EFB or copy texture to staging texture.
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  m_color_readback_texture->CopyFromImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          src_texture->GetImage(), src_aspect, 0, 0, EFB_WIDTH,
                                          EFB_HEIGHT, 0, 0);

  // Restore original layout if we used the EFB as a source.
  if (src_texture == m_framebuffer_mgr->GetEFBColorTexture())
  {
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  }

  // Wait until the copy is complete.
  g_command_buffer_mgr->ExecuteCommandBuffer(false, true);
  state_tracker->InvalidateDescriptorSets();
  state_tracker->SetPendingRebind();

  // Map to host memory.
  if (!m_color_readback_texture->IsMapped() && !m_color_readback_texture->Map())
    return false;

  m_color_readback_texture_valid = true;
  return true;
}

bool EFBCache::PopulateDepthReadbackTexture(StateTracker* state_tracker)
{
  // Can't be in our normal render pass.
  state_tracker->EndRenderPass();

  // Issue a copy from framebuffer -> copy texture if we have >1xIR or MSAA on.
  VkRect2D src_region = {{0, 0},
                         {m_framebuffer_mgr->GetEFBWidth(), m_framebuffer_mgr->GetEFBHeight()}};
  Texture2D* src_texture = m_framebuffer_mgr->ResolveEFBDepthTexture(state_tracker, src_region);
  VkImageAspectFlags src_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
  if (m_framebuffer_mgr->GetEFBWidth() != EFB_WIDTH ||
      m_framebuffer_mgr->GetEFBHeight() != EFB_HEIGHT || g_ActiveConfig.iMultisamples > 1)
  {
    UtilityShaderDraw draw(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                           g_object_cache->GetStandardPipelineLayout(), m_copy_depth_render_pass,
                           g_object_cache->GetSharedShaderCache().GetScreenQuadVertexShader(),
                           VK_NULL_HANDLE, m_copy_depth_shader);

    VkRect2D rect = {{0, 0}, {EFB_WIDTH, EFB_HEIGHT}};
    draw.BeginRenderPass(m_depth_copy_framebuffer, rect);

    // Transition EFB to shader read before drawing.
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    draw.SetPSSampler(0, src_texture->GetView(), g_object_cache->GetPointSampler());
    draw.SetViewportAndScissor(0, 0, EFB_WIDTH, EFB_HEIGHT);
    draw.DrawWithoutVertexBuffer(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, 4);
    draw.EndRenderPass();

    // Use this as a source texture now.
    src_texture = m_depth_copy_texture.get();
    src_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  }

  // Copy from EFB or copy texture to staging texture.
  src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  m_depth_readback_texture->CopyFromImage(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                          src_texture->GetImage(), src_aspect, 0, 0, EFB_WIDTH,
                                          EFB_HEIGHT, 0, 0);

  // Restore original layout if we used the EFB as a source.
  if (src_texture == m_framebuffer_mgr->GetEFBDepthTexture())
  {
    src_texture->TransitionToLayout(g_command_buffer_mgr->GetCurrentCommandBuffer(),
                                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  }

  // Wait until the copy is complete.
  g_command_buffer_mgr->ExecuteCommandBuffer(false, true);
  state_tracker->InvalidateDescriptorSets();
  state_tracker->SetPendingRebind();

  // Map to host memory.
  if (!m_depth_readback_texture->IsMapped() && !m_depth_readback_texture->Map())
    return false;

  m_depth_readback_texture_valid = true;
  return true;
}

}  // namespace Vulkan
