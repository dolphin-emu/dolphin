// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/OGL/SamplerCache.h"

#include <memory>

#include "Common/CommonTypes.h"
#include "Common/GL/GLInterfaceBase.h"
#include "VideoCommon/SamplerCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{
std::unique_ptr<SamplerCache> g_sampler_cache;

SamplerCache::SamplerCache() : m_last_max_anisotropy()
{
  glGenSamplers(2, m_sampler_id);
  glSamplerParameteri(m_sampler_id[0], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glSamplerParameteri(m_sampler_id[0], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glSamplerParameteri(m_sampler_id[0], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glSamplerParameteri(m_sampler_id[0], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glSamplerParameteri(m_sampler_id[1], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glSamplerParameteri(m_sampler_id[1], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glSamplerParameteri(m_sampler_id[1], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glSamplerParameteri(m_sampler_id[1], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

SamplerCache::~SamplerCache()
{
  Clear();
  glDeleteSamplers(2, m_sampler_id);
}

void SamplerCache::BindNearestSampler(int stage)
{
  glBindSampler(stage, m_sampler_id[0]);
}

void SamplerCache::BindLinearSampler(int stage)
{
  glBindSampler(stage, m_sampler_id[1]);
}

void SamplerCache::SetSamplerState(int stage, const TexMode0& tm0, const TexMode1& tm1,
                                   bool custom_tex)
{
  // TODO: can this go somewhere else?
  if (m_last_max_anisotropy != g_ActiveConfig.iMaxAnisotropy)
  {
    m_last_max_anisotropy = g_ActiveConfig.iMaxAnisotropy;
    Clear();
  }

  Params params(tm0, tm1);

  // take equivalent forced linear when bForceFiltering
  if (g_ActiveConfig.bForceFiltering)
  {
    params.tm0.min_filter = SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ? 6 : 4;
    params.tm0.mag_filter = 1;
  }

  // custom textures may have higher resolution, so disable the max_lod
  if (custom_tex)
  {
    params.tm1.max_lod = 255;
  }

  // TODO: Should keep a circular buffer for each stage of recently used samplers.

  auto& active_sampler = m_active_samplers[stage];
  if (active_sampler.first != params || !active_sampler.second.sampler_id)
  {
    // Active sampler does not match parameters (or is invalid), bind the proper one.
    active_sampler.first = params;
    active_sampler.second = GetEntry(params);
    glBindSampler(stage, active_sampler.second.sampler_id);
  }
}

SamplerCache::Value& SamplerCache::GetEntry(const Params& params)
{
  auto& val = m_cache[params];
  if (!val.sampler_id)
  {
    // Sampler not found in cache, create it.
    glGenSamplers(1, &val.sampler_id);
    SetParameters(val.sampler_id, params);

    // TODO: Maybe kill old samplers if the cache gets huge. It doesn't seem to get huge though.
    // ERROR_LOG(VIDEO, "Sampler cache size is now %ld.", m_cache.size());
  }

  return val;
}

void SamplerCache::SetParameters(GLuint sampler_id, const Params& params)
{
  static const GLint min_filters[8] = {
      GL_NEAREST, GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST,
      GL_LINEAR,  GL_LINEAR_MIPMAP_NEAREST,  GL_LINEAR_MIPMAP_LINEAR,  GL_LINEAR,
  };

  static const GLint wrap_settings[4] = {
      GL_CLAMP_TO_EDGE, GL_REPEAT, GL_MIRRORED_REPEAT, GL_REPEAT,
  };

  auto& tm0 = params.tm0;
  auto& tm1 = params.tm1;

  glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, wrap_settings[tm0.wrap_s]);
  glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, wrap_settings[tm0.wrap_t]);

  glSamplerParameterf(sampler_id, GL_TEXTURE_MIN_LOD, tm1.min_lod / 16.f);
  glSamplerParameterf(sampler_id, GL_TEXTURE_MAX_LOD, tm1.max_lod / 16.f);

  if (GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGL)
    glSamplerParameterf(sampler_id, GL_TEXTURE_LOD_BIAS, (s32)tm0.lod_bias / 32.f);

  GLint min_filter = min_filters[tm0.min_filter];
  GLint mag_filter = tm0.mag_filter ? GL_LINEAR : GL_NEAREST;

  if (g_ActiveConfig.iMaxAnisotropy > 0 && g_ogl_config.bSupportsAniso &&
      !SamplerCommon::IsBpTexMode0PointFiltering(tm0))
  {
    // https://www.opengl.org/registry/specs/EXT/texture_filter_anisotropic.txt
    // For predictable results on all hardware/drivers, only use one of:
    //	GL_LINEAR + GL_LINEAR (No Mipmaps [Bilinear])
    //	GL_LINEAR + GL_LINEAR_MIPMAP_LINEAR (w/ Mipmaps [Trilinear])
    // Letting the game set other combinations will have varying arbitrary results;
    // possibly being interpreted as equal to bilinear/trilinear, implicitly
    // disabling anisotropy, or changing the anisotropic algorithm employed.
    min_filter =
        SamplerCommon::AreBpTexMode0MipmapsEnabled(tm0) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    mag_filter = GL_LINEAR;
    glSamplerParameterf(sampler_id, GL_TEXTURE_MAX_ANISOTROPY_EXT,
                        (float)(1 << g_ActiveConfig.iMaxAnisotropy));
  }

  glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, min_filter);
  glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, mag_filter);
}

void SamplerCache::Clear()
{
  for (auto& p : m_cache)
  {
    glDeleteSamplers(1, &p.second.sampler_id);
  }
  m_cache.clear();
}
}
