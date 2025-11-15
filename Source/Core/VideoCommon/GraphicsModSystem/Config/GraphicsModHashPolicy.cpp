// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Config/GraphicsModHashPolicy.h"

#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"

namespace GraphicsModSystem::Config
{
namespace
{
bool ContainsAttribute(HashAttributes all_attributes, HashAttributes attribute)
{
  return static_cast<std::underlying_type_t<HashAttributes>>(all_attributes & attribute) != 0;
}

HashAttributes GetDefaultHashAttributes()
{
  return HashAttributes::Blending | HashAttributes::Indices | HashAttributes::VertexLayout;
}
}  // namespace

HashAttributes operator|(HashAttributes lhs, HashAttributes rhs)
{
  return static_cast<HashAttributes>(static_cast<std::underlying_type_t<HashAttributes>>(lhs) |
                                     static_cast<std::underlying_type_t<HashAttributes>>(rhs));
}

HashAttributes operator&(HashAttributes lhs, HashAttributes rhs)
{
  return static_cast<HashAttributes>(static_cast<std::underlying_type_t<HashAttributes>>(lhs) &
                                     static_cast<std::underlying_type_t<HashAttributes>>(rhs));
}

HashPolicy GetDefaultHashPolicy()
{
  HashPolicy policy;
  policy.attributes = GetDefaultHashAttributes();
  policy.first_texture_only = false;
  policy.version = 1;
  return policy;
}

HashAttributes HashAttributesFromString(const std::string& str)
{
  if (str == "")
    return GetDefaultHashAttributes();

  HashAttributes attributes = static_cast<HashAttributes>(NoHashAttributes);
  auto parts = SplitString(str, ',');
  if (parts.empty())
    return GetDefaultHashAttributes();

  for (auto& part : parts)
  {
    Common::ToLower(&part);
    if (part == "blending")
    {
      attributes = attributes | HashAttributes::Blending;
    }
    else if (part == "projection")
    {
      attributes = attributes | HashAttributes::Projection;
    }
    else if (part == "vertex_position")
    {
      attributes = attributes | HashAttributes::VertexPosition;
    }
    else if (part == "vertex_texcoords")
    {
      attributes = attributes | HashAttributes::VertexTexCoords;
    }
    else if (part == "vertex_layout")
    {
      attributes = attributes | HashAttributes::VertexLayout;
    }
    else if (part == "indices")
    {
      attributes = attributes | HashAttributes::Indices;
    }
  }
  if (static_cast<u16>(attributes) == NoHashAttributes)
    return GetDefaultHashAttributes();
  return attributes;
}

std::string HashAttributesToString(HashAttributes attributes)
{
  std::string result;
  if (ContainsAttribute(attributes, HashAttributes::Blending))
  {
    result += "blending,";
  }
  if (ContainsAttribute(attributes, HashAttributes::Projection))
  {
    result += "projection,";
  }
  if (ContainsAttribute(attributes, HashAttributes::VertexPosition))
  {
    result += "vertex_position,";
  }
  if (ContainsAttribute(attributes, HashAttributes::VertexTexCoords))
  {
    result += "vertex_texcoords,";
  }
  if (ContainsAttribute(attributes, HashAttributes::VertexLayout))
  {
    result += "vertex_layout,";
  }
  if (ContainsAttribute(attributes, HashAttributes::Indices))
  {
    result += "indices,";
  }
  return result;
}
}  // namespace GraphicsModSystem::Config
