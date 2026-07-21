// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoBackends/Metal/MRCHelpers.h"
#include "VideoCommon/NativeVertexFormat.h"

#include <Metal/Metal.h>

namespace Metal
{
class VertexFormat : public NativeVertexFormat
{
public:
  VertexFormat(const PortableVertexDeclaration& vtx_decl);

  MTLVertexDescriptor* Get() const { return m_desc; }

  MRCOwned<MTLVertexDescriptor*> m_desc;
};
}  // namespace Metal
