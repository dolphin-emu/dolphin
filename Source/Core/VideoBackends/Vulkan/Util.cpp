// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/Util.h"

#include "Common/Align.h"
#include "Common/Assert.h"
#include "Common/CommonFuncs.h"
#include "Common/MathUtil.h"
#include "Common/MsgHandler.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"
#include "VideoBackends/Vulkan/ShaderCache.h"
#include "VideoBackends/Vulkan/ShaderCompiler.h"
#include "VideoBackends/Vulkan/StateTracker.h"
#include "VideoBackends/Vulkan/StreamBuffer.h"
#include "VideoBackends/Vulkan/VulkanContext.h"

namespace Vulkan
{
namespace Util
{
size_t AlignBufferOffset(size_t offset, size_t alignment)
{
  // Assume an offset of zero is already aligned to a value larger than alignment.
  if (offset == 0)
    return 0;

  return Common::AlignUp(offset, alignment);
}

u32 MakeRGBA8Color(float r, float g, float b, float a)
{
  return (static_cast<u32>(MathUtil::Clamp(static_cast<int>(r * 255.0f), 0, 255)) << 0) |
         (static_cast<u32>(MathUtil::Clamp(static_cast<int>(g * 255.0f), 0, 255)) << 8) |
         (static_cast<u32>(MathUtil::Clamp(static_cast<int>(b * 255.0f), 0, 255)) << 16) |
         (static_cast<u32>(MathUtil::Clamp(static_cast<int>(a * 255.0f), 0, 255)) << 24);
}

bool IsDepthFormat(VkFormat format)
{
  switch (format)
  {
  case VK_FORMAT_D16_UNORM:
  case VK_FORMAT_D16_UNORM_S8_UINT:
  case VK_FORMAT_D24_UNORM_S8_UINT:
  case VK_FORMAT_D32_SFLOAT:
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return true;
  default:
    return false;
  }
}

bool IsCompressedFormat(VkFormat format)
{
  switch (format)
  {
  case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
  case VK_FORMAT_BC2_UNORM_BLOCK:
  case VK_FORMAT_BC3_UNORM_BLOCK:
  case VK_FORMAT_BC7_UNORM_BLOCK:
    return true;

  default:
    return false;
  }
}

VkFormat GetLinearFormat(VkFormat format)
{
  switch (format)
  {
  case VK_FORMAT_R8_SRGB:
    return VK_FORMAT_R8_UNORM;
  case VK_FORMAT_R8G8_SRGB:
    return VK_FORMAT_R8G8_UNORM;
  case VK_FORMAT_R8G8B8_SRGB:
    return VK_FORMAT_R8G8B8_UNORM;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case VK_FORMAT_B8G8R8_SRGB:
    return VK_FORMAT_B8G8R8_UNORM;
  case VK_FORMAT_B8G8R8A8_SRGB:
    return VK_FORMAT_B8G8R8A8_UNORM;
  default:
    return format;
  }
}

VkFormat GetVkFormatForHostTextureFormat(AbstractTextureFormat format)
{
  switch (format)
  {
  case AbstractTextureFormat::DXT1:
    return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;

  case AbstractTextureFormat::DXT3:
    return VK_FORMAT_BC2_UNORM_BLOCK;

  case AbstractTextureFormat::DXT5:
    return VK_FORMAT_BC3_UNORM_BLOCK;

  case AbstractTextureFormat::BPTC:
    return VK_FORMAT_BC7_UNORM_BLOCK;

  case AbstractTextureFormat::RGBA8:
  default:
    return VK_FORMAT_R8G8B8A8_UNORM;
  }
}

u32 GetTexelSize(VkFormat format)
{
  // Only contains pixel formats we use.
  switch (format)
  {
  case VK_FORMAT_R32_SFLOAT:
    return 4;

  case VK_FORMAT_D32_SFLOAT:
    return 4;

  case VK_FORMAT_R8G8B8A8_UNORM:
    return 4;

  case VK_FORMAT_B8G8R8A8_UNORM:
    return 4;

  case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
    return 8;

  case VK_FORMAT_BC2_UNORM_BLOCK:
  case VK_FORMAT_BC3_UNORM_BLOCK:
  case VK_FORMAT_BC7_UNORM_BLOCK:
    return 16;

  default:
    PanicAlert("Unhandled pixel format");
    return 1;
  }
}

u32 GetBlockSize(VkFormat format)
{
  switch (format)
  {
  case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
  case VK_FORMAT_BC2_UNORM_BLOCK:
  case VK_FORMAT_BC3_UNORM_BLOCK:
  case VK_FORMAT_BC7_UNORM_BLOCK:
    return 4;

  default:
    return 1;
  }
}

VkRect2D ClampRect2D(const VkRect2D& rect, u32 width, u32 height)
{
  VkRect2D out;
  out.offset.x = MathUtil::Clamp(rect.offset.x, 0, static_cast<int32_t>(width - 1));
  out.offset.y = MathUtil::Clamp(rect.offset.y, 0, static_cast<int32_t>(height - 1));
  out.extent.width = std::min(rect.extent.width, width - static_cast<uint32_t>(rect.offset.x));
  out.extent.height = std::min(rect.extent.height, height - static_cast<uint32_t>(rect.offset.y));
  return out;
}

VkBlendFactor GetAlphaBlendFactor(VkBlendFactor factor)
{
  switch (factor)
  {
  case VK_BLEND_FACTOR_SRC_COLOR:
    return VK_BLEND_FACTOR_SRC_ALPHA;
  case VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  case VK_BLEND_FACTOR_DST_COLOR:
    return VK_BLEND_FACTOR_DST_ALPHA;
  case VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  default:
    return factor;
  }
}

RasterizationState GetNoCullRasterizationState()
{
  RasterizationState state = {};
  state.cull_mode = VK_CULL_MODE_NONE;
  state.samples = VK_SAMPLE_COUNT_1_BIT;
  state.per_sample_shading = VK_FALSE;
  state.depth_clamp = VK_FALSE;
  return state;
}

DepthStencilState GetNoDepthTestingDepthStencilState()
{
  DepthStencilState state = {};
  state.test_enable = VK_FALSE;
  state.write_enable = VK_FALSE;
  state.compare_op = VK_COMPARE_OP_ALWAYS;
  return state;
}

BlendingState GetNoBlendingBlendState()
{
  BlendingState state = {};
  state.blendenable = false;
  state.srcfactor = BlendMode::ONE;
  state.srcfactoralpha = BlendMode::ZERO;
  state.dstfactor = BlendMode::ONE;
  state.dstfactoralpha = BlendMode::ZERO;
  state.logicopenable = false;
  state.colorupdate = true;
  state.alphaupdate = true;
  return state;
}

void SetViewportAndScissor(VkCommandBuffer command_buffer, int x, int y, int width, int height,
                           float min_depth /*= 0.0f*/, float max_depth /*= 1.0f*/)
{
  VkViewport viewport = {static_cast<float>(x),
                         static_cast<float>(y),
                         static_cast<float>(width),
                         static_cast<float>(height),
                         min_depth,
                         max_depth};

  VkRect2D scissor = {{x, y}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};

  vkCmdSetViewport(command_buffer, 0, 1, &viewport);
  vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

void BufferMemoryBarrier(VkCommandBuffer command_buffer, VkBuffer buffer,
                         VkAccessFlags src_access_mask, VkAccessFlags dst_access_mask,
                         VkDeviceSize offset, VkDeviceSize size,
                         VkPipelineStageFlags src_stage_mask, VkPipelineStageFlags dst_stage_mask)
{
  VkBufferMemoryBarrier buffer_info = {
      VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,  // VkStructureType    sType
      nullptr,                                  // const void*        pNext
      src_access_mask,                          // VkAccessFlags      srcAccessMask
      dst_access_mask,                          // VkAccessFlags      dstAccessMask
      VK_QUEUE_FAMILY_IGNORED,                  // uint32_t           srcQueueFamilyIndex
      VK_QUEUE_FAMILY_IGNORED,                  // uint32_t           dstQueueFamilyIndex
      buffer,                                   // VkBuffer           buffer
      offset,                                   // VkDeviceSize       offset
      size                                      // VkDeviceSize       size
  };

  vkCmdPipelineBarrier(command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 1,
                       &buffer_info, 0, nullptr);
}

void ExecuteCurrentCommandsAndRestoreState(bool execute_off_thread, bool wait_for_completion)
{
  StateTracker::GetInstance()->EndRenderPass();
  g_command_buffer_mgr->ExecuteCommandBuffer(execute_off_thread, wait_for_completion);
  StateTracker::GetInstance()->InvalidateDescriptorSets();
  StateTracker::GetInstance()->InvalidateConstants();
  StateTracker::GetInstance()->SetPendingRebind();
}

VkShaderModule CreateShaderModule(const u32* spv, size_t spv_word_count)
{
  VkShaderModuleCreateInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = spv_word_count * sizeof(u32);
  info.pCode = spv;

  VkShaderModule module;
  VkResult res = vkCreateShaderModule(g_vulkan_context->GetDevice(), &info, nullptr, &module);
  if (res != VK_SUCCESS)
  {
    LOG_VULKAN_ERROR(res, "vkCreateShaderModule failed: ");
    return VK_NULL_HANDLE;
  }

  return module;
}

VkShaderModule CompileAndCreateVertexShader(const std::string& source_code)
{
  ShaderCompiler::SPIRVCodeVector code;
  if (!ShaderCompiler::CompileVertexShader(&code, source_code.c_str(), source_code.length()))
    return VK_NULL_HANDLE;

  return CreateShaderModule(code.data(), code.size());
}

VkShaderModule CompileAndCreateGeometryShader(const std::string& source_code)
{
  ShaderCompiler::SPIRVCodeVector code;
  if (!ShaderCompiler::CompileGeometryShader(&code, source_code.c_str(), source_code.length()))
    return VK_NULL_HANDLE;

  return CreateShaderModule(code.data(), code.size());
}

VkShaderModule CompileAndCreateFragmentShader(const std::string& source_code)
{
  ShaderCompiler::SPIRVCodeVector code;
  if (!ShaderCompiler::CompileFragmentShader(&code, source_code.c_str(), source_code.length()))
    return VK_NULL_HANDLE;

  return CreateShaderModule(code.data(), code.size());
}

VkShaderModule CompileAndCreateComputeShader(const std::string& source_code)
{
  ShaderCompiler::SPIRVCodeVector code;
  if (!ShaderCompiler::CompileComputeShader(&code, source_code.c_str(), source_code.length()))
    return VK_NULL_HANDLE;

  return CreateShaderModule(code.data(), code.size());
}

}  // namespace Util

UtilityShaderDraw::UtilityShaderDraw(VkCommandBuffer command_buffer,
                                     VkPipelineLayout pipeline_layout, VkRenderPass render_pass,
                                     VkShaderModule vertex_shader, VkShaderModule geometry_shader,
                                     VkShaderModule pixel_shader)
    : m_command_buffer(command_buffer)
{
  // Populate minimal pipeline state
  m_pipeline_info.vertex_format = g_object_cache->GetUtilityShaderVertexFormat();
  m_pipeline_info.pipeline_layout = pipeline_layout;
  m_pipeline_info.render_pass = render_pass;
  m_pipeline_info.vs = vertex_shader;
  m_pipeline_info.gs = geometry_shader;
  m_pipeline_info.ps = pixel_shader;
  m_pipeline_info.rasterization_state.bits = Util::GetNoCullRasterizationState().bits;
  m_pipeline_info.depth_stencil_state.bits = Util::GetNoDepthTestingDepthStencilState().bits;
  m_pipeline_info.blend_state.hex = Util::GetNoBlendingBlendState().hex;
  m_pipeline_info.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

UtilityShaderVertex* UtilityShaderDraw::ReserveVertices(VkPrimitiveTopology topology, size_t count)
{
  m_pipeline_info.primitive_topology = topology;

  if (!g_object_cache->GetUtilityShaderVertexBuffer()->ReserveMemory(
          sizeof(UtilityShaderVertex) * count, sizeof(UtilityShaderVertex), true, true, true))
    PanicAlert("Failed to allocate space for vertices in backend shader");

  m_vertex_buffer = g_object_cache->GetUtilityShaderVertexBuffer()->GetBuffer();
  m_vertex_buffer_offset = g_object_cache->GetUtilityShaderVertexBuffer()->GetCurrentOffset();

  return reinterpret_cast<UtilityShaderVertex*>(
      g_object_cache->GetUtilityShaderVertexBuffer()->GetCurrentHostPointer());
}

void UtilityShaderDraw::CommitVertices(size_t count)
{
  g_object_cache->GetUtilityShaderVertexBuffer()->CommitMemory(sizeof(UtilityShaderVertex) * count);
  m_vertex_count = static_cast<uint32_t>(count);
}

void UtilityShaderDraw::UploadVertices(VkPrimitiveTopology topology, UtilityShaderVertex* vertices,
                                       size_t count)
{
  UtilityShaderVertex* upload_vertices = ReserveVertices(topology, count);
  memcpy(upload_vertices, vertices, sizeof(UtilityShaderVertex) * count);
  CommitVertices(count);
}

u8* UtilityShaderDraw::AllocateVSUniforms(size_t size)
{
  if (!g_object_cache->GetUtilityShaderUniformBuffer()->ReserveMemory(
          size, g_vulkan_context->GetUniformBufferAlignment(), true, true, true))
    PanicAlert("Failed to allocate util uniforms");

  return g_object_cache->GetUtilityShaderUniformBuffer()->GetCurrentHostPointer();
}

void UtilityShaderDraw::CommitVSUniforms(size_t size)
{
  m_vs_uniform_buffer.buffer = g_object_cache->GetUtilityShaderUniformBuffer()->GetBuffer();
  m_vs_uniform_buffer.offset = 0;
  m_vs_uniform_buffer.range = size;
  m_ubo_offsets[UBO_DESCRIPTOR_SET_BINDING_VS] =
      static_cast<uint32_t>(g_object_cache->GetUtilityShaderUniformBuffer()->GetCurrentOffset());

  g_object_cache->GetUtilityShaderUniformBuffer()->CommitMemory(size);
}

u8* UtilityShaderDraw::AllocatePSUniforms(size_t size)
{
  if (!g_object_cache->GetUtilityShaderUniformBuffer()->ReserveMemory(
          size, g_vulkan_context->GetUniformBufferAlignment(), true, true, true))
    PanicAlert("Failed to allocate util uniforms");

  return g_object_cache->GetUtilityShaderUniformBuffer()->GetCurrentHostPointer();
}

void UtilityShaderDraw::CommitPSUniforms(size_t size)
{
  m_ps_uniform_buffer.buffer = g_object_cache->GetUtilityShaderUniformBuffer()->GetBuffer();
  m_ps_uniform_buffer.offset = 0;
  m_ps_uniform_buffer.range = size;
  m_ubo_offsets[UBO_DESCRIPTOR_SET_BINDING_PS] =
      static_cast<uint32_t>(g_object_cache->GetUtilityShaderUniformBuffer()->GetCurrentOffset());

  g_object_cache->GetUtilityShaderUniformBuffer()->CommitMemory(size);
}

void UtilityShaderDraw::SetPushConstants(const void* data, size_t data_size)
{
  _assert_(static_cast<u32>(data_size) < PUSH_CONSTANT_BUFFER_SIZE);

  vkCmdPushConstants(m_command_buffer, m_pipeline_info.pipeline_layout,
                     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                     static_cast<u32>(data_size), data);
}

void UtilityShaderDraw::SetPSSampler(size_t index, VkImageView view, VkSampler sampler)
{
  m_ps_samplers[index].sampler = sampler;
  m_ps_samplers[index].imageView = view;
  m_ps_samplers[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void UtilityShaderDraw::SetPSTexelBuffer(VkBufferView view)
{
  // Should only be used with the texture conversion pipeline layout.
  _assert_(m_pipeline_info.pipeline_layout ==
           g_object_cache->GetPipelineLayout(PIPELINE_LAYOUT_TEXTURE_CONVERSION));

  m_ps_texel_buffer = view;
}

void UtilityShaderDraw::SetRasterizationState(const RasterizationState& state)
{
  m_pipeline_info.rasterization_state.bits = state.bits;
}

void UtilityShaderDraw::SetDepthStencilState(const DepthStencilState& state)
{
  m_pipeline_info.depth_stencil_state.bits = state.bits;
}

void UtilityShaderDraw::SetBlendState(const BlendingState& state)
{
  m_pipeline_info.blend_state.hex = state.hex;
}

void UtilityShaderDraw::BeginRenderPass(VkFramebuffer framebuffer, const VkRect2D& region,
                                        const VkClearValue* clear_value)
{
  VkRenderPassBeginInfo begin_info = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                      nullptr,
                                      m_pipeline_info.render_pass,
                                      framebuffer,
                                      region,
                                      clear_value ? 1u : 0u,
                                      clear_value};

  vkCmdBeginRenderPass(m_command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void UtilityShaderDraw::EndRenderPass()
{
  vkCmdEndRenderPass(m_command_buffer);
}

void UtilityShaderDraw::Draw()
{
  BindVertexBuffer();
  BindDescriptors();
  if (!BindPipeline())
    return;

  vkCmdDraw(m_command_buffer, m_vertex_count, 1, 0, 0);
}

void UtilityShaderDraw::DrawQuad(int x, int y, int width, int height, float z)
{
  UtilityShaderVertex vertices[4];
  vertices[0].SetPosition(-1.0f, 1.0f, z);
  vertices[0].SetTextureCoordinates(0.0f, 1.0f);
  vertices[0].SetColor(1.0f, 1.0f, 1.0f, 1.0f);
  vertices[1].SetPosition(1.0f, 1.0f, z);
  vertices[1].SetTextureCoordinates(1.0f, 1.0f);
  vertices[1].SetColor(1.0f, 1.0f, 1.0f, 1.0f);
  vertices[2].SetPosition(-1.0f, -1.0f, z);
  vertices[2].SetTextureCoordinates(0.0f, 0.0f);
  vertices[2].SetColor(1.0f, 1.0f, 1.0f, 1.0f);
  vertices[3].SetPosition(1.0f, -1.0f, z);
  vertices[3].SetTextureCoordinates(1.0f, 0.0f);
  vertices[3].SetColor(1.0f, 1.0f, 1.0f, 1.0f);

  Util::SetViewportAndScissor(m_command_buffer, x, y, width, height);
  UploadVertices(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, vertices, ArraySize(vertices));
  Draw();
}

void UtilityShaderDraw::DrawQuad(int dst_x, int dst_y, int dst_width, int dst_height, int src_x,
                                 int src_y, int src_layer, int src_width, int src_height,
                                 int src_full_width, int src_full_height, float z)
{
  float u0 = float(src_x) / float(src_full_width);
  float v0 = float(src_y) / float(src_full_height);
  float u1 = float(src_x + src_width) / float(src_full_width);
  float v1 = float(src_y + src_height) / float(src_full_height);
  float w = static_cast<float>(src_layer);

  UtilityShaderVertex vertices[4];
  vertices[0].SetPosition(-1.0f, 1.0f, z);
  vertices[0].SetTextureCoordinates(u0, v1, w);
  vertices[0].SetColor(1.0f, 1.0f, 1.0f, 1.0f);
  vertices[1].SetPosition(1.0f, 1.0f, z);
  vertices[1].SetTextureCoordinates(u1, v1, w);
  vertices[1].SetColor(1.0f, 1.0f, 1.0f, 1.0f);
  vertices[2].SetPosition(-1.0f, -1.0f, z);
  vertices[2].SetTextureCoordinates(u0, v0, w);
  vertices[2].SetColor(1.0f, 1.0f, 1.0f, 1.0f);
  vertices[3].SetPosition(1.0f, -1.0f, z);
  vertices[3].SetTextureCoordinates(u1, v0, w);
  vertices[3].SetColor(1.0f, 1.0f, 1.0f, 1.0f);

  Util::SetViewportAndScissor(m_command_buffer, dst_x, dst_y, dst_width, dst_height);
  UploadVertices(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, vertices, ArraySize(vertices));
  Draw();
}

void UtilityShaderDraw::DrawColoredQuad(int x, int y, int width, int height, float r, float g,
                                        float b, float a, float z)
{
  return DrawColoredQuad(x, y, width, height, Util::MakeRGBA8Color(r, g, b, a), z);
}

void UtilityShaderDraw::DrawColoredQuad(int x, int y, int width, int height, u32 color, float z)
{
  UtilityShaderVertex vertices[4];
  vertices[0].SetPosition(-1.0f, 1.0f, z);
  vertices[0].SetTextureCoordinates(0.0f, 1.0f);
  vertices[0].SetColor(color);
  vertices[1].SetPosition(1.0f, 1.0f, z);
  vertices[1].SetTextureCoordinates(1.0f, 1.0f);
  vertices[1].SetColor(color);
  vertices[2].SetPosition(-1.0f, -1.0f, z);
  vertices[2].SetTextureCoordinates(0.0f, 0.0f);
  vertices[2].SetColor(color);
  vertices[3].SetPosition(1.0f, -1.0f, z);
  vertices[3].SetTextureCoordinates(1.0f, 0.0f);
  vertices[3].SetColor(color);

  Util::SetViewportAndScissor(m_command_buffer, x, y, width, height);
  UploadVertices(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP, vertices, ArraySize(vertices));
  Draw();
}

void UtilityShaderDraw::SetViewportAndScissor(int x, int y, int width, int height)
{
  Util::SetViewportAndScissor(m_command_buffer, x, y, width, height, 0.0f, 1.0f);
}

void UtilityShaderDraw::DrawWithoutVertexBuffer(VkPrimitiveTopology primitive_topology,
                                                u32 vertex_count)
{
  m_pipeline_info.vertex_format = nullptr;
  m_pipeline_info.primitive_topology = primitive_topology;

  BindDescriptors();
  if (!BindPipeline())
    return;

  vkCmdDraw(m_command_buffer, vertex_count, 1, 0, 0);
}

void UtilityShaderDraw::BindVertexBuffer()
{
  vkCmdBindVertexBuffers(m_command_buffer, 0, 1, &m_vertex_buffer, &m_vertex_buffer_offset);
}

void UtilityShaderDraw::BindDescriptors()
{
  // TODO: This method is a mess, clean it up
  std::array<VkDescriptorSet, NUM_DESCRIPTOR_SET_BIND_POINTS> bind_descriptor_sets = {};
  std::array<VkWriteDescriptorSet, NUM_UBO_DESCRIPTOR_SET_BINDINGS + 1> set_writes = {};
  uint32_t num_set_writes = 0;

  VkDescriptorBufferInfo dummy_uniform_buffer = {
      g_object_cache->GetUtilityShaderUniformBuffer()->GetBuffer(), 0, 1};

  // uniform buffers
  if (m_vs_uniform_buffer.buffer != VK_NULL_HANDLE || m_ps_uniform_buffer.buffer != VK_NULL_HANDLE)
  {
    VkDescriptorSet set = g_command_buffer_mgr->AllocateDescriptorSet(
        g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_UNIFORM_BUFFERS));
    if (set == VK_NULL_HANDLE)
      PanicAlert("Failed to allocate descriptor set for utility draw");

    set_writes[num_set_writes++] = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, UBO_DESCRIPTOR_SET_BINDING_VS, 0, 1,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr,
        (m_vs_uniform_buffer.buffer != VK_NULL_HANDLE) ? &m_vs_uniform_buffer :
                                                         &dummy_uniform_buffer,
        nullptr};

    set_writes[num_set_writes++] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                    nullptr,
                                    set,
                                    UBO_DESCRIPTOR_SET_BINDING_GS,
                                    0,
                                    1,
                                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                    nullptr,
                                    &dummy_uniform_buffer,
                                    nullptr};

    set_writes[num_set_writes++] = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, UBO_DESCRIPTOR_SET_BINDING_PS, 0, 1,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, nullptr,
        (m_ps_uniform_buffer.buffer != VK_NULL_HANDLE) ? &m_ps_uniform_buffer :
                                                         &dummy_uniform_buffer,
        nullptr};

    bind_descriptor_sets[DESCRIPTOR_SET_LAYOUT_UNIFORM_BUFFERS] = set;
  }

  // PS samplers
  size_t first_active_sampler;
  for (first_active_sampler = 0; first_active_sampler < NUM_PIXEL_SHADER_SAMPLERS;
       first_active_sampler++)
  {
    if (m_ps_samplers[first_active_sampler].imageView != VK_NULL_HANDLE &&
        m_ps_samplers[first_active_sampler].sampler != VK_NULL_HANDLE)
    {
      break;
    }
  }

  // Check if we have any at all, skip the binding process entirely if we don't
  if (first_active_sampler != NUM_PIXEL_SHADER_SAMPLERS)
  {
    // We need to fill it with non-empty images.
    for (size_t i = 0; i < NUM_PIXEL_SHADER_SAMPLERS; i++)
    {
      if (m_ps_samplers[i].imageView == VK_NULL_HANDLE)
      {
        m_ps_samplers[i].imageView = g_object_cache->GetDummyImageView();
        m_ps_samplers[i].sampler = g_object_cache->GetPointSampler();
      }
    }

    // Allocate a new descriptor set
    VkDescriptorSet set = g_command_buffer_mgr->AllocateDescriptorSet(
        g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_PIXEL_SHADER_SAMPLERS));
    if (set == VK_NULL_HANDLE)
      PanicAlert("Failed to allocate descriptor set for utility draw");

    set_writes[num_set_writes++] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                    nullptr,
                                    set,
                                    0,
                                    0,
                                    static_cast<u32>(NUM_PIXEL_SHADER_SAMPLERS),
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                    m_ps_samplers.data(),
                                    nullptr,
                                    nullptr};

