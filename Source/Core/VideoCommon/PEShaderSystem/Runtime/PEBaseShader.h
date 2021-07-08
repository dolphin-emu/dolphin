// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/AbstractShader.h"
#include "VideoCommon/PEShaderSystem/Config/PEShaderConfig.h"
#include "VideoCommon/PEShaderSystem/Runtime/BuiltinUniforms.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEBaseShaderPass.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderApplyOptions.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderOutput.h"

class AbstractTexture;
class ShaderCode;

namespace VideoCommon::PE
{
class ShaderOption;

class BaseShader
{
public:
  BaseShader() = default;
  virtual ~BaseShader() = default;
  BaseShader(const BaseShader&) = delete;
  BaseShader(BaseShader&&) = default;
  BaseShader& operator=(const BaseShader&) = delete;
  BaseShader& operator=(BaseShader&&) = default;

  bool UpdateConfig(const ShaderConfig& config);
  void CreateOptions(const ShaderConfig& config);
  virtual bool CreatePasses(const ShaderConfig& config) = 0;
  virtual bool CreateAllPassOutputTextures(u32 width, u32 height, u32 layers,
                                           AbstractTextureFormat format) = 0;
  virtual bool RebuildPipeline(AbstractTextureFormat format, u32 layers) = 0;
  virtual void Apply(bool skip_final_copy, const ShaderApplyOptions& options,
                     const AbstractTexture* prev_pass_texture, float prev_pass_output_scale) = 0;

  std::vector<BaseShaderPass*> GetPasses() const;
  virtual bool SupportsDirectWrite() const = 0;

protected:
  virtual bool RecompilePasses(const ShaderConfig& config) = 0;
  void UploadUniformBuffer();
  void PrepareUniformHeader(ShaderCode& shader_source) const;
  std::vector<u8> m_uniform_staging_buffer;
  std::vector<std::unique_ptr<BaseShaderPass>> m_passes;
  std::vector<std::unique_ptr<ShaderOption>> m_options;
  BuiltinUniforms m_builtin_uniforms;
  u32 m_last_static_option_change_count = 0;
  u32 m_last_compile_option_change_count = 0;
  std::map<std::string, ShaderOutput> m_outputs;
};
}  // namespace VideoCommon::PE
