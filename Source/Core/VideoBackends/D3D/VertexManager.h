// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexManagerBase.h"

struct ID3D11Buffer;

namespace DX11
{
class D3DBlob;
class D3DVertexFormat : public NativeVertexFormat
{
public:
  D3DVertexFormat(const PortableVertexDeclaration& vtx_decl);
  ~D3DVertexFormat();
  ID3D11InputLayout* GetInputLayout(D3DBlob* vs_bytecode);

private:
  std::array<D3D11_INPUT_ELEMENT_DESC, 32> m_elems{};
  UINT m_num_elems = 0;

  std::atomic<ID3D11InputLayout*> m_layout{nullptr};
};

class VertexManager : public VertexManagerBase
{
public:
  VertexManager();
  ~VertexManager();

  std::unique_ptr<NativeVertexFormat>
  CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) override;

  void UploadUtilityUniforms(const void* uniforms, u32 uniforms_size) override;

protected:
  void CreateDeviceObjects() override;
  void DestroyDeviceObjects() override;
  void ResetBuffer(u32 vertex_stride, bool cull_all) override;
  void CommitBuffer(u32 num_vertices, u32 vertex_stride, u32 num_indices, u32* out_base_vertex,
                    u32* out_base_index) override;
  void UploadConstants() override;
  void DrawCurrentBatch(u32 base_index, u32 num_indices, u32 base_vertex) override;

private:
  enum
  {
    MAX_BUFFER_COUNT = 2
  };
  ID3D11Buffer* m_buffers[MAX_BUFFER_COUNT] = {};
  u32 m_current_buffer = 0;
  u32 m_buffer_cursor = 0;

  std::vector<u8> m_staging_vertex_buffer;
  std::vector<u16> m_staging_index_buffer;

  ID3D11Buffer* m_vertex_constant_buffer = nullptr;
  ID3D11Buffer* m_geometry_constant_buffer = nullptr;
  ID3D11Buffer* m_pixel_constant_buffer = nullptr;
};

}  // namespace DX11