    bind_descriptor_sets[DESCRIPTOR_SET_BIND_POINT_PIXEL_SHADER_SAMPLERS] = set;
  }

  vkUpdateDescriptorSets(g_vulkan_context->GetDevice(), num_set_writes, set_writes.data(), 0,
                         nullptr);

  if (m_ps_texel_buffer != VK_NULL_HANDLE)
  {
    // TODO: Handle case where this fails.
    // This'll only be when we do over say, 1024 allocations per frame, which shouldn't happen.
    // TODO: Execute the command buffer, reset render passes and then try again.
    VkDescriptorSet set = g_command_buffer_mgr->AllocateDescriptorSet(
        g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_TEXEL_BUFFERS));
    if (set == VK_NULL_HANDLE)
    {
      PanicAlert("Failed to allocate texel buffer descriptor set for utility draw");
      return;
    }

    VkWriteDescriptorSet set_write = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                      nullptr,
                                      set,
                                      0,
                                      0,
                                      1,
                                      VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,
                                      nullptr,
                                      nullptr,
                                      &m_ps_texel_buffer};
    vkUpdateDescriptorSets(g_vulkan_context->GetDevice(), 1, &set_write, 0, nullptr);
    bind_descriptor_sets[DESCRIPTOR_SET_BIND_POINT_STORAGE_OR_TEXEL_BUFFER] = set;
  }

  // Fast path when there are no gaps in the set bindings
  u32 bind_point_index;
  for (bind_point_index = 0; bind_point_index < NUM_DESCRIPTOR_SET_BIND_POINTS; bind_point_index++)
  {
    if (bind_descriptor_sets[bind_point_index] == VK_NULL_HANDLE)
      break;
  }
  if (bind_point_index > 0)
  {
    // Bind the contiguous sets, any others after any gaps will be handled below
    vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipeline_info.pipeline_layout, 0, bind_point_index,
                            &bind_descriptor_sets[0], NUM_UBO_DESCRIPTOR_SET_BINDINGS,
                            m_ubo_offsets.data());
  }

  // Handle any remaining sets
  for (u32 i = bind_point_index; i < NUM_DESCRIPTOR_SET_BIND_POINTS; i++)
  {
    if (bind_descriptor_sets[i] == VK_NULL_HANDLE)
      continue;

    // No need to worry about dynamic offsets here, since #0 will always be bound above.
    vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipeline_info.pipeline_layout, i, 1, &bind_descriptor_sets[i], 0,
                            nullptr);
  }
}

