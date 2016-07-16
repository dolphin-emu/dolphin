// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "Common/LinearDiskCache.h"

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoCommon.h"

#include "VideoBackends/Vulkan/VulkanImports.h"

namespace Vulkan
{
template <typename Uid>
class ShaderCache
{
public:
  ShaderCache(VkDevice device);
  ~ShaderCache();

  VkShaderModule CompileAndCreateShader(const std::string& shader_source,
                                        bool prepend_header = true);

  VkShaderModule GetShaderForUid(const Uid& uid, DSTALPHA_MODE dstalpha_mode);

private:
  bool CompileShaderToSPV(const std::string& shader_source, std::vector<unsigned int>* spv);
  void LoadShadersFromDisk();

  VkDevice m_device;

  std::map<Uid, VkShaderModule> m_shaders;

  LinearDiskCache<Uid, u32> m_disk_cache;
};

using VertexShaderCache = ShaderCache<VertexShaderUid>;
using GeometryShaderCache = ShaderCache<GeometryShaderUid>;
using PixelShaderCache = ShaderCache<PixelShaderUid>;

std::string GetPipelineDiskCacheFileName();

}  // namespace Vulkan
