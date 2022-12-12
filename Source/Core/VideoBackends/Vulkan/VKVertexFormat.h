// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "VideoBackends/Vulkan/Constants.h"
#include "VideoCommon/NativeVertexFormat.h"

enum class ShaderAttrib : u32;

namespace Vulkan
{
class VertexFormat : public ::NativeVertexFormat
{
public:
  VertexFormat(const PortableVertexDeclaration& vtx_decl);

  // Passed to pipeline state creation
  const VkPipelineVertexInputStateCreateInfo& GetVertexInputStateInfo() const;

  // Converting PortableVertexDeclaration -> Vulkan types
  void MapAttributes();
  void SetupInputState();

private:
  void AddAttribute(ShaderAttrib location, uint32_t binding, VkFormat format, uint32_t offset);

  VkVertexInputBindingDescription m_binding_description = {};

  std::array<VkVertexInputAttributeDescription, MAX_VERTEX_ATTRIBUTES> m_attribute_descriptions =
      {};

  VkPipelineVertexInputStateCreateInfo m_input_state_info = {};

  uint32_t m_num_attributes = 0;
};
}  // namespace Vulkan
