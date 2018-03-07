// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoBackends/Metal/MetalVertexFormat.h"

#include "Common/Assert.h"

#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexShaderGen.h"

namespace Metal
{
static mtlpp::VertexFormat VarToMetalFormat(VarType t, uint32_t components, bool integer)
{
  static const mtlpp::VertexFormat float_type_lookup[][4] = {
      {mtlpp::VertexFormat::UCharNormalized, mtlpp::VertexFormat::UChar2Normalized,
       mtlpp::VertexFormat::UChar3Normalized,
       mtlpp::VertexFormat::UChar4Normalized},  // VAR_UNSIGNED_BYTE
      {mtlpp::VertexFormat::CharNormalized, mtlpp::VertexFormat::Char2Normalized,
       mtlpp::VertexFormat::Char3Normalized, mtlpp::VertexFormat::Char4Normalized},  // VAR_BYTE
      {mtlpp::VertexFormat::UShortNormalized, mtlpp::VertexFormat::UShort2Normalized,
       mtlpp::VertexFormat::UShort3Normalized,
       mtlpp::VertexFormat::UShort4Normalized},  // VAR_UNSIGNED_SHORT
      {mtlpp::VertexFormat::ShortNormalized, mtlpp::VertexFormat::Short2Normalized,
       mtlpp::VertexFormat::Short3Normalized, mtlpp::VertexFormat::Short4Normalized},  // VAR_SHORT
      {mtlpp::VertexFormat::Float, mtlpp::VertexFormat::Float2, mtlpp::VertexFormat::Float3,
       mtlpp::VertexFormat::Float4}  // VAR_FLOAT
  };

  static const mtlpp::VertexFormat integer_type_lookup[][4] = {
      {mtlpp::VertexFormat::UChar, mtlpp::VertexFormat::UChar2, mtlpp::VertexFormat::UChar3,
       mtlpp::VertexFormat::UChar4},  // VAR_UNSIGNED_BYTE
      {mtlpp::VertexFormat::Char, mtlpp::VertexFormat::Char2, mtlpp::VertexFormat::Char3,
       mtlpp::VertexFormat::Char4},  // VAR_BYTE
      {mtlpp::VertexFormat::UShort, mtlpp::VertexFormat::UShort2, mtlpp::VertexFormat::UShort3,
       mtlpp::VertexFormat::UShort4},  // VAR_UNSIGNED_SHORT
      {mtlpp::VertexFormat::Short, mtlpp::VertexFormat::Short2, mtlpp::VertexFormat::Short3,
       mtlpp::VertexFormat::Short4},  // VAR_SHORT
      {mtlpp::VertexFormat::Float, mtlpp::VertexFormat::Float2, mtlpp::VertexFormat::Float3,
       mtlpp::VertexFormat::Float4}  // VAR_FLOAT
  };

  _dbg_assert_(VIDEO, components > 0 && components <= 4);
  return integer ? integer_type_lookup[t][components - 1] : float_type_lookup[t][components - 1];
}

MetalVertexFormat::MetalVertexFormat(const PortableVertexDeclaration& in_vtx_decl)
{
  vtx_decl = in_vtx_decl;
  InitDescriptor();
}

void MetalVertexFormat::InitDescriptor()
{
  // Set layout (buffer info).
  auto layout = m_desc.GetLayouts()[0];
  layout.SetStepFunction(mtlpp::VertexStepFunction::PerVertex);
  layout.SetStepRate(1);
  layout.SetStride(vtx_decl.stride);

  // Set attributes.
  auto attrs = m_desc.GetAttributes();
  auto SetAttribute = [&](u32 index, const AttributeFormat& af) {
    auto attr = attrs[index];
    attr.SetBufferIndex(0);
    attr.SetOffset(af.offset);
    attr.SetFormat(VarToMetalFormat(af.type, af.components, af.integer));
  };

  if (vtx_decl.position.enable)
    SetAttribute(SHADER_POSITION_ATTRIB, vtx_decl.position);

  for (size_t i = 0; i < ArraySize(vtx_decl.normals); i++)
  {
    if (vtx_decl.normals[i].enable)
      SetAttribute(SHADER_NORM0_ATTRIB + i, vtx_decl.normals[i]);
  }

  for (size_t i = 0; i < ArraySize(vtx_decl.colors); i++)
  {
    if (vtx_decl.colors[i].enable)
      SetAttribute(SHADER_COLOR0_ATTRIB + i, vtx_decl.colors[i]);
  }

  for (size_t i = 0; i < ArraySize(vtx_decl.texcoords); i++)
  {
    if (vtx_decl.texcoords[i].enable)
      SetAttribute(SHADER_TEXTURE0_ATTRIB + i, vtx_decl.texcoords[i]);
  }

  if (vtx_decl.posmtx.enable)
    SetAttribute(SHADER_POSMTX_ATTRIB, vtx_decl.posmtx);
}
}  // namespace Metal
