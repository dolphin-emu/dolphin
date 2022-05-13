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

  DataReader PrepareForAdditionalData(OpcodeDecoder::Primitive primitive, u32 count, u32 stride,
                                      bool cullall) override;

protected:
  void DrawCurrentBatch(u32 base_index, u32 num_indices, u32 base_vertex) override;

  void SetFormat();
  void ParseVertex(const PortableVertexDeclaration& vdec, int index);

  InputVertexData m_vertex{};
  SetupUnit m_setup_unit;
};
