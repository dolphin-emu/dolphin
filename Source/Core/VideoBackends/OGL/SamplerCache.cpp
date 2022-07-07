// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/OGL/SamplerCache.h"

#include <memory>

#include "Common/CommonTypes.h"
#include "VideoBackends/OGL/OGLRender.h"
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
  struct GLMinFilter
  {
    GLenum mip_near, mip_linear;
  };
  // TODO: I think the type of the nested initialization braces could be deduced automatically if
  // EnumMap were an aggregate type.
  static constexpr Common::EnumMap<GLMinFilter, FilterMode::Linear> min_filters = {
      GLMinFilter{GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR},
      GLMinFilter{GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR_MIPMAP_LINEAR},
  };
  static constexpr Common::EnumMap<GLenum, FilterMode::Linear> mag_filters = {
      GL_NEAREST,
      GL_LINEAR,
  };
  static constexpr Common::EnumMap<GLenum, WrapMode::Invalid> address_modes = {
      GL_CLAMP_TO_EDGE,
      GL_REPEAT,
      GL_MIRRORED_REPEAT,
      GL_CLAMP_TO_EDGE,
  };

  glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER,
                      params.tm0.mipmap_filter() == FilterMode::Near ?
                          min_filters[params.tm0.min_filter()].mip_near :
                          min_filters[params.tm0.min_filter()].mip_linear);
  glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, mag_filters[params.tm0.mag_filter()]);

  glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, address_modes[params.tm0.wrap_u()]);
  glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, address_modes[params.tm0.wrap_v()]);

  glSamplerParameterf(sampler_id, GL_TEXTURE_MIN_LOD, params.tm1.min_lod() / 16.f);
  glSamplerParameterf(sampler_id, GL_TEXTURE_MAX_LOD, params.tm1.max_lod() / 16.f);

  if (g_ActiveConfig.backend_info.bSupportsLodBiasInSampler)
  {
    glSamplerParameterf(sampler_id, GL_TEXTURE_LOD_BIAS, params.tm0.lod_bias() / 256.f);
  }

  if (params.tm0.anisotropic_filtering() && g_ogl_config.bSupportsAniso)
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
