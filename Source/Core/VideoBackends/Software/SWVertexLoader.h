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

class SWVertexLoader : public VertexManagerBase
{
public:
  SWVertexLoader();
  ~SWVertexLoader();

  std::unique_ptr<NativeVertexFormat>
  CreateNativeVertexFormat(const PortableVertexDeclaration& vdec) override;

protected:
  void ResetBuffer(u32 stride) override;

private:
  void vFlush() override;
  std::vector<u8> m_local_vertex_buffer;
  std::vector<u16> m_local_index_buffer;

  InputVertexData m_vertex;

  void ParseVertex(const PortableVertexDeclaration& vdec, int index);

  SetupUnit m_setup_unit;

  bool m_tex_gen_special_case;

public:
  void SetFormat(u8 attributeIndex, u8 primitiveType);
};
