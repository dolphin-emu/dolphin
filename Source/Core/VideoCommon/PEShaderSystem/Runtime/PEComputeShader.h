// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/PEShaderSystem/Runtime/PEBaseShader.h"

#include <memory>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/PEShaderSystem/Config/PEShaderConfig.h"
#include "VideoCommon/PEShaderSystem/Runtime/BuiltinUniforms.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEComputeShaderPass.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderApplyOptions.h"

class AbstractTexture;
class ShaderCode;

namespace VideoCommon::PE
{
class ShaderOption;

class ComputeShader final : public BaseShader
{
public:
  bool CreatePasses(const ShaderConfig& config) override;
  bool CreateAllPassOutputTextures(u32 width, u32 height, u32 layers,
                                   AbstractTextureFormat format) override;
  bool RebuildPipeline(AbstractTextureFormat format, u32 layers) override;
  void Apply(bool skip_final_copy, const ShaderApplyOptions& options,
             const AbstractTexture* prev_pass_texture, float prev_pass_output_scale) override;

  bool SupportsDirectWrite() const override { return false; }

private:
  bool RecompilePasses(const ShaderConfig& config) override;
  std::unique_ptr<AbstractShader> CompileShader(const ShaderConfig& config,
                                                const ComputeShaderPass& pass,
                                                const ShaderConfigPass& config_pass) const;
  void ShaderFooter(ShaderCode& shader_source, const ShaderConfigPass& config_pass) const;
  void ShaderHeader(ShaderCode& shader_source, const ComputeShaderPass& pass) const;
};
}  // namespace VideoCommon::PE
