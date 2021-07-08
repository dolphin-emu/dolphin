// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/PEShaderSystem/Config/PEShaderConfigGroup.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEBaseShader.h"
#include "VideoCommon/PEShaderSystem/Runtime/PEShaderApplyOptions.h"
#include "VideoCommon/TextureConfig.h"

namespace VideoCommon::PE
{
class ShaderGroup
{
public:
  static void Initialize();
  static void Shutdown();
  void UpdateConfig(const ShaderConfigGroup& group);
  void Apply(const ShaderApplyOptions& options, ShaderGroup* skip_group = nullptr);

  bool ShouldSkip() const { return m_shaders.empty() || m_skip; }

private:
  bool CreateShaders(const ShaderConfigGroup& group);
  std::optional<u32> m_last_change_count = 0;
  std::vector<std::unique_ptr<BaseShader>> m_shaders;
  bool m_skip = false;

  u32 m_target_width;
  u32 m_target_height;
  u32 m_target_layers;
  AbstractTextureFormat m_target_format;
};
}  // namespace VideoCommon::PE
