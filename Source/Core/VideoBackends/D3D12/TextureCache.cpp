// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D12/TextureCache.h"
#include "VideoBackends/D3D12/AbstractTexture.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DDescriptorHeapManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/D3DState.h"
#include "VideoBackends/D3D12/D3DStreamBuffer.h"
#include "VideoBackends/D3D12/D3DUtil.h"
#include "VideoBackends/D3D12/FramebufferManager.h"
#include "VideoBackends/D3D12/PSTextureEncoder.h"
#include "VideoBackends/D3D12/StaticShaderCache.h"
#include "VideoBackends/D3D12/TextureEncoder.h"
#include "VideoCommon/ImageWrite.h"
#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{
static std::unique_ptr<TextureEncoder> s_encoder = nullptr;

std::unique_ptr<D3DStreamBuffer> s_efb_copy_stream_buffer = nullptr;
u32 s_efb_copy_last_cbuf_id = UINT_MAX;

ID3D12Resource* s_texture_cache_entry_readback_buffer = nullptr;
size_t s_texture_cache_entry_readback_buffer_size = 0;

void TextureCache::CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row,
                           u32 num_blocks_y, u32 memory_stride, PEControl::PixelFormat srcFormat,
                           const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf)
{
  s_encoder->Encode(dst, format, native_width, bytes_per_row, num_blocks_y, memory_stride,
                    srcFormat, srcRect, isIntensity, scaleByHalf);
}

static const constexpr char s_palette_shader_hlsl[] =
    R"HLSL(
sampler samp0 : register(s0);
Texture2DArray Tex0 : register(t0);
Buffer<uint> Tex1 : register(t1);
uniform float Multiply;

uint Convert3To8(uint v)
{
	// Swizzle bits: 00000123 -> 12312312
	return (v << 5) | (v << 2) | (v >> 1);
}

uint Convert4To8(uint v)
{
	// Swizzle bits: 00001234 -> 12341234
	return (v << 4) | v;
}

uint Convert5To8(uint v)
{
	// Swizzle bits: 00012345 -> 12345123
	return (v << 3) | (v >> 2);
}

uint Convert6To8(uint v)
{
	// Swizzle bits: 00123456 -> 12345612
	return (v << 2) | (v >> 4);
}

float4 DecodePixel_RGB5A3(uint val)
{
	int r,g,b,a;
	if ((val&0x8000))
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
	return float4(r, g, b, a) / 255;
}

float4 DecodePixel_RGB565(uint val)
{
	int r, g, b, a;
	r = Convert5To8((val >> 11) & 0x1f);
	g = Convert6To8((val >> 5) & 0x3f);
	b = Convert5To8((val) & 0x1f);
	a = 0xFF;
	return float4(r, g, b, a) / 255;
}

float4 DecodePixel_IA8(uint val)
{
	int i = val & 0xFF;
	int a = val >> 8;
	return float4(i, i, i, a) / 255;
}

void main(
	out float4 ocol0 : SV_Target,
	in float4 pos : SV_Position,
	in float3 uv0 : TEXCOORD0)
{
	uint src = round(Tex0.Sample(samp0,uv0) * Multiply).r;
	src = Tex1.Load(src);
	src = ((src << 8) & 0xFF00) | (src >> 8);
	ocol0 = DECODE(src);
}
)HLSL";

