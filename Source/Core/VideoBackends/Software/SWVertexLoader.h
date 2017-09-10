// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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

  std::unique_ptr<NativeVertexFormat>
  CreateNativeVertexFormat(const PortableVertexDeclaration& vdec) override;

private:
  void ResetBuffer(u32 stride) override;
  void vFlush() override;

  void SetFormat(u8 attributeIndex, u8 primitiveType);
  void ParseVertex(const PortableVertexDeclaration& vdec, int index);

  std::vector<u8> m_local_vertex_buffer;
  std::vector<u16> m_local_index_buffer;

  InputVertexData m_vertex;
  SetupUnit m_setup_unit;

  bool m_tex_gen_special_case;
};
