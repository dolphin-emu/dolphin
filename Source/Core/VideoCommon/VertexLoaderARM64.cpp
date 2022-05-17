// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexLoaderARM64.h"

#include <array>

#include "Common/CommonTypes.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/VertexLoaderManager.h"

using namespace Arm64Gen;

constexpr ARM64Reg src_reg = ARM64Reg::X0;
constexpr ARM64Reg dst_reg = ARM64Reg::X1;
constexpr ARM64Reg remaining_reg = ARM64Reg::W2;
constexpr ARM64Reg skipped_reg = ARM64Reg::W17;
constexpr ARM64Reg scratch1_reg = ARM64Reg::W16;
constexpr ARM64Reg scratch2_reg = ARM64Reg::W15;
constexpr ARM64Reg scratch3_reg = ARM64Reg::W14;
constexpr ARM64Reg saved_count = ARM64Reg::W12;

constexpr ARM64Reg stride_reg = ARM64Reg::X11;
constexpr ARM64Reg arraybase_reg = ARM64Reg::X10;
constexpr ARM64Reg scale_reg = ARM64Reg::X9;

static constexpr int GetLoadSize(int load_bytes)
{
  if (load_bytes == 1)
    return 1;
  else if (load_bytes <= 2)
    return 2;
  else if (load_bytes <= 4)
    return 4;
  else if (load_bytes <= 8)
    return 8;
  else
    return 16;
}

alignas(16) static const float scale_factors[] = {
    1.0 / (1ULL << 0),  1.0 / (1ULL << 1),  1.0 / (1ULL << 2),  1.0 / (1ULL << 3),
    1.0 / (1ULL << 4),  1.0 / (1ULL << 5),  1.0 / (1ULL << 6),  1.0 / (1ULL << 7),
    1.0 / (1ULL << 8),  1.0 / (1ULL << 9),  1.0 / (1ULL << 10), 1.0 / (1ULL << 11),
    1.0 / (1ULL << 12), 1.0 / (1ULL << 13), 1.0 / (1ULL << 14), 1.0 / (1ULL << 15),
    1.0 / (1ULL << 16), 1.0 / (1ULL << 17), 1.0 / (1ULL << 18), 1.0 / (1ULL << 19),
    1.0 / (1ULL << 20), 1.0 / (1ULL << 21), 1.0 / (1ULL << 22), 1.0 / (1ULL << 23),
    1.0 / (1ULL << 24), 1.0 / (1ULL << 25), 1.0 / (1ULL << 26), 1.0 / (1ULL << 27),
    1.0 / (1ULL << 28), 1.0 / (1ULL << 29), 1.0 / (1ULL << 30), 1.0 / (1ULL << 31),
};

VertexLoaderARM64::VertexLoaderARM64(const TVtxDesc& vtx_desc, const VAT& vtx_att)
    : VertexLoaderBase(vtx_desc, vtx_att), m_float_emit(this)
{
  AllocCodeSpace(4096);
  const Common::ScopedJITPageWriteAndNoExecute enable_jit_page_writes;
  ClearCodeSpace();
  GenerateVertexLoader();
  WriteProtect();
}

