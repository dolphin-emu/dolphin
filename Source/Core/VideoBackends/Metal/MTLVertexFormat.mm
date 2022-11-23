// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoBackends/Metal/MTLVertexFormat.h"

#include "VideoCommon/VertexShaderGen.h"

static MTLVertexFormat ConvertFormat(ComponentFormat format, int count, bool int_format)
{
  // clang-format off
  if (int_format)
  {
    switch (format)
    {
    case ComponentFormat::UByte:
      switch (count)
      {
      case 1: return MTLVertexFormatUChar;
      case 2: return MTLVertexFormatUChar2;
      case 3: return MTLVertexFormatUChar3;
      case 4: return MTLVertexFormatUChar4;
      default: return MTLVertexFormatInvalid;
      }
    case ComponentFormat::Byte:
      switch (count)
      {
      case 1: return MTLVertexFormatChar;
      case 2: return MTLVertexFormatChar2;
      case 3: return MTLVertexFormatChar3;
      case 4: return MTLVertexFormatChar4;
      default: return MTLVertexFormatInvalid;
      }
    case ComponentFormat::UShort:
      switch (count)
      {
      case 1: return MTLVertexFormatUShort;
      case 2: return MTLVertexFormatUShort2;
      case 3: return MTLVertexFormatUShort3;
      case 4: return MTLVertexFormatUShort4;
      default: return MTLVertexFormatInvalid;
      }
    case ComponentFormat::Short:
      switch (count)
      {
      case 1: return MTLVertexFormatShort;
      case 2: return MTLVertexFormatShort2;
      case 3: return MTLVertexFormatShort3;
      case 4: return MTLVertexFormatShort4;
      default: return MTLVertexFormatInvalid;
      }
    case ComponentFormat::Float:
      switch (count)
      {
      case 1: return MTLVertexFormatFloat;
      case 2: return MTLVertexFormatFloat2;
      case 3: return MTLVertexFormatFloat3;
      case 4: return MTLVertexFormatFloat4;
      default: return MTLVertexFormatInvalid;
      }
    }
  }
  else
  {
    switch (format)
    {
    case ComponentFormat::UByte:
      switch (count)
      {
      case 1: return MTLVertexFormatUCharNormalized;
      case 2: return MTLVertexFormatUChar2Normalized;
      case 3: return MTLVertexFormatUChar3Normalized;
      case 4: return MTLVertexFormatUChar4Normalized;
      default: return MTLVertexFormatInvalid;
      }
    case ComponentFormat::Byte:
      switch (count)
      {
      case 1: return MTLVertexFormatCharNormalized;
      case 2: return MTLVertexFormatChar2Normalized;
      case 3: return MTLVertexFormatChar3Normalized;
      case 4: return MTLVertexFormatChar4Normalized;
      default: return MTLVertexFormatInvalid;
      }
    case ComponentFormat::UShort:
      switch (count)
      {
      case 1: return MTLVertexFormatUShortNormalized;
      case 2: return MTLVertexFormatUShort2Normalized;
      case 3: return MTLVertexFormatUShort3Normalized;
      case 4: return MTLVertexFormatUShort4Normalized;
      default: return MTLVertexFormatInvalid;
      }
    case ComponentFormat::Short:
      switch (count)
      {
      case 1: return MTLVertexFormatShortNormalized;
      case 2: return MTLVertexFormatShort2Normalized;
      case 3: return MTLVertexFormatShort3Normalized;
      case 4: return MTLVertexFormatShort4Normalized;
      default: return MTLVertexFormatInvalid;
      }
    case ComponentFormat::Float:
      switch (count)
      {
      case 1: return MTLVertexFormatFloat;
      case 2: return MTLVertexFormatFloat2;
      case 3: return MTLVertexFormatFloat3;
      case 4: return MTLVertexFormatFloat4;
      default: return MTLVertexFormatInvalid;
      }
    }
  }
  // clang-format on
}

static void SetAttribute(MTLVertexDescriptor* desc, ShaderAttrib attribute,
                         const AttributeFormat& format)
{
  if (!format.enable)
    return;
  MTLVertexAttributeDescriptor* attr_desc =
      [[desc attributes] objectAtIndexedSubscript:(u32)attribute];
  [attr_desc setFormat:ConvertFormat(format.type, format.components, format.integer)];
  [attr_desc setOffset:format.offset];
  [attr_desc setBufferIndex:0];
}

template <size_t N>
static void SetAttributes(MTLVertexDescriptor* desc, ShaderAttrib attribute,
                          const std::array<AttributeFormat, N>& format)
{
  for (size_t i = 0; i < N; ++i)
    SetAttribute(desc, attribute + i, format[i]);
}

Metal::VertexFormat::VertexFormat(const PortableVertexDeclaration& vtx_decl)
    : NativeVertexFormat(vtx_decl), m_desc(MRCTransfer([MTLVertexDescriptor new]))
{
  [[[m_desc layouts] objectAtIndexedSubscript:0] setStride:vtx_decl.stride];
  SetAttribute(m_desc, ShaderAttrib::Position, vtx_decl.position);
  SetAttributes(m_desc, ShaderAttrib::Normal, vtx_decl.normals);
  SetAttributes(m_desc, ShaderAttrib::Color0, vtx_decl.colors);
  SetAttributes(m_desc, ShaderAttrib::TexCoord0, vtx_decl.texcoords);
  SetAttribute(m_desc, ShaderAttrib::PositionMatrix, vtx_decl.posmtx);
}
