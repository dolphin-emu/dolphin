// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoCommon/Constants.h"
#include "VideoCommon/RenderState.h"

namespace DX11
{
class DXFramebuffer;

class StateCache
{
public:
  ~StateCache();

  // Get existing or create new render state.
  // Returned objects is owned by the cache and does not need to be released.
  ID3D11SamplerState* Get(SamplerState state);
  ID3D11BlendState* Get(BlendingState state);
  ID3D11RasterizerState* Get(RasterizationState state);
  ID3D11DepthStencilState* Get(DepthState state);

  // Convert RasterState primitive type to D3D11 primitive topology.
  static D3D11_PRIMITIVE_TOPOLOGY GetPrimitiveTopology(PrimitiveType primitive);

private:
  std::unordered_map<u32, ComPtr<ID3D11DepthStencilState>> m_depth;
  std::unordered_map<u32, ComPtr<ID3D11RasterizerState>> m_raster;
  std::unordered_map<u32, ComPtr<ID3D11BlendState>> m_blend;
  std::unordered_map<SamplerState, ComPtr<ID3D11SamplerState>> m_sampler;
  std::mutex m_lock;
};

namespace D3D
{
class StateManager
{
public:
  StateManager();
  ~StateManager();

  void SetBlendState(ID3D11BlendState* state)
  {
    if (m_current.blendState != state)
      m_dirtyFlags.set(DirtyFlag_BlendState);

    m_pending.blendState = state;
  }

  void SetDepthState(ID3D11DepthStencilState* state)
  {
    if (m_current.depthState != state)
      m_dirtyFlags.set(DirtyFlag_DepthState);

    m_pending.depthState = state;
  }

  void SetRasterizerState(ID3D11RasterizerState* state)
  {
    if (m_current.rasterizerState != state)
      m_dirtyFlags.set(DirtyFlag_RasterizerState);

    m_pending.rasterizerState = state;
  }

  void SetTexture(size_t index, ID3D11ShaderResourceView* texture)
  {
    if (m_current.textures[index] != texture)
      m_dirtyFlags.set(DirtyFlag_Texture0 + index);

    m_pending.textures[index] = texture;
  }

  void SetSampler(size_t index, ID3D11SamplerState* sampler)
  {
    if (m_current.samplers[index] != sampler)
      m_dirtyFlags.set(DirtyFlag_Sampler0 + index);

    m_pending.samplers[index] = sampler;
  }

  void SetPixelConstants(ID3D11Buffer* buffer0, ID3D11Buffer* buffer1 = nullptr)
  {
    if (m_current.pixelConstants[0] != buffer0 || m_current.pixelConstants[1] != buffer1)
      m_dirtyFlags.set(DirtyFlag_PixelConstants);

    m_pending.pixelConstants[0] = buffer0;
    m_pending.pixelConstants[1] = buffer1;
  }

  void SetVertexConstants(ID3D11Buffer* buffer)
  {
    if (m_current.vertexConstants != buffer)
      m_dirtyFlags.set(DirtyFlag_VertexConstants);

    m_pending.vertexConstants = buffer;
  }

  void SetGeometryConstants(ID3D11Buffer* buffer)
  {
    if (m_current.geometryConstants != buffer)
      m_dirtyFlags.set(DirtyFlag_GeometryConstants);

    m_pending.geometryConstants = buffer;
  }

  void SetVertexBuffer(ID3D11Buffer* buffer, u32 stride, u32 offset)
  {
    if (m_current.vertexBuffer != buffer || m_current.vertexBufferStride != stride ||
        m_current.vertexBufferOffset != offset)
      m_dirtyFlags.set(DirtyFlag_VertexBuffer);

    m_pending.vertexBuffer = buffer;
    m_pending.vertexBufferStride = stride;
    m_pending.vertexBufferOffset = offset;
  }

  void SetIndexBuffer(ID3D11Buffer* buffer)
  {
    if (m_current.indexBuffer != buffer)
      m_dirtyFlags.set(DirtyFlag_IndexBuffer);

    m_pending.indexBuffer = buffer;
  }

  void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology)
  {
    if (m_current.topology != topology)
      m_dirtyFlags.set(DirtyFlag_InputAssembler);

    m_pending.topology = topology;
  }

  void SetInputLayout(ID3D11InputLayout* layout)
  {
    if (m_current.inputLayout != layout)
      m_dirtyFlags.set(DirtyFlag_InputAssembler);

    m_pending.inputLayout = layout;
  }

  void SetPixelShader(ID3D11PixelShader* shader)
  {
    if (m_current.pixelShader != shader)
      m_dirtyFlags.set(DirtyFlag_PixelShader);

    m_pending.pixelShader = shader;
  }