void VertexLoaderARM64::GetVertexAddr(CPArray array, VertexComponentFormat attribute, ARM64Reg reg)
{
  if (IsIndexed(attribute))
  {
    if (attribute == VertexComponentFormat::Index8)
    {
      if (m_src_ofs < 4096)
      {
        LDRB(IndexType::Unsigned, scratch1_reg, src_reg, m_src_ofs);
      }
      else
      {
        ADD(reg, src_reg, m_src_ofs);
        LDRB(IndexType::Unsigned, scratch1_reg, reg, 0);
      }
      m_src_ofs += 1;
    }
    else
    {
      if (m_src_ofs < 256)
      {
        LDURH(scratch1_reg, src_reg, m_src_ofs);
      }
      else if (m_src_ofs <= 8190 && !(m_src_ofs & 1))
      {
        LDRH(IndexType::Unsigned, scratch1_reg, src_reg, m_src_ofs);
      }
      else
      {
        ADD(reg, src_reg, m_src_ofs);
        LDRH(IndexType::Unsigned, scratch1_reg, reg, 0);
      }
      m_src_ofs += 2;
      REV16(scratch1_reg, scratch1_reg);
    }

    if (array == CPArray::Position)
    {
      EOR(scratch2_reg, scratch1_reg,
          attribute == VertexComponentFormat::Index8 ? LogicalImm(0xFF, 32) :
                                                       LogicalImm(0xFFFF, 32));
      m_skip_vertex = CBZ(scratch2_reg);
    }

    LDR(IndexType::Unsigned, scratch2_reg, stride_reg, static_cast<u8>(array) * 4);
    MUL(scratch1_reg, scratch1_reg, scratch2_reg);

    LDR(IndexType::Unsigned, EncodeRegTo64(scratch2_reg), arraybase_reg,
        static_cast<u8>(array) * 8);
    ADD(EncodeRegTo64(reg), EncodeRegTo64(scratch1_reg), EncodeRegTo64(scratch2_reg));
  }
  else
    ADD(reg, src_reg, m_src_ofs);
}

s32 VertexLoaderARM64::GetAddressImm(CPArray array, VertexComponentFormat attribute,
                                     Arm64Gen::ARM64Reg reg, u32 align)
{
  if (IsIndexed(attribute) || (m_src_ofs > 255 && (m_src_ofs & (align - 1))))
    GetVertexAddr(array, attribute, reg);
  else
    return m_src_ofs;
  return -1;
}

