// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "VideoBackends/Metal/Common.h"
#include "VideoCommon/NativeVertexFormat.h"

namespace Metal
{
class MetalVertexFormat : public ::NativeVertexFormat
{
public:
  MetalVertexFormat(const PortableVertexDeclaration& in_vtx_decl);

  const mtlpp::VertexDescriptor& GetDescriptor() const { return m_desc; }
private:
  void InitDescriptor();

  mtlpp::VertexDescriptor m_desc;
};
}  // namespace Metal