  void SetPixelShaderDynamic(ID3D11PixelShader* shader, ID3D11ClassInstance* const* classInstances,
                             u32 classInstancesCount)
  {
    D3D::context->PSSetShader(shader, classInstances, classInstancesCount);
    m_current.pixelShader = shader;
    m_pending.pixelShader = shader;
  }

  void SetVertexShader(ID3D11VertexShader* shader)
  {
    if (m_current.vertexShader != shader)
      m_dirtyFlags.set(DirtyFlag_VertexShader);

    m_pending.vertexShader = shader;
  }

  void SetGeometryShader(ID3D11GeometryShader* shader)
  {
    if (m_current.geometryShader != shader)
      m_dirtyFlags.set(DirtyFlag_GeometryShader);

    m_pending.geometryShader = shader;
  }

  void SetFramebuffer(DXFramebuffer* fb)
  {
    if (m_current.framebuffer != fb)
      m_dirtyFlags.set(DirtyFlag_Framebuffer);

    m_pending.framebuffer = fb;
  }

  void SetOMUAV(ID3D11UnorderedAccessView* uav)
  {
    if (m_current.uav != uav)
      m_dirtyFlags.set(DirtyFlag_Framebuffer);

    m_pending.uav = uav;
  }

  void SetIntegerRTV(bool enable)
  {
    if (m_current.use_integer_rtv != enable)
      m_dirtyFlags.set(DirtyFlag_Framebuffer);

    m_pending.use_integer_rtv = enable;
  }

  // removes currently set texture from all slots, returns mask of previously bound slots
  u32 UnsetTexture(ID3D11ShaderResourceView* srv);
  void SetTextureByMask(u32 textureSlotMask, ID3D11ShaderResourceView* srv);
  void ApplyTextures();

  // call this immediately before any drawing operation or to explicitly apply pending resource
  // state changes
  void Apply();

  // Binds constant buffers/textures/samplers to the compute shader stage.
  // We don't track these explicitly because it's not often-used.
  void SetComputeUAV(ID3D11UnorderedAccessView* uav);
  void SetComputeShader(ID3D11ComputeShader* shader);
  void SyncComputeBindings();

private:
  enum DirtyFlags
  {
    DirtyFlag_Texture0 = 0,
    DirtyFlag_Sampler0 = DirtyFlag_Texture0 + VideoCommon::MAX_PIXEL_SHADER_SAMPLERS,
    DirtyFlag_PixelConstants = DirtyFlag_Sampler0 + VideoCommon::MAX_PIXEL_SHADER_SAMPLERS,
    DirtyFlag_VertexConstants,
    DirtyFlag_GeometryConstants,

    DirtyFlag_VertexBuffer,
    DirtyFlag_IndexBuffer,

    DirtyFlag_PixelShader,
    DirtyFlag_VertexShader,
    DirtyFlag_GeometryShader,

    DirtyFlag_InputAssembler,
    DirtyFlag_BlendState,
    DirtyFlag_DepthState,
    DirtyFlag_RasterizerState,
    DirtyFlag_Framebuffer,
    DirtyFlag_Max
  };

  std::bitset<DirtyFlags::DirtyFlag_Max> m_dirtyFlags;

  struct Resources
  {
    std::array<ID3D11ShaderResourceView*, VideoCommon::MAX_PIXEL_SHADER_SAMPLERS> textures;
    std::array<ID3D11SamplerState*, VideoCommon::MAX_PIXEL_SHADER_SAMPLERS> samplers;
    std::array<ID3D11Buffer*, 2> pixelConstants;
    ID3D11Buffer* vertexConstants;
    ID3D11Buffer* geometryConstants;
    ID3D11Buffer* vertexBuffer;
    ID3D11Buffer* indexBuffer;
    u32 vertexBufferStride;
    u32 vertexBufferOffset;
    D3D11_PRIMITIVE_TOPOLOGY topology;
    ID3D11InputLayout* inputLayout;
    ID3D11PixelShader* pixelShader;
    ID3D11VertexShader* vertexShader;
    ID3D11GeometryShader* geometryShader;
    ID3D11BlendState* blendState;
    ID3D11DepthStencilState* depthState;
    ID3D11RasterizerState* rasterizerState;
    DXFramebuffer* framebuffer;
    ID3D11UnorderedAccessView* uav;
    bool use_integer_rtv;
  };

  Resources m_pending = {};
  Resources m_current = {};

  // Compute resources are synced with the graphics resources when we need them.
  ID3D11Buffer* m_compute_constants = nullptr;
  std::array<ID3D11ShaderResourceView*, 8> m_compute_textures{};
  std::array<ID3D11SamplerState*, 8> m_compute_samplers{};
  ID3D11UnorderedAccessView* m_compute_image = nullptr;
  ID3D11ComputeShader* m_compute_shader = nullptr;
};

extern std::unique_ptr<StateManager> stateman;

}  // namespace D3D

}  // namespace DX11