void TextureCache::ConvertTexture(TCacheEntry* dest, TCacheEntry* source, void* palette,
                                  TlutFormat format)
{
  const unsigned int palette_buffer_allocation_size = 512;
  m_palette_stream_buffer->AllocateSpaceInBuffer(palette_buffer_allocation_size, 256);
  memcpy(m_palette_stream_buffer->GetCPUAddressOfCurrentAllocation(), palette,
         palette_buffer_allocation_size);

  AbstractTexture* dest_tex = static_cast<AbstractTexture*>(dest->texture.get());
  const AbstractTexture* source_tex = static_cast<AbstractTexture*>(source->texture.get());

  // stretch picture with increased internal resolution
  D3D::SetViewportAndScissor(0, 0, source->width(), source->height());

  // D3D12: Because the second SRV slot is occupied by this buffer, and an arbitrary texture
  // occupies the first SRV slot,
  // we need to allocate temporary space out of our descriptor heap, place the palette SRV in the
  // second slot, then copy the
  // existing texture's descriptor into the first slot.

  // First, allocate the (temporary) space in the descriptor heap.
  D3D12_CPU_DESCRIPTOR_HANDLE srv_group_cpu_handle[2] = {};
  D3D12_GPU_DESCRIPTOR_HANDLE srv_group_gpu_handle[2] = {};
  D3D::gpu_descriptor_heap_mgr->AllocateGroup(srv_group_cpu_handle, 2, srv_group_gpu_handle,
                                              nullptr, true);

  srv_group_cpu_handle[1].ptr = srv_group_cpu_handle[0].ptr + D3D::resource_descriptor_size;

  // Now, create the palette SRV at the appropriate offset.
  D3D12_SHADER_RESOURCE_VIEW_DESC palette_buffer_srv_desc = {
      DXGI_FORMAT_R16_UINT,                     // DXGI_FORMAT Format;
      D3D12_SRV_DIMENSION_BUFFER,               // D3D12_SRV_DIMENSION ViewDimension;
      D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING  // UINT Shader4ComponentMapping;
  };

  // Each 'element' is two bytes since format is R16.
  palette_buffer_srv_desc.Buffer.FirstElement =
      m_palette_stream_buffer->GetOffsetOfCurrentAllocation() / sizeof(u16);
  palette_buffer_srv_desc.Buffer.NumElements = 256;

  D3D::device12->CreateShaderResourceView(m_palette_stream_buffer->GetBuffer(),
                                          &palette_buffer_srv_desc, srv_group_cpu_handle[1]);

  // Now, copy the existing texture's descriptor into the new temporary location.
  source_tex->m_texture->TransitionToResourceState(D3D::current_command_list,
                                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  D3D::device12->CopyDescriptorsSimple(1, srv_group_cpu_handle[0],
                                       source_tex->m_texture->GetSRV12GPUCPUShadow(),
                                       D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  // Finally, bind our temporary location.
  D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV,
                                                            srv_group_gpu_handle[0]);

  // D3D11EXISTINGTODO: Add support for C14X2 format.  (Different multiplier, more palette entries.)

  // D3D12: See TextureCache::TextureCache() - because there are only two possible buffer contents
  // here,
  // just pre-populate the data in two parts of the same upload heap.
  if ((source->format & 0xf) == GX_TF_I4)
  {
    D3D::current_command_list->SetGraphicsRootConstantBufferView(
        DESCRIPTOR_TABLE_PS_CBVONE, m_palette_uniform_buffer->GetGPUVirtualAddress());
  }
  else
  {
    D3D::current_command_list->SetGraphicsRootConstantBufferView(
        DESCRIPTOR_TABLE_PS_CBVONE, m_palette_uniform_buffer->GetGPUVirtualAddress() + 256);
  }

  D3D::command_list_mgr->SetCommandListDirtyState(COMMAND_LIST_STATE_PS_CBV, true);

  const D3D12_RECT source_rect = CD3DX12_RECT(0, 0, source->width(), source->height());

  D3D::SetPointCopySampler();

  // Make sure we don't draw with the texture set as both a source and target.
  // (This can happen because we don't unbind textures when we free them.)

  dest_tex->m_texture->TransitionToResourceState(D3D::current_command_list,
                                                 D3D12_RESOURCE_STATE_RENDER_TARGET);
  D3D::current_command_list->OMSetRenderTargets(1, &source_tex->m_texture->GetRTV12(), FALSE,
                                                nullptr);

  // Create texture copy
  D3D::DrawShadedTexQuad(source_tex->m_texture, &source_rect, source->width(), source->height(),
                         m_palette_pixel_shaders[format],
                         StaticShaderCache::GetSimpleVertexShader(),
                         StaticShaderCache::GetSimpleVertexShaderInputLayout(),
                         StaticShaderCache::GetCopyGeometryShader(), 1.0f, 0,
                         DXGI_FORMAT_R8G8B8A8_UNORM, true, dest_tex->m_texture->GetMultisampled());

  dest_tex->m_texture->TransitionToResourceState(D3D::current_command_list,
                                                 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

  FramebufferManager::GetEFBColorTexture()->TransitionToResourceState(
      D3D::current_command_list, D3D12_RESOURCE_STATE_RENDER_TARGET);
  FramebufferManager::GetEFBDepthTexture()->TransitionToResourceState(
      D3D::current_command_list, D3D12_RESOURCE_STATE_DEPTH_WRITE);

  g_renderer->RestoreAPIState();
}

D3D12_SHADER_BYTECODE GetConvertShader12(std::string& Type)
{
  std::string shader = "#define DECODE DecodePixel_";
  shader.append(Type);
  shader.append("\n");
  shader.append(s_palette_shader_hlsl);

  ID3DBlob* blob = nullptr;
  D3D::CompilePixelShader(shader, &blob);

  return {blob->GetBufferPointer(), blob->GetBufferSize()};
}

TextureCache::TextureCache()
{
  s_encoder = std::make_unique<PSTextureEncoder>();
  s_encoder->Init();

  s_efb_copy_stream_buffer = std::make_unique<D3DStreamBuffer>(1024 * 1024, 1024 * 1024, nullptr);
  s_efb_copy_last_cbuf_id = UINT_MAX;

  s_texture_cache_entry_readback_buffer = nullptr;
  s_texture_cache_entry_readback_buffer_size = 0;

  m_palette_pixel_shaders[GX_TL_IA8] = GetConvertShader12(std::string("IA8"));
  m_palette_pixel_shaders[GX_TL_RGB565] = GetConvertShader12(std::string("RGB565"));
  m_palette_pixel_shaders[GX_TL_RGB5A3] = GetConvertShader12(std::string("RGB5A3"));

  m_palette_stream_buffer = std::make_unique<D3DStreamBuffer>(
      sizeof(u16) * 256 * 1024, sizeof(u16) * 256 * 1024 * 16, nullptr);

  // Right now, there are only two variants of palette_uniform data. So, we'll just create an upload
  // heap to permanently store both of these.
  CheckHR(D3D::device12->CreateCommittedResource(
      &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
      &CD3DX12_RESOURCE_DESC::Buffer(
          ((16 + 255) & ~255) *
          2),  // Constant Buffers have to be 256b aligned. "* 2" to create for two sets of data.
      D3D12_RESOURCE_STATE_COPY_DEST,
      nullptr, IID_PPV_ARGS(&m_palette_uniform_buffer)));

  D3D::SetDebugObjectName12(m_palette_uniform_buffer,
                            "a constant buffer used in TextureCache::ConvertTexture");

  // Temporarily repurpose m_palette_stream_buffer as a copy source to populate initial data here.
  m_palette_stream_buffer->AllocateSpaceInBuffer(256 * 2, 256);

  u8* upload_heap_data_location =
      reinterpret_cast<u8*>(m_palette_stream_buffer->GetCPUAddressOfCurrentAllocation());

  memset(upload_heap_data_location, 0, 256 * 2);

  float paramsFormatZero[4] = {15.f};
  float paramsFormatNonzero[4] = {255.f};

  memcpy(upload_heap_data_location, paramsFormatZero, sizeof(paramsFormatZero));
  memcpy(upload_heap_data_location + 256, paramsFormatNonzero, sizeof(paramsFormatNonzero));

  D3D::current_command_list->CopyBufferRegion(
      m_palette_uniform_buffer, 0, m_palette_stream_buffer->GetBuffer(),
      m_palette_stream_buffer->GetOffsetOfCurrentAllocation(), 256 * 2);

  DX12::D3D::ResourceBarrier(D3D::current_command_list, m_palette_uniform_buffer,
                             D3D12_RESOURCE_STATE_COPY_DEST,
                             D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 0);
}

TextureCache::~TextureCache()
{
  s_encoder->Shutdown();
  s_encoder.reset();

  s_efb_copy_stream_buffer.reset();

  if (s_texture_cache_entry_readback_buffer)
  {
    // Safe to destroy the readback buffer immediately, as the only time it's used is blocked until
    // completion.
    s_texture_cache_entry_readback_buffer->Release();
    s_texture_cache_entry_readback_buffer = nullptr;
    s_texture_cache_entry_readback_buffer_size = 0;
  }

  D3D::command_list_mgr->DestroyResourceAfterCurrentCommandListExecuted(m_palette_uniform_buffer);
}

void TextureCache::BindTextures()
{
  unsigned int last_texture = 0;
  for (unsigned int i = 0; i < 8; ++i)
  {
    if (bound_textures[i] != nullptr)
    {
      last_texture = i;
    }
  }

  if (last_texture == 0 && bound_textures[0] != nullptr)
  {
    auto* texture = static_cast<AbstractTexture*>(bound_textures[0]->texture.get());
    DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(
        DESCRIPTOR_TABLE_PS_SRV, texture->m_texture_srv_gpu_handle);
    return;
  }

  // If more than one texture, allocate space for group.
  D3D12_CPU_DESCRIPTOR_HANDLE s_group_base_texture_cpu_handle;
  D3D12_GPU_DESCRIPTOR_HANDLE s_group_base_texture_gpu_handle;
  DX12::D3D::gpu_descriptor_heap_mgr->AllocateGroup(
      &s_group_base_texture_cpu_handle, 8, &s_group_base_texture_gpu_handle, nullptr, true);

  for (unsigned int stage = 0; stage < 8; stage++)
  {
    if (bound_textures[stage] != nullptr)
    {
      D3D12_CPU_DESCRIPTOR_HANDLE textureDestDescriptor;
      textureDestDescriptor.ptr =
          s_group_base_texture_cpu_handle.ptr + stage * D3D::resource_descriptor_size;
      auto* texture = static_cast<AbstractTexture*>(bound_textures[0]->texture.get());

      DX12::D3D::device12->CopyDescriptorsSimple(1, textureDestDescriptor,
                                                 texture->m_texture_srv_gpu_handle_cpu_shadow,
                                                 D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    else
    {
      D3D12_CPU_DESCRIPTOR_HANDLE nullDestDescriptor;
      nullDestDescriptor.ptr =
          s_group_base_texture_cpu_handle.ptr + stage * D3D::resource_descriptor_size;

      DX12::D3D::device12->CopyDescriptorsSimple(1, nullDestDescriptor,
                                                 DX12::D3D::null_srv_cpu_shadow,
                                                 D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
  }

  // Actually bind the textures.
  DX12::D3D::current_command_list->SetGraphicsRootDescriptorTable(DESCRIPTOR_TABLE_PS_SRV,
                                                                  s_group_base_texture_gpu_handle);
}

std::unique_ptr<AbstractTextureBase>
TextureCache::CreateTexture(const AbstractTextureBase::TextureConfig& config)
{
  return std::make_unique<AbstractTexture>(config);
}
}
