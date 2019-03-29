// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "VideoBackends/D3D/D3DBase.h"
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
  std::unordered_map<SamplerState::StorageType, ComPtr<ID3D11SamplerState>> m_sampler;
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
      m_dirtyFlags |= DirtyFlag_BlendState;

    m_pending.blendState = state;
  }

  void SetDepthState(ID3D11DepthStencilState* state)
  {
    if (m_current.depthState != state)
      m_dirtyFlags |= DirtyFlag_DepthState;

    m_pending.depthState = state;
  }

  void SetRasterizerState(ID3D11RasterizerState* state)
  {
    if (m_current.rasterizerState != state)
      m_dirtyFlags |= DirtyFlag_RasterizerState;

    m_pending.rasterizerState = state;
  }

  void SetTexture(size_t index, ID3D11ShaderResourceView* texture)
  {
    if (m_current.textures[index] != texture)
      m_dirtyFlags |= DirtyFlag_Texture0 << index;

    m_pending.textures[index] = texture;
  }

  void SetSampler(size_t index, ID3D11SamplerState* sampler)
  {
    if (m_current.samplers[index] != sampler)
      m_dirtyFlags |= DirtyFlag_Sampler0 << index;

    m_pending.samplers[index] = sampler;
  }

  void SetPixelConstants(ID3D11Buffer* buffer0, ID3D11Buffer* buffer1 = nullptr)
  {
    if (m_current.pixelConstants[0] != buffer0 || m_current.pixelConstants[1] != buffer1)
      m_dirtyFlags |= DirtyFlag_PixelConstants;

    m_pending.pixelConstants[0] = buffer0;
    m_pending.pixelConstants[1] = buffer1;
  }

  void SetVertexConstants(ID3D11Buffer* buffer)
  {
    if (m_current.vertexConstants != buffer)
      m_dirtyFlags |= DirtyFlag_VertexConstants;

    m_pending.vertexConstants = buffer;
  }

  void SetGeometryConstants(ID3D11Buffer* buffer)
  {
    if (m_current.geometryConstants != buffer)
      m_dirtyFlags |= DirtyFlag_GeometryConstants;

    m_pending.geometryConstants = buffer;
  }

  void SetVertexBuffer(ID3D11Buffer* buffer, u32 stride, u32 offset)
  {
    if (m_current.vertexBuffer != buffer || m_current.vertexBufferStride != stride ||
        m_current.vertexBufferOffset != offset)
      m_dirtyFlags |= DirtyFlag_VertexBuffer;

    m_pending.vertexBuffer = buffer;
    m_pending.vertexBufferStride = stride;
    m_pending.vertexBufferOffset = offset;
  }

  void SetIndexBuffer(ID3D11Buffer* buffer)
  {
    if (m_current.indexBuffer != buffer)
      m_dirtyFlags |= DirtyFlag_IndexBuffer;

    m_pending.indexBuffer = buffer;
  }

  void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology)
  {
    if (m_current.topology != topology)
      m_dirtyFlags |= DirtyFlag_InputAssembler;

    m_pending.topology = topology;
  }

  void SetInputLayout(ID3D11InputLayout* layout)
  {
    if (m_current.inputLayout != layout)
      m_dirtyFlags |= DirtyFlag_InputAssembler;

    m_pending.inputLayout = layout;
  }

  void SetPixelShader(ID3D11PixelShader* shader)
  {
    if (m_current.pixelShader != shader)
      m_dirtyFlags |= DirtyFlag_PixelShader;

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
      m_dirtyFlags |= DirtyFlag_VertexShader;

    m_pending.vertexShader = shader;
  }

  void SetGeometryShader(ID3D11GeometryShader* shader)
  {
    if (m_current.geometryShader != shader)
      m_dirtyFlags |= DirtyFlag_GeometryShader;

    m_pending.geometryShader = shader;
  }

  void SetFramebuffer(DXFramebuffer* fb)
  {
    if (m_current.framebuffer != fb)
      m_dirtyFlags |= DirtyFlag_Framebuffer;

    m_pending.framebuffer = fb;
  }

  void SetOMUAV(ID3D11UnorderedAccessView* uav)
  {
    if (m_current.uav != uav)
      m_dirtyFlags |= DirtyFlag_Framebuffer;

    m_pending.uav = uav;
  }

  void SetIntegerRTV(bool enable)
  {
    if (m_current.use_integer_rtv != enable)
      m_dirtyFlags |= DirtyFlag_Framebuffer;

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
    DirtyFlag_Texture0 = 1 << 0,
    DirtyFlag_Texture1 = 1 << 1,
    DirtyFlag_Texture2 = 1 << 2,
    DirtyFlag_Texture3 = 1 << 3,
    DirtyFlag_Texture4 = 1 << 4,
    DirtyFlag_Texture5 = 1 << 5,
    DirtyFlag_Texture6 = 1 << 6,
    DirtyFlag_Texture7 = 1 << 7,

    DirtyFlag_Sampler0 = 1 << 8,
    DirtyFlag_Sampler1 = 1 << 9,
    DirtyFlag_Sampler2 = 1 << 10,
    DirtyFlag_Sampler3 = 1 << 11,
    DirtyFlag_Sampler4 = 1 << 12,
    DirtyFlag_Sampler5 = 1 << 13,
    DirtyFlag_Sampler6 = 1 << 14,
    DirtyFlag_Sampler7 = 1 << 15,

    DirtyFlag_PixelConstants = 1 << 16,
    DirtyFlag_VertexConstants = 1 << 17,
    DirtyFlag_GeometryConstants = 1 << 18,

    DirtyFlag_VertexBuffer = 1 << 19,
    DirtyFlag_IndexBuffer = 1 << 20,

    DirtyFlag_PixelShader = 1 << 21,
    DirtyFlag_VertexShader = 1 << 22,
    DirtyFlag_GeometryShader = 1 << 23,

    DirtyFlag_InputAssembler = 1 << 24,
    DirtyFlag_BlendState = 1 << 25,
    DirtyFlag_DepthState = 1 << 26,
    DirtyFlag_RasterizerState = 1 << 27,
    DirtyFlag_Framebuffer = 1 << 28
  };

  u32 m_dirtyFlags = ~0u;

  struct Resources
  {
    std::array<ID3D11ShaderResourceView*, 8> textures;
    std::array<ID3D11SamplerState*, 8> samplers;
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
