// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <utility>

#include "Common/CommonTypes.h"
#include "Common/PPC64Emitter.h"
#include "VideoCommon/VertexLoaderBase.h"

enum class VertexComponentFormat;
enum class ComponentFormat;
enum class ColorFormat;
enum class CPArray : u8;

class VertexLoaderPPC64 : public VertexLoaderBase, public PPC64Gen::PPC64CodeBlock
{
public:
  VertexLoaderPPC64(const TVtxDesc& vtx_desc, const VAT& vtx_att);

protected:
  int RunVertices(const u8* src, u8* dst, int count) override;

private:
  u32 m_src_ofs = 0;
  u32 m_dst_ofs = 0;
  PPC64Gen::FixupBranch m_skip_vertex;
  std::pair<PPC64Gen::GPR, u32> GetVertexAddr(CPArray array, VertexComponentFormat attribute);
  void ReadVertex(VertexComponentFormat attribute, ComponentFormat format, int count_in,
                  int count_out, bool dequantize, u8 scaling_exponent,
                  AttributeFormat* native_format, PPC64Gen::GPR reg, u32 offset);
  void ReadColor(VertexComponentFormat attribute, ColorFormat format, PPC64Gen::GPR reg,
                 u32 offset);
  void GenerateVertexLoader();
};
