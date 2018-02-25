// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <d3d11.h>
#include <memory>
#include "Common/CommonTypes.h"

#include "VideoBackends/D3D/D3DBlob.h"
#include "VideoCommon/AbstractShader.h"

namespace DX11
{
class DXShader final : public AbstractShader
{
public:
  // Note: vs/gs/ps/cs references are transferred.
  DXShader(D3DBlob* bytecode, ID3D11VertexShader* vs);
  DXShader(D3DBlob* bytecode, ID3D11GeometryShader* gs);
  DXShader(D3DBlob* bytecode, ID3D11PixelShader* ps);
  DXShader(D3DBlob* bytecode, ID3D11ComputeShader* cs);
  ~DXShader() override;

  D3DBlob* GetByteCode() const;
  ID3D11VertexShader* GetD3DVertexShader() const;
  ID3D11GeometryShader* GetD3DGeometryShader() const;
  ID3D11PixelShader* GetD3DPixelShader() const;
  ID3D11ComputeShader* GetD3DComputeShader() const;

  bool HasBinary() const override;
  BinaryData GetBinary() const override;

  // Creates a new shader object. The reference to bytecode is not transfered upon failure.
  static std::unique_ptr<DXShader> CreateFromBlob(ShaderStage stage, D3DBlob* bytecode);
  static std::unique_ptr<DXShader> CreateFromBinary(ShaderStage stage, const void* data,
                                                    size_t length);
  static std::unique_ptr<DXShader> CreateFromSource(ShaderStage stage, const char* source,
                                                    size_t length);

private:
  ID3D11DeviceChild* m_shader;
  D3DBlob* m_bytecode;
};

}  // namespace DX11
