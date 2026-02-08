// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Vulkan/VKVertexFormat.h"

#include "Common/Assert.h"
#include "Common/EnumMap.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"

#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderGen.h"

namespace Vulkan
{
static VkFormat VarToVkFormat(ComponentFormat t, uint32_t components, bool integer)
{
  using ComponentArray = std::array<VkFormat, 4>;
  static constexpr auto f = [](ComponentArray a) { return a; };  // Deduction helper

  static constexpr Common::EnumMap<ComponentArray, ComponentFormat::InvalidFloat7>
      float_type_lookup = {
          f({VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM,
             VK_FORMAT_R8G8B8A8_UNORM}),  // UByte
          f({VK_FORMAT_R8_SNORM, VK_FORMAT_R8G8_SNORM, VK_FORMAT_R8G8B8_SNORM,
             VK_FORMAT_R8G8B8A8_SNORM}),  // Byte
          f({VK_FORMAT_R16_UNORM, VK_FORMAT_R16G16_UNORM, VK_FORMAT_R16G16B16_UNORM,
             VK_FORMAT_R16G16B16A16_UNORM}),  // UShort
          f({VK_FORMAT_R16_SNORM, VK_FORMAT_R16G16_SNORM, VK_FORMAT_R16G16B16_SNORM,
             VK_FORMAT_R16G16B16A16_SNORM}),  // Short
          f({VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
             VK_FORMAT_R32G32B32A32_SFLOAT}),  // Float
          f({VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
             VK_FORMAT_R32G32B32A32_SFLOAT}),  // Invalid
          f({VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
             VK_FORMAT_R32G32B32A32_SFLOAT}),  // Invalid
          f({VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
             VK_FORMAT_R32G32B32A32_SFLOAT}),  // Invalid
      };

  static constexpr Common::EnumMap<ComponentArray, ComponentFormat::InvalidFloat7>
      integer_type_lookup = {
          f({VK_FORMAT_R8_UINT, VK_FORMAT_R8G8_UINT, VK_FORMAT_R8G8B8_UINT,
             VK_FORMAT_R8G8B8A8_UINT}),  // UByte
          f({VK_FORMAT_R8_SINT, VK_FORMAT_R8G8_SINT, VK_FORMAT_R8G8B8_SINT,
             VK_FORMAT_R8G8B8A8_SINT}),  // Byte
          f({VK_FORMAT_R16_UINT, VK_FORMAT_R16G16_UINT, VK_FORMAT_R16G16B16_UINT,
             VK_FORMAT_R16G16B16A16_UINT}),  // UShort
          f({VK_FORMAT_R16_SINT, VK_FORMAT_R16G16_SINT, VK_FORMAT_R16G16B16_SINT,
             VK_FORMAT_R16G16B16A16_SINT}),  // Short
          f({VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
             VK_FORMAT_R32G32B32A32_SFLOAT}),  // Float
          f({VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
             VK_FORMAT_R32G32B32A32_SFLOAT}),  // Invalid
          f({VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
             VK_FORMAT_R32G32B32A32_SFLOAT}),  // Invalid
          f({VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
             VK_FORMAT_R32G32B32A32_SFLOAT}),  // Invalid
      };

  ASSERT(components > 0 && components <= 4);
  return integer ? integer_type_lookup[t][components - 1] : float_type_lookup[t][components - 1];
}

VertexFormat::VertexFormat(const PortableVertexDeclaration& vtx_decl) : NativeVertexFormat(vtx_decl)
{
  MapAttributes();
  SetupInputState();
}

const VkPipelineVertexInputStateCreateInfo& VertexFormat::GetVertexInputStateInfo() const
{
  return m_input_state_info;
}

void VertexFormat::MapAttributes()
{
  m_num_attributes = 0;

  if (m_decl.position.enable)
    AddAttribute(
        ShaderAttrib::Position, 0,
        VarToVkFormat(m_decl.position.type, m_decl.position.components, m_decl.position.integer),
        m_decl.position.offset);

  for (uint32_t i = 0; i < 3; i++)
  {
    if (m_decl.normals[i].enable)
      AddAttribute(ShaderAttrib::Normal + i, 0,
                   VarToVkFormat(m_decl.normals[i].type, m_decl.normals[i].components,
                                 m_decl.normals[i].integer),
                   m_decl.normals[i].offset);
  }

  for (uint32_t i = 0; i < 2; i++)
  {
    if (m_decl.colors[i].enable)
      AddAttribute(ShaderAttrib::Color0 + i, 0,
                   VarToVkFormat(m_decl.colors[i].type, m_decl.colors[i].components,
                                 m_decl.colors[i].integer),
                   m_decl.colors[i].offset);
  }

  for (uint32_t i = 0; i < 8; i++)
  {
    if (m_decl.texcoords[i].enable)
      AddAttribute(ShaderAttrib::TexCoord0 + i, 0,
                   VarToVkFormat(m_decl.texcoords[i].type, m_decl.texcoords[i].components,
                                 m_decl.texcoords[i].integer),
                   m_decl.texcoords[i].offset);
  }

  if (m_decl.posmtx.enable)
    AddAttribute(ShaderAttrib::PositionMatrix, 0,
                 VarToVkFormat(m_decl.posmtx.type, m_decl.posmtx.components, m_decl.posmtx.integer),
                 m_decl.posmtx.offset);
}

void VertexFormat::SetupInputState()
{
  m_binding_description.binding = 0;
  m_binding_description.stride = m_decl.stride;
  m_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  m_input_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  m_input_state_info.pNext = nullptr;
  m_input_state_info.flags = 0;
  m_input_state_info.vertexBindingDescriptionCount = 1;
  m_input_state_info.pVertexBindingDescriptions = &m_binding_description;
  m_input_state_info.vertexAttributeDescriptionCount = m_num_attributes;
  m_input_state_info.pVertexAttributeDescriptions = m_attribute_descriptions.data();
}

void VertexFormat::AddAttribute(ShaderAttrib location, uint32_t binding, VkFormat format,
                                uint32_t offset)
{
  ASSERT(m_num_attributes < MAX_VERTEX_ATTRIBUTES);

  m_attribute_descriptions[m_num_attributes].location = static_cast<uint32_t>(location);
  m_attribute_descriptions[m_num_attributes].binding = binding;
  m_attribute_descriptions[m_num_attributes].format = format;
  m_attribute_descriptions[m_num_attributes].offset = offset;
  m_num_attributes++;
}
}  // namespace Vulkan
