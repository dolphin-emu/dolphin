// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Assert.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/DXShader.h"

namespace DX11
{
DXShader::DXShader(D3DBlob* bytecode, ID3D11VertexShader* vs)
    : AbstractShader(ShaderStage::Vertex), m_bytecode(bytecode), m_shader(vs)
{
}

DXShader::DXShader(D3DBlob* bytecode, ID3D11GeometryShader* gs)
    : AbstractShader(ShaderStage::Geometry), m_bytecode(bytecode), m_shader(gs)
{
}

DXShader::DXShader(D3DBlob* bytecode, ID3D11PixelShader* ps)
    : AbstractShader(ShaderStage::Pixel), m_bytecode(bytecode), m_shader(ps)
{
}

DXShader::DXShader(D3DBlob* bytecode, ID3D11ComputeShader* cs)
    : AbstractShader(ShaderStage::Compute), m_bytecode(bytecode), m_shader(cs)
{
}

DXShader::~DXShader()
{
  m_shader->Release();
  m_bytecode->Release();
}

D3DBlob* DXShader::GetByteCode() const
{
  return m_bytecode;
}

ID3D11VertexShader* DXShader::GetD3DVertexShader() const
{
  DEBUG_ASSERT(m_stage == ShaderStage::Vertex);
  return static_cast<ID3D11VertexShader*>(m_shader);
}

ID3D11GeometryShader* DXShader::GetD3DGeometryShader() const
{
  DEBUG_ASSERT(m_stage == ShaderStage::Geometry);
  return static_cast<ID3D11GeometryShader*>(m_shader);
}

ID3D11PixelShader* DXShader::GetD3DPixelShader() const
{
  DEBUG_ASSERT(m_stage == ShaderStage::Pixel);
  return static_cast<ID3D11PixelShader*>(m_shader);
}

ID3D11ComputeShader* DXShader::GetD3DComputeShader() const
{
  DEBUG_ASSERT(m_stage == ShaderStage::Compute);
  return static_cast<ID3D11ComputeShader*>(m_shader);
}

bool DXShader::HasBinary() const
{
  ASSERT(m_bytecode);
  return true;
}

AbstractShader::BinaryData DXShader::GetBinary() const
{
  return BinaryData(m_bytecode->Data(), m_bytecode->Data() + m_bytecode->Size());
}

std::unique_ptr<DXShader> DXShader::CreateFromBlob(ShaderStage stage, D3DBlob* bytecode)
{
  switch (stage)
  {
  case ShaderStage::Vertex:
  {
    ID3D11VertexShader* vs = D3D::CreateVertexShaderFromByteCode(bytecode);
    if (vs)
      return std::make_unique<DXShader>(bytecode, vs);
  }
  break;

  case ShaderStage::Geometry:
  {
    ID3D11GeometryShader* gs = D3D::CreateGeometryShaderFromByteCode(bytecode);
    if (gs)
      return std::make_unique<DXShader>(bytecode, gs);
  }
  break;

  case ShaderStage::Pixel:
  {
    ID3D11PixelShader* ps = D3D::CreatePixelShaderFromByteCode(bytecode);
    if (ps)
      return std::make_unique<DXShader>(bytecode, ps);
  }
  break;

  case ShaderStage::Compute:
  {
    ID3D11ComputeShader* cs = D3D::CreateComputeShaderFromByteCode(bytecode);
    if (cs)
      return std::make_unique<DXShader>(bytecode, cs);
  }
  break;

  default:
    break;
  }

  return nullptr;
}

std::unique_ptr<DXShader> DXShader::CreateFromSource(ShaderStage stage, const char* source,
                                                     size_t length)
{
  D3DBlob* bytecode;
  switch (stage)
  {
  case ShaderStage::Vertex:
  {
    if (!D3D::CompileVertexShader(std::string(source, length), &bytecode))
      return nullptr;
  }
  break;

  case ShaderStage::Geometry:
  {
    if (!D3D::CompileGeometryShader(std::string(source, length), &bytecode))
      return nullptr;
  }
  break;

  case ShaderStage::Pixel:
  {
    if (!D3D::CompilePixelShader(std::string(source, length), &bytecode))
      return nullptr;
  }
  break;

  case ShaderStage::Compute:
  {
    if (!D3D::CompileComputeShader(std::string(source, length), &bytecode))
      return nullptr;
  }

  default:
    return nullptr;
  }

  std::unique_ptr<DXShader> shader = CreateFromBlob(stage, bytecode);
  if (!shader)
  {
    bytecode->Release();
    return nullptr;
  }

  return shader;
}

std::unique_ptr<DXShader> DXShader::CreateFromBinary(ShaderStage stage, const void* data,
                                                     size_t length)
{
  D3DBlob* bytecode = new D3DBlob(static_cast<unsigned int>(length), static_cast<const u8*>(data));
  std::unique_ptr<DXShader> shader = CreateFromBlob(stage, bytecode);
  if (!shader)
  {
    bytecode->Release();
    return nullptr;
  }

  return shader;
}

}  // namespace DX11
