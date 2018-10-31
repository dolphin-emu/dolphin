// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoBackends/OGL/Render.h"

#include <memory>

#include "Common/CommonTypes.h"
#include "VideoCommon/SamplerCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
std::unique_ptr<SamplerCache> g_sampler_cache;

SamplerCache::SamplerCache()
{
  glGenSamplers(1, &m_point_sampler);
  glGenSamplers(1, &m_linear_sampler);
  glSamplerParameteri(m_point_sampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glSamplerParameteri(m_point_sampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glSamplerParameteri(m_point_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glSamplerParameteri(m_point_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glSamplerParameteri(m_linear_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glSamplerParameteri(m_linear_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glSamplerParameteri(m_linear_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glSamplerParameteri(m_linear_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

SamplerCache::~SamplerCache()
{
  Clear();
  glDeleteSamplers(1, &m_point_sampler);
  glDeleteSamplers(1, &m_linear_sampler);
}

void SamplerCache::BindNearestSampler(int stage)
{
  glBindSampler(stage, m_point_sampler);
}

void SamplerCache::BindLinearSampler(int stage)
{
  glBindSampler(stage, m_linear_sampler);
}

void SamplerCache::SetSamplerState(u32 stage, const SamplerState& state)
{
  if (m_active_samplers[stage].first == state && m_active_samplers[stage].second != 0)
    return;

  auto it = m_cache.find(state);
  if (it == m_cache.end())
  {
    GLuint sampler;
    glGenSamplers(1, &sampler);
    SetParameters(sampler, state);
    it = m_cache.emplace(state, sampler).first;
  }

  m_active_samplers[stage].first = state;
  m_active_samplers[stage].second = it->second;
  glBindSampler(stage, it->second);
}

void SamplerCache::InvalidateBinding(u32 stage)
{
  m_active_samplers[stage].second = 0;
}

void SamplerCache::SetParameters(GLuint sampler_id, const SamplerState& params)
{
  GLenum min_filter;
  GLenum mag_filter = (params.mag_filter == SamplerState::Filter::Point) ? GL_NEAREST : GL_LINEAR;
  if (params.mipmap_filter == SamplerState::Filter::Linear)
  {
    min_filter = (params.min_filter == SamplerState::Filter::Point) ? GL_NEAREST_MIPMAP_LINEAR :
                                                                      GL_LINEAR_MIPMAP_LINEAR;
  }
  else
  {
    min_filter = (params.min_filter == SamplerState::Filter::Point) ? GL_NEAREST_MIPMAP_NEAREST :
                                                                      GL_LINEAR_MIPMAP_NEAREST;
  }

  glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, min_filter);
  glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, mag_filter);

  static constexpr std::array<GLenum, 3> address_modes = {
      {GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT}};

  glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S,
                      address_modes[static_cast<u32>(params.wrap_u.Value())]);
  glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T,
                      address_modes[static_cast<u32>(params.wrap_v.Value())]);

  glSamplerParameterf(sampler_id, GL_TEXTURE_MIN_LOD, params.min_lod / 16.f);
  glSamplerParameterf(sampler_id, GL_TEXTURE_MAX_LOD, params.max_lod / 16.f);

  if (!static_cast<Renderer*>(g_renderer.get())->IsGLES())
    glSamplerParameterf(sampler_id, GL_TEXTURE_LOD_BIAS, params.lod_bias / 256.f);

  if (params.anisotropic_filtering && g_ogl_config.bSupportsAniso)
  {
    glSamplerParameterf(sampler_id, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        static_cast<float>(1 << g_ActiveConfig.iMaxAnisotropy));
  }
}

void SamplerCache::Clear()
{
  for (auto& p : m_cache)
    glDeleteSamplers(1, &p.second);
  for (auto& p : m_active_samplers)
    p.second = 0;
  m_cache.clear();
}
}  // namespace OGL
