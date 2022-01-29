// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3DCommon/Shader.h"

namespace DX11
{
class DXShader final : public D3DCommon::Shader
{
public:
  DXShader(ShaderStage stage, BinaryData bytecode, ID3D11DeviceChild* shader,
           std::string_view name);
  ~DXShader() override;

  ID3D11VertexShader* GetD3DVertexShader() const;
  ID3D11GeometryShader* GetD3DGeometryShader() const;
  ID3D11PixelShader* GetD3DPixelShader() const;
  ID3D11ComputeShader* GetD3DComputeShader() const;

  static std::unique_ptr<DXShader> CreateFromBytecode(ShaderStage stage, BinaryData bytecode,
                                                      std::string_view name);

private:
  ComPtr<ID3D11DeviceChild> m_shader;
  std::string m_name;
};

}  // namespace DX11
