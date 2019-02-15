// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "VideoBackends/Vulkan/VulkanContext.h"

#include "VideoCommon/PostProcessing.h"
#include "VideoCommon/VideoCommon.h"

namespace Vulkan
{
class Texture2D;

class VulkanPostProcessing : public PostProcessingShaderImplementation
{
public:
  VulkanPostProcessing() = default;
  ~VulkanPostProcessing();

  bool Initialize(const Texture2D* font_texture);

  void BlitFromTexture(const TargetRectangle& dst, const TargetRectangle& src,
                       const Texture2D* src_tex, int src_layer, VkRenderPass render_pass);

  void UpdateConfig();

private:
  size_t CalculateUniformsSize() const;
  void FillUniformBuffer(u8* buf, const TargetRectangle& src, const Texture2D* src_tex,
                         int src_layer);

  bool CompileDefaultShader();
  bool RecompileShader();
  std::string GetGLSLUniformBlock() const;

  const Texture2D* m_font_texture = nullptr;
  VkShaderModule m_fragment_shader = VK_NULL_HANDLE;
  VkShaderModule m_default_fragment_shader = VK_NULL_HANDLE;
};

}  // namespace
