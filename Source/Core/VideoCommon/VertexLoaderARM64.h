// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <utility>

#include "Common/Arm64Emitter.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/VertexLoaderBase.h"

enum class VertexComponentFormat;
enum class ComponentFormat;
enum class ColorFormat;
enum class CPArray : u8;

class VertexLoaderARM64 : public VertexLoaderBase, public Arm64Gen::ARM64CodeBlock
{
public:
  VertexLoaderARM64(const TVtxDesc& vtx_desc, const VAT& vtx_att);

protected:
  int RunVertices(const u8* src, u8* dst, int count) override;

private:
  u32 m_src_ofs = 0;
  u32 m_dst_ofs = 0;
  Arm64Gen::FixupBranch m_skip_vertex;
  Arm64Gen::ARM64FloatEmitter m_float_emit;
  std::pair<Arm64Gen::ARM64Reg, u32> GetVertexAddr(CPArray array, VertexComponentFormat attribute);
  void ReadVertex(VertexComponentFormat attribute, ComponentFormat format, int count_in,
                  int count_out, bool dequantize, u8 scaling_exponent,
                  AttributeFormat* native_format, Arm64Gen::ARM64Reg reg, u32 offset);
  void ReadColor(VertexComponentFormat attribute, ColorFormat format, Arm64Gen::ARM64Reg reg,
                 u32 offset);
  void GenerateVertexLoader();
};
