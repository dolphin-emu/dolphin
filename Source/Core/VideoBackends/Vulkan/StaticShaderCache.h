// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

#include "VideoBackends/Vulkan/Globals.h"
#include "VideoBackends/Vulkan/ShaderCache.h"
#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan {

class StaticShaderCache
{
public:
	StaticShaderCache(VkDevice device, VertexShaderCache* vs_cache, GeometryShaderCache* gs_cache, PixelShaderCache* ps_cache);
	~StaticShaderCache();

	// Recompile static shaders, call when MSAA mode changes, etc.
	bool CompileShaders();
	void DestroyShaders();

	// Vertex Shaders
	VkShaderModule GetScreenQuadVertexShader() const { return m_vertex_shaders.screen_quad; }

	// Fragment Shaders
	VkShaderModule GetBlitFragmentShader() const { return m_fragment_shaders.blit; }

private:
	VkDevice m_device;
	VertexShaderCache* m_vs_cache;
	GeometryShaderCache* m_gs_cache;
	PixelShaderCache* m_ps_cache;

	struct
	{
		VkShaderModule screen_quad;
	} m_vertex_shaders;

	struct
	{
		VkShaderModule blit;
	} m_fragment_shaders;
};

}  // namespace Vulkan
