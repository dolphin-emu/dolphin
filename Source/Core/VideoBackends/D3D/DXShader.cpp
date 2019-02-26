// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>

#include "Common/Assert.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/DXShader.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
DXShader::DXShader(ShaderStage stage, BinaryData bytecode, ID3D11DeviceChild* shader)
    : AbstractShader(stage), m_bytecode(bytecode), m_shader(shader)
{
}

DXShader::~DXShader()
{
  m_shader->Release();
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
  return true;
}

AbstractShader::BinaryData DXShader::GetBinary() const
{
  return m_bytecode;
}

std::unique_ptr<DXShader> DXShader::CreateFromBytecode(ShaderStage stage, BinaryData bytecode)
{
  switch (stage)
  {
  case ShaderStage::Vertex:
  {
    ID3D11VertexShader* vs;
    HRESULT hr = D3D::device->CreateVertexShader(bytecode.data(), bytecode.size(), nullptr, &vs);
    CHECK(SUCCEEDED(hr), "Create vertex shader");
    if (FAILED(hr))
      return nullptr;

    return std::make_unique<DXShader>(ShaderStage::Vertex, std::move(bytecode), vs);
  }

  case ShaderStage::Geometry:
  {
    ID3D11GeometryShader* gs;
    HRESULT hr = D3D::device->CreateGeometryShader(bytecode.data(), bytecode.size(), nullptr, &gs);
    CHECK(SUCCEEDED(hr), "Create geometry shader");
    if (FAILED(hr))
      return nullptr;

    return std::make_unique<DXShader>(ShaderStage::Geometry, std::move(bytecode), gs);
  }
  break;

  case ShaderStage::Pixel:
  {
    ID3D11PixelShader* ps;
    HRESULT hr = D3D::device->CreatePixelShader(bytecode.data(), bytecode.size(), nullptr, &ps);
    CHECK(SUCCEEDED(hr), "Create pixel shader");
    if (FAILED(hr))
      return nullptr;

    return std::make_unique<DXShader>(ShaderStage::Pixel, std::move(bytecode), ps);
  }
  break;

  case ShaderStage::Compute:
  {
    ID3D11ComputeShader* cs;
    HRESULT hr = D3D::device->CreateComputeShader(bytecode.data(), bytecode.size(), nullptr, &cs);
    CHECK(SUCCEEDED(hr), "Create compute shader");
    if (FAILED(hr))
      return nullptr;

    return std::make_unique<DXShader>(ShaderStage::Compute, std::move(bytecode), cs);
  }
  break;

  default:
    break;
  }

  return nullptr;
}

static const char* GetCompileTarget(ShaderStage stage)
{
  switch (stage)
  {
  case ShaderStage::Vertex:
    return D3D::VertexShaderVersionString();
  case ShaderStage::Geometry:
    return D3D::GeometryShaderVersionString();
  case ShaderStage::Pixel:
    return D3D::PixelShaderVersionString();
  case ShaderStage::Compute:
    return D3D::ComputeShaderVersionString();
  default:
    return "";
  }
}

bool DXShader::CompileShader(BinaryData* out_bytecode, ShaderStage stage, const char* source,
                             size_t length)
{
  static constexpr D3D_SHADER_MACRO macros[] = {{"API_D3D", "1"}, {nullptr, nullptr}};
  const UINT flags = g_ActiveConfig.bEnableValidationLayer ?
                         (D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION) :
                         (D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION);
  const char* target = GetCompileTarget(stage);

  ID3DBlob* code = nullptr;
  ID3DBlob* errors = nullptr;
  HRESULT hr = PD3DCompile(source, length, nullptr, macros, nullptr, "main", target, flags, 0,
                           &code, &errors);
  if (FAILED(hr))
  {
    static int num_failures = 0;
    std::string filename = StringFromFormat(
        "%sbad_%s_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), target, num_failures++);
    std::ofstream file;
    File::OpenFStream(file, filename, std::ios_base::out);
    file.write(source, length);
    file << "\n";
    file.write(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize());
    file.close();

    PanicAlert("Failed to compile %s:\nDebug info (%s):\n%s", filename.c_str(), target,
               static_cast<const char*>(errors->GetBufferPointer()));
    errors->Release();
    return false;
  }

  if (errors && errors->GetBufferSize() > 0)
  {
    WARN_LOG(VIDEO, "%s compilation succeeded with warnings:\n%s", target,
             static_cast<const char*>(errors->GetBufferPointer()));
  }
  SAFE_RELEASE(errors);

  out_bytecode->resize(code->GetBufferSize());
  std::memcpy(out_bytecode->data(), code->GetBufferPointer(), code->GetBufferSize());
  code->Release();
  return true;
}

std::unique_ptr<DXShader> DXShader::CreateFromSource(ShaderStage stage, const char* source,
                                                     size_t length)
{
  BinaryData bytecode;
  if (!CompileShader(&bytecode, stage, source, length))
    return nullptr;

  return CreateFromBytecode(stage, std::move(bytecode));
}

std::unique_ptr<DXShader> DXShader::CreateFromBinary(ShaderStage stage, const void* data,
                                                     size_t length)
{
  if (length == 0)
    return nullptr;

  BinaryData bytecode(length);
  std::memcpy(bytecode.data(), data, length);
  return CreateFromBytecode(stage, std::move(bytecode));
}
}  // namespace DX11
