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
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderApplyOptions.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderPass.h"

class AbstractTexture;
class ShaderCode;

namespace VideoCommon::PE
{
class ShaderOption;
class VertexPixelShader final : public BaseShader
{
public:
  bool CreatePasses(const ShaderConfig& config) override;
  bool CreateAllPassOutputTextures(u32 width, u32 height, u32 layers,
                                   AbstractTextureFormat format) override;
  bool RebuildPipeline(AbstractTextureFormat format, u32 layers) override;
  void Apply(bool skip_final_copy, const ShaderApplyOptions& options,
             const AbstractTexture* prev_pass_texture, float prev_pass_output_scale) override;

  bool SupportsDirectWrite() const override { return true; }

private:
  bool RecompilePasses(const ShaderConfig& config) override;
  std::unique_ptr<AbstractShader> CompileGeometryShader(const ShaderConfig& config) const;
  std::shared_ptr<AbstractShader> CompileVertexShader(const ShaderConfig& config) const;
  std::unique_ptr<AbstractShader> CompilePixelShader(const ShaderConfig& config,
                                                     const ShaderPass& pass,
                                                     const ShaderConfigPass& config_pass) const;
  void PixelShaderFooter(ShaderCode& shader_source, const ShaderConfigPass& config_pass) const;
  void PixelShaderHeader(ShaderCode& shader_source, const ShaderPass& pass) const;
  void VertexShaderMain(ShaderCode& shader_source) const;
  AbstractTextureFormat m_last_pipeline_format;
  u32 m_last_pipeline_layers;
  std::unique_ptr<AbstractShader> m_geometry_shader;
};
}  // namespace VideoCommon::PE
