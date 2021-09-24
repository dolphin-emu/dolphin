// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include "Common/CommonTypes.h"

#include "VideoBackends/Software/NativeVertexFormat.h"
#include "VideoBackends/Software/SetupUnit.h"

#include "VideoCommon/VertexManagerBase.h"

class SWVertexLoader final : public VertexManagerBase
{
public:
  SWVertexLoader();
  ~SWVertexLoader();

protected:
  void DrawCurrentBatch(u32 base_index, u32 num_indices, u32 base_vertex) override;

  void SetFormat(u8 attributeIndex, u8 primitiveType);
  void ParseVertex(const PortableVertexDeclaration& vdec, int index);

  InputVertexData m_vertex;
  SetupUnit m_setup_unit;
};
