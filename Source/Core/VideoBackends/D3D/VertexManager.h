// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <memory>
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
  void SetInputLayout(D3DBlob* vs_bytecode);

private:
  std::array<D3D11_INPUT_ELEMENT_DESC, 32> m_elems{};
  UINT m_num_elems = 0;

  ID3D11InputLayout* m_layout = nullptr;
};

class VertexManager : public VertexManagerBase
{
public:
  VertexManager();
  ~VertexManager();

  std::unique_ptr<NativeVertexFormat>
  CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) override;

  void CreateDeviceObjects() override;
  void DestroyDeviceObjects() override;

protected:
  void ResetBuffer(u32 stride) override;
  u16* GetIndexBuffer() { return &LocalIBuffer[0]; }
private:
  void PrepareDrawBuffers(u32 stride);
  void Draw(u32 stride);
  // temp
  void vFlush() override;

  u32 m_vertexDrawOffset;
  u32 m_indexDrawOffset;
  u32 m_currentBuffer;
  u32 m_bufferCursor;

  enum
  {
    MAX_BUFFER_COUNT = 2
  };
  ID3D11Buffer* m_buffers[MAX_BUFFER_COUNT];

  std::vector<u8> LocalVBuffer;
  std::vector<u16> LocalIBuffer;

  std::vector<u8> LocalVReplayBuffer;
  std::vector<u16> LocalIReplayBuffer;
};

}  // namespace
