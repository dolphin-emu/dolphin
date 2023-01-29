// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <memory>
#include <string>

#include "Common/CommonTypes.h"
#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigInput.h"
#include "VideoCommon/RenderState.h"

class AbstractTexture;

namespace VideoCommon::PE
{
struct ShaderOutput;

enum class InputType : u32
{
  ExternalImage,         // external image loaded from file
  ColorBuffer,           // colorbuffer at internal resolution
  DepthBuffer,           // depthbuffer at internal resolution
  PreviousPassOutput,    // output of the previous pass
  PreviousShaderOutput,  // output of the previous shader or the color buffer
  NamedOutput            // output of a named pass
};

struct BaseShaderPass;

class ShaderInput
{
public:
  static std::unique_ptr<ShaderInput> Create(const ShaderConfigInput& input_config,
                                             const std::map<std::string, ShaderOutput>& outputs);
  virtual ~ShaderInput() = default;
  ShaderInput(const ShaderInput&) = default;
  ShaderInput(ShaderInput&&) = default;
  ShaderInput& operator=(const ShaderInput&) = default;
  ShaderInput& operator=(ShaderInput&&) = default;

  InputType GetType() const;
  u32 GetTextureUnit() const;
  SamplerState GetSamplerState() const;
  virtual const AbstractTexture* GetTexture() const = 0;

protected:
  ShaderInput(InputType type, u32 texture_unit, SamplerState state);

private:
  InputType m_type;
  u32 m_texture_unit;
  SamplerState m_sampler_state;
};

class PreviousNamedOutput final : public ShaderInput
{
public:
  PreviousNamedOutput(std::string name, const ShaderOutput* output, u32 texture_unit,
                      SamplerState state);
  const std::string& GetName() const;
  const AbstractTexture* GetTexture() const override;

private:
  std::string m_name;
  const ShaderOutput* m_output;
};
}  // namespace VideoCommon::PE