bool UtilityShaderDraw::BindPipeline()
{
  VkPipeline pipeline = g_shader_cache->GetPipeline(m_pipeline_info);
  if (pipeline == VK_NULL_HANDLE)
  {
    PanicAlert("Failed to get pipeline for backend shader draw");
    return false;
  }

  vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
  return true;
}

ComputeShaderDispatcher::ComputeShaderDispatcher(VkCommandBuffer command_buffer,
                                                 VkPipelineLayout pipeline_layout,
                                                 VkShaderModule compute_shader)
    : m_command_buffer(command_buffer)
{
  // Populate minimal pipeline state
  m_pipeline_info.pipeline_layout = pipeline_layout;
  m_pipeline_info.cs = compute_shader;
}

u8* ComputeShaderDispatcher::AllocateUniformBuffer(size_t size)
{
  if (!g_object_cache->GetUtilityShaderUniformBuffer()->ReserveMemory(
          size, g_vulkan_context->GetUniformBufferAlignment(), true, true, true))
    PanicAlert("Failed to allocate util uniforms");

  return g_object_cache->GetUtilityShaderUniformBuffer()->GetCurrentHostPointer();
}

void ComputeShaderDispatcher::CommitUniformBuffer(size_t size)
{
  m_uniform_buffer.buffer = g_object_cache->GetUtilityShaderUniformBuffer()->GetBuffer();
  m_uniform_buffer.offset = 0;
  m_uniform_buffer.range = size;
  m_uniform_buffer_offset =
      static_cast<u32>(g_object_cache->GetUtilityShaderUniformBuffer()->GetCurrentOffset());

  g_object_cache->GetUtilityShaderUniformBuffer()->CommitMemory(size);
}

