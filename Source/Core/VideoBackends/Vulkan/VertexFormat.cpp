// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Vulkan/VertexFormat.h"

#include "Common/Assert.h"

#include "VideoBackends/Vulkan/CommandBufferManager.h"
#include "VideoBackends/Vulkan/ObjectCache.h"

#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderGen.h"

namespace Vulkan
{
static VkFormat VarToVkFormat(VarType t, uint32_t components, bool integer)
{
  static const VkFormat float_type_lookup[][4] = {
      {VK_FORMAT_R8_UNORM, VK_FORMAT_R8G8_UNORM, VK_FORMAT_R8G8B8_UNORM,
       VK_FORMAT_R8G8B8A8_UNORM},  // VAR_UNSIGNED_BYTE
      {VK_FORMAT_R8_SNORM, VK_FORMAT_R8G8_SNORM, VK_FORMAT_R8G8B8_SNORM,
       VK_FORMAT_R8G8B8A8_SNORM},  // VAR_BYTE
      {VK_FORMAT_R16_UNORM, VK_FORMAT_R16G16_UNORM, VK_FORMAT_R16G16B16_UNORM,
       VK_FORMAT_R16G16B16A16_UNORM},  // VAR_UNSIGNED_SHORT
      {VK_FORMAT_R16_SNORM, VK_FORMAT_R16G16_SNORM, VK_FORMAT_R16G16B16_SNORM,
       VK_FORMAT_R16G16B16A16_SNORM},  // VAR_SHORT
      {VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
       VK_FORMAT_R32G32B32A32_SFLOAT}  // VAR_FLOAT
  };

  static const VkFormat integer_type_lookup[][4] = {
      {VK_FORMAT_R8_UINT, VK_FORMAT_R8G8_UINT, VK_FORMAT_R8G8B8_UINT,
       VK_FORMAT_R8G8B8A8_UINT},  // VAR_UNSIGNED_BYTE
      {VK_FORMAT_R8_SINT, VK_FORMAT_R8G8_SINT, VK_FORMAT_R8G8B8_SINT,
       VK_FORMAT_R8G8B8A8_SINT},  // VAR_BYTE
      {VK_FORMAT_R16_UINT, VK_FORMAT_R16G16_UINT, VK_FORMAT_R16G16B16_UINT,
       VK_FORMAT_R16G16B16A16_UINT},  // VAR_UNSIGNED_SHORT
      {VK_FORMAT_R16_SINT, VK_FORMAT_R16G16_SINT, VK_FORMAT_R16G16B16_SINT,
       VK_FORMAT_R16G16B16A16_SINT},  // VAR_SHORT
      {VK_FORMAT_R32_SFLOAT, VK_FORMAT_R32G32_SFLOAT, VK_FORMAT_R32G32B32_SFLOAT,
       VK_FORMAT_R32G32B32A32_SFLOAT}  // VAR_FLOAT
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
        SHADER_POSITION_ATTRIB, 0,
        VarToVkFormat(m_decl.position.type, m_decl.position.components, m_decl.position.integer),
        m_decl.position.offset);

  for (uint32_t i = 0; i < 3; i++)
  {
    if (m_decl.normals[i].enable)
      AddAttribute(SHADER_NORM0_ATTRIB + i, 0,
                   VarToVkFormat(m_decl.normals[i].type, m_decl.normals[i].components,
                                 m_decl.normals[i].integer),
                   m_decl.normals[i].offset);
  }

  for (uint32_t i = 0; i < 2; i++)
  {
    if (m_decl.colors[i].enable)
      AddAttribute(SHADER_COLOR0_ATTRIB + i, 0,
                   VarToVkFormat(m_decl.colors[i].type, m_decl.colors[i].components,
                                 m_decl.colors[i].integer),
                   m_decl.colors[i].offset);
  }

  for (uint32_t i = 0; i < 8; i++)
  {
    if (m_decl.texcoords[i].enable)
      AddAttribute(SHADER_TEXTURE0_ATTRIB + i, 0,
                   VarToVkFormat(m_decl.texcoords[i].type, m_decl.texcoords[i].components,
                                 m_decl.texcoords[i].integer),
                   m_decl.texcoords[i].offset);
  }

  if (m_decl.posmtx.enable)
    AddAttribute(SHADER_POSMTX_ATTRIB, 0,
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

void VertexFormat::AddAttribute(uint32_t location, uint32_t binding, VkFormat format,
                                uint32_t offset)
{
  ASSERT(m_num_attributes < MAX_VERTEX_ATTRIBUTES);

  m_attribute_descriptions[m_num_attributes].location = location;
  m_attribute_descriptions[m_num_attributes].binding = binding;
  m_attribute_descriptions[m_num_attributes].format = format;
  m_attribute_descriptions[m_num_attributes].offset = offset;
  m_num_attributes++;
}
}  // namespace Vulkan