int VertexLoaderARM64::ReadVertex(VertexComponentFormat attribute, ComponentFormat format,
                                  int count_in, int count_out, bool dequantize, u8 scaling_exponent,
                                  AttributeFormat* native_format, s32 offset)
{
  ARM64Reg coords = count_in == 3 ? ARM64Reg::Q31 : ARM64Reg::D31;
  ARM64Reg scale = count_in == 3 ? ARM64Reg::Q30 : ARM64Reg::D30;

  int elem_size = GetElementSize(format);
  int load_bytes = elem_size * count_in;
  int load_size = GetLoadSize(load_bytes);
  load_size <<= 3;
  elem_size <<= 3;

  if (offset == -1)
  {
    if (count_in == 1)
      m_float_emit.LDR(elem_size, IndexType::Unsigned, coords, EncodeRegTo64(scratch1_reg), 0);
    else
      m_float_emit.LD1(elem_size, 1, coords, EncodeRegTo64(scratch1_reg));
  }
  else if (offset & (load_size - 1))  // Not aligned - unscaled
  {
    m_float_emit.LDUR(load_size, coords, src_reg, offset);
  }
  else
  {
    m_float_emit.LDR(load_size, IndexType::Unsigned, coords, src_reg, offset);
  }

  if (format != ComponentFormat::Float)
  {
    // Extend and convert to float
    switch (format)
    {
    case ComponentFormat::UByte:
      m_float_emit.UXTL(8, EncodeRegToDouble(coords), EncodeRegToDouble(coords));
      m_float_emit.UXTL(16, EncodeRegToDouble(coords), EncodeRegToDouble(coords));
      break;
    case ComponentFormat::Byte:
      m_float_emit.SXTL(8, EncodeRegToDouble(coords), EncodeRegToDouble(coords));
      m_float_emit.SXTL(16, EncodeRegToDouble(coords), EncodeRegToDouble(coords));
      break;
    case ComponentFormat::UShort:
      m_float_emit.REV16(8, EncodeRegToDouble(coords), EncodeRegToDouble(coords));
      m_float_emit.UXTL(16, EncodeRegToDouble(coords), EncodeRegToDouble(coords));
      break;
    case ComponentFormat::Short:
      m_float_emit.REV16(8, EncodeRegToDouble(coords), EncodeRegToDouble(coords));
      m_float_emit.SXTL(16, EncodeRegToDouble(coords), EncodeRegToDouble(coords));
      break;
    }

    m_float_emit.SCVTF(32, coords, coords);

    if (dequantize && scaling_exponent)
    {
      m_float_emit.LDR(32, IndexType::Unsigned, scale, scale_reg, scaling_exponent * 4);
      m_float_emit.FMUL(32, coords, coords, scale, 0);
    }
  }
  else
  {
    m_float_emit.REV32(8, coords, coords);
  }

  const u32 write_size = count_out == 3 ? 128 : count_out * 32;
  const u32 mask = count_out == 3 ? 0xF : count_out == 2 ? 0x7 : 0x3;
  if (m_dst_ofs < 256)
  {
    m_float_emit.STUR(write_size, coords, dst_reg, m_dst_ofs);
  }
  else if (!(m_dst_ofs & mask))
  {
    m_float_emit.STR(write_size, IndexType::Unsigned, coords, dst_reg, m_dst_ofs);
  }
  else
  {
    ADD(EncodeRegTo64(scratch2_reg), dst_reg, m_dst_ofs);
    m_float_emit.ST1(32, 1, coords, EncodeRegTo64(scratch2_reg));
  }

  // Z-Freeze
  if (native_format == &m_native_vtx_decl.position)
  {
    CMP(remaining_reg, 3);
    FixupBranch dont_store = B(CC_GE);
    MOVP2R(EncodeRegTo64(scratch2_reg), VertexLoaderManager::position_cache.data());
    m_float_emit.STR(128, coords, EncodeRegTo64(scratch2_reg), ArithOption(remaining_reg, true));
    SetJumpTarget(dont_store);
  }
  else if (native_format == &m_native_vtx_decl.normals[1])
  {
    FixupBranch dont_store = CBNZ(remaining_reg);
    MOVP2R(EncodeRegTo64(scratch2_reg), VertexLoaderManager::tangent_cache.data());
    m_float_emit.STR(128, IndexType::Unsigned, coords, EncodeRegTo64(scratch2_reg), 0);
    SetJumpTarget(dont_store);
  }
  else if (native_format == &m_native_vtx_decl.normals[2])
  {
    FixupBranch dont_store = CBNZ(remaining_reg);
    MOVP2R(EncodeRegTo64(scratch2_reg), VertexLoaderManager::binormal_cache.data());
    m_float_emit.STR(128, IndexType::Unsigned, coords, EncodeRegTo64(scratch2_reg), 0);
    SetJumpTarget(dont_store);
  }

  native_format->components = count_out;
  native_format->enable = true;
  native_format->offset = m_dst_ofs;
  native_format->type = ComponentFormat::Float;
  native_format->integer = false;
  m_dst_ofs += sizeof(float) * count_out;

  if (attribute == VertexComponentFormat::Direct)
    m_src_ofs += load_bytes;

  return load_bytes;
}

