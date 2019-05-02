// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/D3D12/DXShader.h"
#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/DXContext.h"

namespace DX12
{
DXShader::DXShader(ShaderStage stage, BinaryData bytecode)
    : D3DCommon::Shader(stage, std::move(bytecode))
{
}

DXShader::~DXShader() = default;

std::unique_ptr<DXShader> DXShader::CreateFromBytecode(ShaderStage stage, BinaryData bytecode)
{
  std::unique_ptr<DXShader> shader(new DXShader(stage, std::move(bytecode)));
  if (stage == ShaderStage::Compute && !shader->CreateComputePipeline())
    return nullptr;

  return shader;
}

std::unique_ptr<DXShader> DXShader::CreateFromSource(ShaderStage stage, const char* source,
                                                     size_t length)
{
  BinaryData bytecode;
  if (!CompileShader(g_dx_context->GetFeatureLevel(), &bytecode, stage, source, length))
    return nullptr;

  return CreateFromBytecode(stage, std::move(bytecode));
}

D3D12_SHADER_BYTECODE DXShader::GetD3DByteCode() const
{
  return D3D12_SHADER_BYTECODE{m_bytecode.data(), m_bytecode.size()};
}

bool DXShader::CreateComputePipeline()
{
  D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
  desc.pRootSignature = g_dx_context->GetComputeRootSignature();
  desc.CS = GetD3DByteCode();
  desc.NodeMask = 1;

  HRESULT hr = g_dx_context->GetDevice()->CreateComputePipelineState(
      &desc, IID_PPV_ARGS(&m_compute_pipeline));
  CHECK(SUCCEEDED(hr), "Creating compute pipeline failed");
  return SUCCEEDED(hr);
}

}  // namespace DX12
