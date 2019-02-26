// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include <d3d11.h>
#include <memory>

#include "VideoCommon/AbstractShader.h"

namespace DX11
{
class DXShader final : public AbstractShader
{
public:
  DXShader(ShaderStage stage, BinaryData bytecode, ID3D11DeviceChild* shader);
  ~DXShader() override;

  const BinaryData& GetByteCode() const { return m_bytecode; }

  ID3D11VertexShader* GetD3DVertexShader() const;
  ID3D11GeometryShader* GetD3DGeometryShader() const;
  ID3D11PixelShader* GetD3DPixelShader() const;
  ID3D11ComputeShader* GetD3DComputeShader() const;

  bool HasBinary() const override;
  BinaryData GetBinary() const override;

  // Creates a new shader object.
  static std::unique_ptr<DXShader> CreateFromBytecode(ShaderStage stage, BinaryData bytecode);
  static bool CompileShader(BinaryData* out_bytecode, ShaderStage stage, const char* source,
                            size_t length);

  static std::unique_ptr<DXShader> CreateFromBinary(ShaderStage stage, const void* data,
                                                    size_t length);
  static std::unique_ptr<DXShader> CreateFromSource(ShaderStage stage, const char* source,
                                                    size_t length);

private:
  ID3D11DeviceChild* m_shader;
  BinaryData m_bytecode;
};

}  // namespace DX11