void VertexLoaderARM64::ReadColor(VertexComponentFormat attribute, ColorFormat format, s32 offset)
{
  int load_bytes = 0;
  switch (format)
  {
  case ColorFormat::RGB888:
  case ColorFormat::RGB888x:
  case ColorFormat::RGBA8888:
    if (offset == -1)
      LDR(IndexType::Unsigned, scratch2_reg, EncodeRegTo64(scratch1_reg), 0);
    else if (offset & 3)  // Not aligned - unscaled
      LDUR(scratch2_reg, src_reg, offset);
    else
      LDR(IndexType::Unsigned, scratch2_reg, src_reg, offset);

    if (format != ColorFormat::RGBA8888)
      ORR(scratch2_reg, scratch2_reg, LogicalImm(0xFF000000, 32));
    STR(IndexType::Unsigned, scratch2_reg, dst_reg, m_dst_ofs);
    load_bytes = format == ColorFormat::RGB888 ? 3 : 4;
    break;

  case ColorFormat::RGB565:
    //                   RRRRRGGG GGGBBBBB
    // AAAAAAAA BBBBBBBB GGGGGGGG RRRRRRRR
    if (offset == -1)
      LDRH(IndexType::Unsigned, scratch3_reg, EncodeRegTo64(scratch1_reg), 0);
    else if (offset & 1)  // Not aligned - unscaled
      LDURH(scratch3_reg, src_reg, offset);
    else
      LDRH(IndexType::Unsigned, scratch3_reg, src_reg, offset);

    REV16(scratch3_reg, scratch3_reg);

    // B
    AND(scratch2_reg, scratch3_reg, LogicalImm(0x1F, 32));
    ORR(scratch2_reg, ARM64Reg::WSP, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 3));
    ORR(scratch2_reg, scratch2_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSR, 5));
    ORR(scratch1_reg, ARM64Reg::WSP, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 16));

    // G
    UBFM(scratch2_reg, scratch3_reg, 5, 10);
    ORR(scratch2_reg, ARM64Reg::WSP, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 2));
    ORR(scratch2_reg, scratch2_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSR, 6));
    ORR(scratch1_reg, scratch1_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 8));

    // R
    UBFM(scratch2_reg, scratch3_reg, 11, 15);
    ORR(scratch1_reg, scratch1_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 3));
    ORR(scratch1_reg, scratch1_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSR, 2));

    // A
    ORR(scratch1_reg, scratch1_reg, LogicalImm(0xFF000000, 32));

    STR(IndexType::Unsigned, scratch1_reg, dst_reg, m_dst_ofs);
    load_bytes = 2;
    break;

  case ColorFormat::RGBA4444:
    //                   BBBBAAAA RRRRGGGG
    //           REV16 - RRRRGGGG BBBBAAAA
    // AAAAAAAA BBBBBBBB GGGGGGGG RRRRRRRR
    if (offset == -1)
      LDRH(IndexType::Unsigned, scratch3_reg, EncodeRegTo64(scratch1_reg), 0);
    else if (offset & 1)  // Not aligned - unscaled
      LDURH(scratch3_reg, src_reg, offset);
    else
      LDRH(IndexType::Unsigned, scratch3_reg, src_reg, offset);

    // R
    UBFM(scratch1_reg, scratch3_reg, 4, 7);

    // G
    AND(scratch2_reg, scratch3_reg, LogicalImm(0xF, 32));
    ORR(scratch1_reg, scratch1_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 8));

    // B
    UBFM(scratch2_reg, scratch3_reg, 12, 15);
    ORR(scratch1_reg, scratch1_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 16));

    // A
    UBFM(scratch2_reg, scratch3_reg, 8, 11);
    ORR(scratch1_reg, scratch1_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 24));

    // Final duplication
    ORR(scratch1_reg, scratch1_reg, scratch1_reg, ArithOption(scratch1_reg, ShiftType::LSL, 4));

    STR(IndexType::Unsigned, scratch1_reg, dst_reg, m_dst_ofs);
    load_bytes = 2;
    break;

  case ColorFormat::RGBA6666:
    //          RRRRRRGG GGGGBBBB BBAAAAAA
    // AAAAAAAA BBBBBBBB GGGGGGGG RRRRRRRR
    if (offset == -1)
    {
      LDUR(scratch3_reg, EncodeRegTo64(scratch1_reg), -1);
    }
    else
    {
      offset -= 1;
      if (offset & 3)  // Not aligned - unscaled
        LDUR(scratch3_reg, src_reg, offset);
      else
        LDR(IndexType::Unsigned, scratch3_reg, src_reg, offset);
    }

    REV32(scratch3_reg, scratch3_reg);

    // A
    UBFM(scratch2_reg, scratch3_reg, 0, 5);
    ORR(scratch2_reg, ARM64Reg::WSP, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 2));
    ORR(scratch2_reg, scratch2_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSR, 6));
    ORR(scratch1_reg, ARM64Reg::WSP, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 24));

    // B
    UBFM(scratch2_reg, scratch3_reg, 6, 11);
    ORR(scratch2_reg, ARM64Reg::WSP, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 2));
    ORR(scratch2_reg, scratch2_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSR, 6));
    ORR(scratch1_reg, scratch1_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 16));

    // G
    UBFM(scratch2_reg, scratch3_reg, 12, 17);
    ORR(scratch2_reg, ARM64Reg::WSP, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 2));
    ORR(scratch2_reg, scratch2_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSR, 6));
    ORR(scratch1_reg, scratch1_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 8));

    // R
    UBFM(scratch2_reg, scratch3_reg, 18, 23);
    ORR(scratch1_reg, scratch1_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSL, 2));
    ORR(scratch1_reg, scratch1_reg, scratch2_reg, ArithOption(scratch2_reg, ShiftType::LSR, 4));

    STR(IndexType::Unsigned, scratch1_reg, dst_reg, m_dst_ofs);

    load_bytes = 3;
    break;
  }
  if (attribute == VertexComponentFormat::Direct)
    m_src_ofs += load_bytes;
}

