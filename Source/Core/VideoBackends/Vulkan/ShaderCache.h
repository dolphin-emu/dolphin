// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <vector>

#include <shaderc/shaderc.hpp>

#include "Common/LinearDiskCache.h"

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoCommon.h"

#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan {

template <typename Uid>
class ShaderCache
{
public:
	ShaderCache(VkDevice device);
	~ShaderCache();

	VkShaderModule GetShaderForUid(const Uid& uid, u32 primitive_type, DSTALPHA_MODE dstalpha_mode);

private:
	void LoadShadersFromDisk();

	VkDevice m_device;

	std::map<Uid, VkShaderModule> m_shaders;

	LinearDiskCache<Uid, u32> m_disk_cache;
};

using VertexShaderCache = ShaderCache<VertexShaderUid>;
using GeometryShaderCache = ShaderCache<GeometryShaderUid>;
using FragmentShaderCache = ShaderCache<PixelShaderUid>;

}  // namespace Vulkan
