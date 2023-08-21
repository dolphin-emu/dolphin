// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <map>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/GL/GLUtil.h"
#include "VideoCommon/Constants.h"
#include "VideoCommon/RenderState.h"

namespace OGL
{
class SamplerCache
{
public:
  SamplerCache();
  ~SamplerCache();

  SamplerCache(const SamplerCache&) = delete;
  SamplerCache& operator=(const SamplerCache&) = delete;
  SamplerCache(SamplerCache&&) = delete;
  SamplerCache& operator=(SamplerCache&&) = delete;

  void SetSamplerState(u32 stage, const SamplerState& state);
  void InvalidateBinding(u32 stage);

  void Clear();
  void BindNearestSampler(int stage);
  void BindLinearSampler(int stage);

private:
  static void SetParameters(GLuint sampler_id, const SamplerState& params);

  std::map<SamplerState, GLuint> m_cache;
  std::array<std::pair<SamplerState, GLuint>, VideoCommon::MAX_PIXEL_SHADER_SAMPLERS>
      m_active_samplers{};

  GLuint m_point_sampler;
  GLuint m_linear_sampler;
};

extern std::unique_ptr<SamplerCache> g_sampler_cache;
}  // namespace OGL
