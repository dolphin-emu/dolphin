// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/x64Emitter.h"
#include "VideoCommon/VertexLoaderBase.h"

enum class VertexComponentFormat;
enum class ComponentFormat;
enum class ColorFormat;
enum class CPArray : u8;

class VertexLoaderX64 : public VertexLoaderBase, public Gen::X64CodeBlock
{
public:
  VertexLoaderX64(const TVtxDesc& vtx_desc, const VAT& vtx_att);

protected:
  int RunVertices(const u8* src, u8* dst, int count) override;

private:
  u32 m_src_ofs = 0;
  u32 m_dst_ofs = 0;
  Gen::FixupBranch m_skip_vertex;
  Gen::OpArg GetVertexAddr(CPArray array, VertexComponentFormat attribute);
  void ReadVertex(Gen::OpArg data, VertexComponentFormat attribute, ComponentFormat format,
                  int count_in, int count_out, bool dequantize, u8 scaling_exponent,
                  AttributeFormat* native_format);
  void ReadColor(Gen::OpArg data, VertexComponentFormat attribute, ColorFormat format);
  void GenerateVertexLoader();
};
