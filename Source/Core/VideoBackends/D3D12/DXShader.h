// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include <memory>
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

  static std::unique_ptr<DXShader> CreateFromBytecode(ShaderStage stage, BinaryData bytecode);
  static std::unique_ptr<DXShader> CreateFromSource(ShaderStage stage, const char* source,
                                                    size_t length);

private:
  DXShader(ShaderStage stage, BinaryData bytecode);

  bool CreateComputePipeline();

  ComPtr<ID3D12PipelineState> m_compute_pipeline;
};

}  // namespace DX12
