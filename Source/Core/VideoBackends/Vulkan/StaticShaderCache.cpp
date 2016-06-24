// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/StaticShaderCache.h"
#include "VideoBackends/Vulkan/ShaderCache.h"

namespace Vulkan {

StaticShaderCache::StaticShaderCache(VkDevice device, VertexShaderCache* vs_cache, GeometryShaderCache* gs_cache, PixelShaderCache* ps_cache)
	: m_device(device)
	, m_vs_cache(vs_cache)
	, m_gs_cache(gs_cache)
	, m_ps_cache(ps_cache)
{

}

StaticShaderCache::~StaticShaderCache()
{
	DestroyShaders();
}

#include "VideoBackends/Vulkan/StaticShaderSource.inl"

bool StaticShaderCache::CompileShaders()
{
	u32 efb_layers = (g_ActiveConfig.iStereoMode != STEREO_OFF) ? 2 : 1;

	// Header - will include information about MSAA modes, etc.
	std::string header = "";
	if (g_ActiveConfig.iMultisamples > 1)
		header += StringFromFormat("#define MSAA_ENABLED 1\n#define MSAA_SAMPLES %d\n", g_ActiveConfig.iMultisamples);
	header += StringFromFormat("#define EFB_LAYERS %u\n", efb_layers);

	// Vertex Shaders
	if ((m_vertex_shaders.screen_quad = m_vs_cache->CompileAndCreateShader(header + SCREEN_QUAD_VERTEX_SHADER_SOURCE)) == nullptr)
	{
		return false;
	}

	// Fragment Shaders
	if ((m_fragment_shaders.blit = m_ps_cache->CompileAndCreateShader(header + BLIT_FRAGMENT_SHADER_SOURCE)) == nullptr)
	{
		return false;
	}

	return true;
}

void StaticShaderCache::DestroyShaders()
{
	auto DestroyShader = [this](VkShaderModule& shader) { if (shader != VK_NULL_HANDLE) { vkDestroyShaderModule(m_device, shader, nullptr); shader = VK_NULL_HANDLE; }};

	// Vertex Shaders
	DestroyShader(m_vertex_shaders.screen_quad);

	// Fragment Shaders
	DestroyShader(m_fragment_shaders.blit);
}

}
