// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "VideoBackends/OGL/GLInterfaceBase.h"
#include "VideoBackends/OGL/SamplerCache.h"
#include "VideoCommon/DriverDetails.h"

namespace OGL
{

SamplerCache *g_sampler_cache;

SamplerCache::SamplerCache()
	: m_last_max_anisotropy()
{}

SamplerCache::~SamplerCache()
{
	Clear();
}

void SamplerCache::SetSamplerState(int stage, const TexMode0& tm0, const TexMode1& tm1)
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
		params.tm0.min_filter |= 0x4;
		params.tm0.mag_filter |= 0x1;
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
		//ERROR_LOG(VIDEO, "Sampler cache size is now %ld.", m_cache.size());
	}

	return val;
}

void SamplerCache::SetParameters(GLuint sampler_id, const Params& params)
{
	static const GLint min_filters[8] =
	{
		GL_NEAREST,
		GL_NEAREST_MIPMAP_NEAREST,
		GL_NEAREST_MIPMAP_LINEAR,
		GL_NEAREST,
		GL_LINEAR,
		GL_LINEAR_MIPMAP_NEAREST,
		GL_LINEAR_MIPMAP_LINEAR,
		GL_LINEAR,
	};

	static const GLint wrap_settings[4] =
	{
		GL_CLAMP_TO_EDGE,
		GL_REPEAT,
		GL_MIRRORED_REPEAT,
		GL_REPEAT,
	};

	auto& tm0 = params.tm0;
	auto& tm1 = params.tm1;

	glSamplerParameteri(sampler_id, GL_TEXTURE_MIN_FILTER, min_filters[tm0.min_filter % ArraySize(min_filters)]);
	glSamplerParameteri(sampler_id, GL_TEXTURE_MAG_FILTER, tm0.mag_filter ? GL_LINEAR : GL_NEAREST);

	glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_S, wrap_settings[tm0.wrap_s]);
	glSamplerParameteri(sampler_id, GL_TEXTURE_WRAP_T, wrap_settings[tm0.wrap_t]);

	glSamplerParameterf(sampler_id, GL_TEXTURE_MIN_LOD, tm1.min_lod / 16.f);
	glSamplerParameterf(sampler_id, GL_TEXTURE_MAX_LOD, tm1.max_lod / 16.f);

	if (GLInterface->GetMode() == GLInterfaceMode::MODE_OPENGL)
	{
		glSamplerParameterf(sampler_id, GL_TEXTURE_LOD_BIAS, (s32)tm0.lod_bias / 32.f);

		if (g_ActiveConfig.iMaxAnisotropy > 0)
			glSamplerParameterf(sampler_id, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)(1 << g_ActiveConfig.iMaxAnisotropy));
	}
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