void ComputeShaderDispatcher::SetPushConstants(const void* data, size_t data_size)
{
  _assert_(static_cast<u32>(data_size) < PUSH_CONSTANT_BUFFER_SIZE);

  vkCmdPushConstants(m_command_buffer, m_pipeline_info.pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                     0, static_cast<u32>(data_size), data);
}

void ComputeShaderDispatcher::SetSampler(size_t index, VkImageView view, VkSampler sampler)
{
  m_samplers[index].sampler = sampler;
  m_samplers[index].imageView = view;
  m_samplers[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void ComputeShaderDispatcher::SetStorageImage(VkImageView view, VkImageLayout image_layout)
{
  m_storage_image.sampler = VK_NULL_HANDLE;
  m_storage_image.imageView = view;
  m_storage_image.imageLayout = image_layout;
}

void ComputeShaderDispatcher::SetTexelBuffer(size_t index, VkBufferView view)
{
  m_texel_buffers[index] = view;
}

void ComputeShaderDispatcher::Dispatch(u32 groups_x, u32 groups_y, u32 groups_z)
{
  BindDescriptors();
  if (!BindPipeline())
    return;

  vkCmdDispatch(m_command_buffer, groups_x, groups_y, groups_z);
}

void ComputeShaderDispatcher::BindDescriptors()
{
  VkDescriptorSet set = g_command_buffer_mgr->AllocateDescriptorSet(
      g_object_cache->GetDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_COMPUTE));
  if (set == VK_NULL_HANDLE)
  {
    PanicAlert("Failed to allocate descriptor set for compute dispatch");
    return;
  }

  // Reserve enough descriptors to write every binding.
  std::array<VkWriteDescriptorSet, 7> set_writes = {};
  u32 num_set_writes = 0;

  if (m_uniform_buffer.buffer != VK_NULL_HANDLE)
  {
    set_writes[num_set_writes++] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                    nullptr,
                                    set,
                                    0,
                                    0,
                                    1,
                                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                                    nullptr,
                                    &m_uniform_buffer,
                                    nullptr};
  }

  // Samplers
  for (size_t i = 0; i < m_samplers.size(); i++)
  {
    const VkDescriptorImageInfo& info = m_samplers[i];
    if (info.imageView != VK_NULL_HANDLE && info.sampler != VK_NULL_HANDLE)
    {
      set_writes[num_set_writes++] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                      nullptr,
                                      set,
                                      static_cast<u32>(1 + i),
                                      0,
                                      1,
                                      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                      &info,
                                      nullptr,
                                      nullptr};
    }
  }

  for (size_t i = 0; i < m_texel_buffers.size(); i++)
  {
    if (m_texel_buffers[i] != VK_NULL_HANDLE)
    {
      set_writes[num_set_writes++] = {
          VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,  nullptr, set,     5 + static_cast<u32>(i), 0, 1,
          VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, nullptr, nullptr, &m_texel_buffers[i]};
    }
  }

  if (m_storage_image.imageView != VK_NULL_HANDLE)
  {
    set_writes[num_set_writes++] = {
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,          set,     7,      0, 1,
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,       &m_storage_image, nullptr, nullptr};
  }

  if (num_set_writes > 0)
  {
    vkUpdateDescriptorSets(g_vulkan_context->GetDevice(), num_set_writes, set_writes.data(), 0,
                           nullptr);
  }

  vkCmdBindDescriptorSets(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          m_pipeline_info.pipeline_layout, 0, 1, &set, 1, &m_uniform_buffer_offset);
}

bool ComputeShaderDispatcher::BindPipeline()
{
  VkPipeline pipeline = g_shader_cache->GetComputePipeline(m_pipeline_info);
  if (pipeline == VK_NULL_HANDLE)
  {
    PanicAlert("Failed to get pipeline for backend compute dispatch");
    return false;
  }

  vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
  return true;
}

}  // namespace Vulkan
