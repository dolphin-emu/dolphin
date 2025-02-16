// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D/DXShader.h"

#include "Common/Assert.h"

#include "VideoBackends/D3D/D3DBase.h"

namespace DX11
{
DXShader::DXShader(ShaderStage stage, BinaryData bytecode, ID3D11DeviceChild* shader,
                   std::string_view name)
    : D3DCommon::Shader(stage, std::move(bytecode)), m_shader(shader), m_name(name)
{
  if (!m_name.empty())
  {
    m_shader->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(m_name.size()),
                             m_name.data());
  }
}

DXShader::~DXShader() = default;

ID3D11VertexShader* DXShader::GetD3DVertexShader() const
{
  DEBUG_ASSERT(m_stage == ShaderStage::Vertex);
  return static_cast<ID3D11VertexShader*>(m_shader.Get());
}

ID3D11GeometryShader* DXShader::GetD3DGeometryShader() const
{
  DEBUG_ASSERT(m_stage == ShaderStage::Geometry);
  return static_cast<ID3D11GeometryShader*>(m_shader.Get());
}

ID3D11PixelShader* DXShader::GetD3DPixelShader() const
{
  DEBUG_ASSERT(m_stage == ShaderStage::Pixel);
  return static_cast<ID3D11PixelShader*>(m_shader.Get());
}

ID3D11ComputeShader* DXShader::GetD3DComputeShader() const
{
  DEBUG_ASSERT(m_stage == ShaderStage::Compute);
  return static_cast<ID3D11ComputeShader*>(m_shader.Get());
}

std::unique_ptr<DXShader> DXShader::CreateFromBytecode(ShaderStage stage, BinaryData bytecode,
                                                       std::string_view name)
{
  switch (stage)
  {
  case ShaderStage::Vertex:
  {
    ComPtr<ID3D11VertexShader> vs;
    HRESULT hr = D3D::device->CreateVertexShader(bytecode.data(), bytecode.size(), nullptr, &vs);
    ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create vertex shader: {}", DX11HRWrap(hr));
    if (FAILED(hr))
      return nullptr;

    return std::make_unique<DXShader>(ShaderStage::Vertex, std::move(bytecode), vs.Get(), name);
  }

  case ShaderStage::Geometry:
  {
    ComPtr<ID3D11GeometryShader> gs;
    HRESULT hr = D3D::device->CreateGeometryShader(bytecode.data(), bytecode.size(), nullptr, &gs);
    ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create geometry shader: {}", DX11HRWrap(hr));
    if (FAILED(hr))
      return nullptr;

    return std::make_unique<DXShader>(ShaderStage::Geometry, std::move(bytecode), gs.Get(), name);
  }

  case ShaderStage::Pixel:
  {
    ComPtr<ID3D11PixelShader> ps;
    HRESULT hr = D3D::device->CreatePixelShader(bytecode.data(), bytecode.size(), nullptr, &ps);
    ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create pixel shader: {}", DX11HRWrap(hr));
    if (FAILED(hr))
      return nullptr;

    return std::make_unique<DXShader>(ShaderStage::Pixel, std::move(bytecode), ps.Get(), name);
  }

  case ShaderStage::Compute:
  {
    ComPtr<ID3D11ComputeShader> cs;
    HRESULT hr = D3D::device->CreateComputeShader(bytecode.data(), bytecode.size(), nullptr, &cs);
    ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Failed to create compute shader: {}", DX11HRWrap(hr));
    if (FAILED(hr))
      return nullptr;

    return std::make_unique<DXShader>(ShaderStage::Compute, std::move(bytecode), cs.Get(), name);
  }

  default:
    break;
  }

  return nullptr;
}
}  // namespace DX11