void VertexLoaderARM64::GenerateVertexLoader()
{
  // R0 - Source pointer
  // R1 - Destination pointer
  // R2 - Count
  // R30 - LR
  //
  // R0 return how many
  //
  // Registers we don't have to worry about saving
  // R9-R17 are caller saved temporaries
  // R18 is a temporary or platform specific register(iOS)
  //
  // VFP registers
  // We can touch all except v8-v15
  // If we need to use those, we need to retain the lower 64bits(!) of the register

  bool has_tc = false;
  bool has_tc_scale = false;
  for (size_t i = 0; i < m_VtxDesc.high.TexCoord.Size(); i++)
  {
    has_tc |= m_VtxDesc.high.TexCoord[i] != VertexComponentFormat::NotPresent;
    has_tc_scale |= (m_VtxAttr.GetTexFrac(i) != 0);
  }

  bool need_scale = (m_VtxAttr.g0.ByteDequant && m_VtxAttr.g0.PosFrac) ||
                    (has_tc && has_tc_scale) ||
                    (m_VtxDesc.low.Normal != VertexComponentFormat::NotPresent);

  AlignCode16();
  if (IsIndexed(m_VtxDesc.low.Position))
    MOV(skipped_reg, ARM64Reg::WZR);
  ADD(saved_count, remaining_reg, 1);

  MOVP2R(stride_reg, g_main_cp_state.array_strides.data());
  MOVP2R(arraybase_reg, VertexLoaderManager::cached_arraybases.data());

  if (need_scale)
    MOVP2R(scale_reg, scale_factors);

  const u8* loop_start = GetCodePtr();

  if (m_VtxDesc.low.PosMatIdx)
  {
    LDRB(IndexType::Unsigned, scratch1_reg, src_reg, m_src_ofs);
    AND(scratch1_reg, scratch1_reg, LogicalImm(0x3F, 32));
    STR(IndexType::Unsigned, scratch1_reg, dst_reg, m_dst_ofs);

    // Z-Freeze
    CMP(remaining_reg, 3);
    FixupBranch dont_store = B(CC_GE);
    MOVP2R(EncodeRegTo64(scratch2_reg), VertexLoaderManager::position_matrix_index_cache.data());
    STR(scratch1_reg, EncodeRegTo64(scratch2_reg), ArithOption(remaining_reg, true));
    SetJumpTarget(dont_store);

    m_native_vtx_decl.posmtx.components = 4;
    m_native_vtx_decl.posmtx.enable = true;
    m_native_vtx_decl.posmtx.offset = m_dst_ofs;
    m_native_vtx_decl.posmtx.type = ComponentFormat::UByte;
    m_native_vtx_decl.posmtx.integer = true;
    m_src_ofs += sizeof(u8);
    m_dst_ofs += sizeof(u32);
  }

  std::array<u32, 8> texmatidx_ofs;
  for (size_t i = 0; i < m_VtxDesc.low.TexMatIdx.Size(); i++)
  {
    if (m_VtxDesc.low.TexMatIdx[i])
      texmatidx_ofs[i] = m_src_ofs++;
  }

  // Position
  {
    int elem_size = GetElementSize(m_VtxAttr.g0.PosFormat);
    int pos_elements = m_VtxAttr.g0.PosElements == CoordComponentCount::XY ? 2 : 3;
    int load_bytes = elem_size * pos_elements;
    int load_size = GetLoadSize(load_bytes);
    load_size <<= 3;

    s32 offset = GetAddressImm(CPArray::Position, m_VtxDesc.low.Position,
                               EncodeRegTo64(scratch1_reg), load_size);
    ReadVertex(m_VtxDesc.low.Position, m_VtxAttr.g0.PosFormat, pos_elements, pos_elements,
               m_VtxAttr.g0.ByteDequant, m_VtxAttr.g0.PosFrac, &m_native_vtx_decl.position, offset);
  }

  if (m_VtxDesc.low.Normal != VertexComponentFormat::NotPresent)
  {
    static const u8 map[8] = {7, 6, 15, 14};
    const u8 scaling_exponent = map[u32(m_VtxAttr.g0.NormalFormat.Value())];
    const int limit = m_VtxAttr.g0.NormalElements == NormalComponentCount::NTB ? 3 : 1;

    s32 offset = -1;
    for (int i = 0; i < limit; i++)
    {
      if (!i || m_VtxAttr.g0.NormalIndex3)
      {
        int elem_size = GetElementSize(m_VtxAttr.g0.NormalFormat);

        int load_bytes = elem_size * 3;
        int load_size = GetLoadSize(load_bytes);

        offset = GetAddressImm(CPArray::Normal, m_VtxDesc.low.Normal, EncodeRegTo64(scratch1_reg),
                               load_size << 3);

        if (offset == -1)
          ADD(EncodeRegTo64(scratch1_reg), EncodeRegTo64(scratch1_reg), i * elem_size * 3);
        else
          offset += i * elem_size * 3;
      }
      int bytes_read = ReadVertex(m_VtxDesc.low.Normal, m_VtxAttr.g0.NormalFormat, 3, 3, true,
                                  scaling_exponent, &m_native_vtx_decl.normals[i], offset);

      if (offset == -1)
        ADD(EncodeRegTo64(scratch1_reg), EncodeRegTo64(scratch1_reg), bytes_read);
      else
        offset += bytes_read;
    }
  }

  for (u8 i = 0; i < m_VtxDesc.low.Color.Size(); i++)
  {
    m_native_vtx_decl.colors[i].components = 4;
    m_native_vtx_decl.colors[i].type = ComponentFormat::UByte;
    m_native_vtx_decl.colors[i].integer = false;

    if (m_VtxDesc.low.Color[i] != VertexComponentFormat::NotPresent)
    {
      u32 align = 4;
      if (m_VtxAttr.GetColorFormat(i) == ColorFormat::RGB565 ||
          m_VtxAttr.GetColorFormat(i) == ColorFormat::RGBA4444)
        align = 2;

      s32 offset = GetAddressImm(CPArray::Color0 + i, m_VtxDesc.low.Color[i],
                                 EncodeRegTo64(scratch1_reg), align);
      ReadColor(m_VtxDesc.low.Color[i], m_VtxAttr.GetColorFormat(i), offset);
      m_native_vtx_decl.colors[i].components = 4;
      m_native_vtx_decl.colors[i].enable = true;
      m_native_vtx_decl.colors[i].offset = m_dst_ofs;
      m_native_vtx_decl.colors[i].type = ComponentFormat::UByte;
      m_native_vtx_decl.colors[i].integer = false;
      m_dst_ofs += 4;
    }
  }

  for (u8 i = 0; i < m_VtxDesc.high.TexCoord.Size(); i++)
  {
    m_native_vtx_decl.texcoords[i].offset = m_dst_ofs;
    m_native_vtx_decl.texcoords[i].type = ComponentFormat::Float;
    m_native_vtx_decl.texcoords[i].integer = false;

    int elements = m_VtxAttr.GetTexElements(i) == TexComponentCount::S ? 1 : 2;
    if (m_VtxDesc.high.TexCoord[i] != VertexComponentFormat::NotPresent)
    {
      int elem_size = GetElementSize(m_VtxAttr.GetTexFormat(i));
      int load_bytes = elem_size * (elements + 2);
      int load_size = GetLoadSize(load_bytes);
      load_size <<= 3;

      s32 offset = GetAddressImm(CPArray::TexCoord0 + i, m_VtxDesc.high.TexCoord[i],
                                 EncodeRegTo64(scratch1_reg), load_size);
      u8 scaling_exponent = m_VtxAttr.GetTexFrac(i);
      ReadVertex(m_VtxDesc.high.TexCoord[i], m_VtxAttr.GetTexFormat(i), elements,
                 m_VtxDesc.low.TexMatIdx[i] ? 2 : elements, m_VtxAttr.g0.ByteDequant,
                 scaling_exponent, &m_native_vtx_decl.texcoords[i], offset);
    }
    if (m_VtxDesc.low.TexMatIdx[i])
    {
      m_native_vtx_decl.texcoords[i].components = 3;
      m_native_vtx_decl.texcoords[i].enable = true;
      m_native_vtx_decl.texcoords[i].type = ComponentFormat::Float;
      m_native_vtx_decl.texcoords[i].integer = false;

      LDRB(IndexType::Unsigned, scratch2_reg, src_reg, texmatidx_ofs[i]);
      m_float_emit.UCVTF(ARM64Reg::S31, scratch2_reg);

      if (m_VtxDesc.high.TexCoord[i] != VertexComponentFormat::NotPresent)
      {
        m_float_emit.STR(32, IndexType::Unsigned, ARM64Reg::D31, dst_reg, m_dst_ofs);
        m_dst_ofs += sizeof(float);
      }
      else
      {
        m_native_vtx_decl.texcoords[i].offset = m_dst_ofs;

        if (m_dst_ofs < 256)
        {
          STUR(ARM64Reg::SP, dst_reg, m_dst_ofs);
        }
        else if (!(m_dst_ofs & 7))
        {
          // If m_dst_ofs isn't 8byte aligned we can't store an 8byte zero register
          // So store two 4byte zero registers
          // The destination is always 4byte aligned
          STR(IndexType::Unsigned, ARM64Reg::WSP, dst_reg, m_dst_ofs);
          STR(IndexType::Unsigned, ARM64Reg::WSP, dst_reg, m_dst_ofs + 4);
        }
        else
        {
          STR(IndexType::Unsigned, ARM64Reg::SP, dst_reg, m_dst_ofs);
        }
        m_float_emit.STR(32, IndexType::Unsigned, ARM64Reg::D31, dst_reg, m_dst_ofs + 8);

        m_dst_ofs += sizeof(float) * 3;
      }
    }
  }

  // Prepare for the next vertex.
  ADD(dst_reg, dst_reg, m_dst_ofs);
  const u8* cont = GetCodePtr();
  ADD(src_reg, src_reg, m_src_ofs);

  SUBS(remaining_reg, remaining_reg, 1);
  B(CCFlags::CC_GE, loop_start);

  if (IsIndexed(m_VtxDesc.low.Position))
  {
    SUB(ARM64Reg::W0, saved_count, skipped_reg);
    RET(ARM64Reg::X30);

    SetJumpTarget(m_skip_vertex);
    ADD(skipped_reg, skipped_reg, 1);
    B(cont);
  }
  else
  {
    MOV(ARM64Reg::W0, saved_count);
    RET(ARM64Reg::X30);
  }

  FlushIcache();

  ASSERT(m_vertex_size == m_src_ofs);
  m_native_vtx_decl.stride = m_dst_ofs;
}

int VertexLoaderARM64::RunVertices(DataReader src, DataReader dst, int count)
{
  m_numLoadedVertices += count;
  return ((int (*)(u8 * src, u8 * dst, int count)) region)(src.GetPointer(), dst.GetPointer(),
                                                           count - 1);
}
