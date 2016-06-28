// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "VideoCommon/VertexManagerBase.h"

namespace Null
{
class VertexManager : public VertexManagerBase
{
public:
  VertexManager();
  ~VertexManager();
  NativeVertexFormat* CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) override;

protected:
  void ResetBuffer(u32 stride) override;

private:
  void vFlush(bool use_dst_alpha) override;
  std::vector<u8> m_local_v_buffer;
  std::vector<u16> m_local_i_buffer;
};
}
