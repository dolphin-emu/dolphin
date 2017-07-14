// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/AbstractPixelShader.h"

namespace Vulkan
{
class Texture2D;
class VKPixelShader final : public AbstractPixelShader
{
public:
  explicit VKPixelShader(const std::string& shader_source);
  virtual ~VKPixelShader();
  void ApplyTo(AbstractTexture* texture) override;
private:
  bool CreateRenderPass();
  void DrawTexture(Texture2D* vulkan_texture, const VkFramebuffer& frame_buffer, int width, int height);

  static const VkFormat TEXTURE_FORMAT = VK_FORMAT_B8G8R8A8_UNORM;

  VkRenderPass m_render_pass = VK_NULL_HANDLE;
  VkShaderModule m_shader = VK_NULL_HANDLE;
};
} // namespace Vulkan
