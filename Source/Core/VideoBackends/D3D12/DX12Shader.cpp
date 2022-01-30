// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/D3D12/DX12Shader.h"

#include "Common/Assert.h"
#include "Common/StringUtil.h"

#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3D12/DX12Context.h"

namespace DX12
{
DXShader::DXShader(ShaderStage stage, BinaryData bytecode, std::string_view name)
    : D3DCommon::Shader(stage, std::move(bytecode)), m_name(UTF8ToWString(name))
{
}

DXShader::~DXShader() = default;

std::unique_ptr<DXShader> DXShader::CreateFromBytecode(ShaderStage stage, BinaryData bytecode,
                                                       std::string_view name)
{
  std::unique_ptr<DXShader> shader(new DXShader(stage, std::move(bytecode), name));
  if (stage == ShaderStage::Compute && !shader->CreateComputePipeline())
    return nullptr;

  return shader;
}

std::unique_ptr<DXShader> DXShader::CreateFromSource(ShaderStage stage, std::string_view source,
                                                     std::string_view name)
{
  auto bytecode = CompileShader(g_dx_context->GetFeatureLevel(), stage, source);
  if (!bytecode)
    return nullptr;

  return CreateFromBytecode(stage, std::move(*bytecode), name);
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
  ASSERT_MSG(VIDEO, SUCCEEDED(hr), "Creating compute pipeline failed: {}", DX12HRWrap(hr));

  if (m_compute_pipeline && !m_name.empty())
    m_compute_pipeline->SetName(m_name.c_str());

  return SUCCEEDED(hr);
}

}  // namespace DX12
