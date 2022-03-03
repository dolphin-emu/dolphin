// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include "VideoBackends/D3D12/Common.h"
#include "VideoBackends/D3DCommon/Shader.h"

namespace DX12
{
class DXShader final : public D3DCommon::Shader
{
public:
  ~DXShader() override;

  ID3D12PipelineState* GetComputePipeline() const { return m_compute_pipeline.Get(); }
  D3D12_SHADER_BYTECODE GetD3DByteCode() const;

  static std::unique_ptr<DXShader> CreateFromBytecode(ShaderStage stage, BinaryData bytecode,
                                                      std::string_view name);
  static std::unique_ptr<DXShader> CreateFromSource(ShaderStage stage, std::string_view source,
                                                    std::string_view name);

private:
  DXShader(ShaderStage stage, BinaryData bytecode, std::string_view name);

  bool CreateComputePipeline();

  ComPtr<ID3D12PipelineState> m_compute_pipeline;

  std::wstring m_name;
};

}  // namespace DX12
