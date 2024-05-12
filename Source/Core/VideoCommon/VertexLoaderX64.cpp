// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/VertexLoaderX64.h"

#include <array>
#include <cstring>
#include <string>

#include "Common/BitSet.h"
#include "Common/CPUDetect.h"
#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/Intrinsics.h"
#include "Common/JitRegister.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/VertexLoaderManager.h"

using namespace Gen;

static const X64Reg src_reg = ABI_PARAM1;
static const X64Reg dst_reg = ABI_PARAM2;
static const X64Reg scratch1 = RAX;
static const X64Reg scratch2 = ABI_PARAM3;
static const X64Reg scratch3 = ABI_PARAM4;
// The remaining number of vertices to be processed.  Starts at count - 1, and the final loop has it
// at 0.
static const X64Reg remaining_reg = R10;
static const X64Reg skipped_reg = R11;
static const X64Reg base_reg = RBX;

static const u8* memory_base_ptr = (u8*)&g_main_cp_state.array_strides;

static OpArg MPIC(const void* ptr, X64Reg scale_reg, int scale = SCALE_1)
{
  return MComplex(base_reg, scale_reg, scale, PtrOffset(ptr, memory_base_ptr));
}

static OpArg MPIC(const void* ptr)
{
  return MDisp(base_reg, PtrOffset(ptr, memory_base_ptr));
}

VertexLoaderX64::VertexLoaderX64(const TVtxDesc& vtx_desc, const VAT& vtx_att)
    : VertexLoaderBase(vtx_desc, vtx_att)
{
  AllocCodeSpace(4096);
  ClearCodeSpace();
  GenerateVertexLoader();
  WriteProtect(true);

  Common::JitRegister::Register(region, GetCodePtr(), "VertexLoaderX64\nVtx desc: \n{}\nVAT:\n{}",
                                vtx_desc, vtx_att);
}

OpArg VertexLoaderX64::GetVertexAddr(CPArray array, VertexComponentFormat attribute)
{
  OpArg data = MDisp(src_reg, m_src_ofs);
  if (IsIndexed(attribute))
  {
    int bits = attribute == VertexComponentFormat::Index8 ? 8 : 16;
    LoadAndSwap(bits, scratch1, data);
    m_src_ofs += bits / 8;
    if (array == CPArray::Position)
    {
      CMP(bits, R(scratch1), Imm8(-1));
      m_skip_vertex = J_CC(CC_E, Jump::Near);
    }
    IMUL(32, scratch1, MPIC(&g_main_cp_state.array_strides[array]));
    MOV(64, R(scratch2), MPIC(&VertexLoaderManager::cached_arraybases[array]));
    return MRegSum(scratch1, scratch2);
  }
  else
  {
    return data;
  }
}

void VertexLoaderX64::ReadVertex(OpArg data, VertexComponentFormat attribute,
                                 ComponentFormat format, int count_in, int count_out,
                                 bool dequantize, u8 scaling_exponent,
                                 AttributeFormat* native_format)
{
  using ShuffleRow = std::array<__m128i, 3>;
  static const Common::EnumMap<ShuffleRow, ComponentFormat::InvalidFloat7> shuffle_lut = {
      ShuffleRow{_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFF00L),   // 1x u8
                 _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFF01L, 0xFFFFFF00L),   // 2x u8
                 _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFF02L, 0xFFFFFF01L, 0xFFFFFF00L)},  // 3x u8
      ShuffleRow{_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0x00FFFFFFL),   // 1x s8
                 _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x01FFFFFFL, 0x00FFFFFFL),   // 2x s8
                 _mm_set_epi32(0xFFFFFFFFL, 0x02FFFFFFL, 0x01FFFFFFL, 0x00FFFFFFL)},  // 3x s8
      ShuffleRow{_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFF0001L),   // 1x u16
                 _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFF0203L, 0xFFFF0001L),   // 2x u16
                 _mm_set_epi32(0xFFFFFFFFL, 0xFFFF0405L, 0xFFFF0203L, 0xFFFF0001L)},  // 3x u16
      ShuffleRow{_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0x0001FFFFL),   // 1x s16
                 _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x0203FFFFL, 0x0001FFFFL),   // 2x s16
                 _mm_set_epi32(0xFFFFFFFFL, 0x0405FFFFL, 0x0203FFFFL, 0x0001FFFFL)},  // 3x s16
      ShuffleRow{_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0x00010203L),   // 1x float
                 _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x04050607L, 0x00010203L),   // 2x float
                 _mm_set_epi32(0xFFFFFFFFL, 0x08090A0BL, 0x04050607L, 0x00010203L)},  // 3x float
      ShuffleRow{_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0x00010203L),   // 1x invalid
                 _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x04050607L, 0x00010203L),   // 2x invalid
                 _mm_set_epi32(0xFFFFFFFFL, 0x08090A0BL, 0x04050607L, 0x00010203L)},  // 3x invalid
      ShuffleRow{_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0x00010203L),   // 1x invalid
                 _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x04050607L, 0x00010203L),   // 2x invalid
                 _mm_set_epi32(0xFFFFFFFFL, 0x08090A0BL, 0x04050607L, 0x00010203L)},  // 3x invalid
      ShuffleRow{_mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0xFFFFFFFFL, 0x00010203L),   // 1x invalid
                 _mm_set_epi32(0xFFFFFFFFL, 0xFFFFFFFFL, 0x04050607L, 0x00010203L),   // 2x invalid
                 _mm_set_epi32(0xFFFFFFFFL, 0x08090A0BL, 0x04050607L, 0x00010203L)},  // 3x invalid
  };
  static const __m128 scale_factors[32] = {
      _mm_set_ps1(1. / (1u << 0)),  _mm_set_ps1(1. / (1u << 1)),  _mm_set_ps1(1. / (1u << 2)),
      _mm_set_ps1(1. / (1u << 3)),  _mm_set_ps1(1. / (1u << 4)),  _mm_set_ps1(1. / (1u << 5)),
      _mm_set_ps1(1. / (1u << 6)),  _mm_set_ps1(1. / (1u << 7)),  _mm_set_ps1(1. / (1u << 8)),
      _mm_set_ps1(1. / (1u << 9)),  _mm_set_ps1(1. / (1u << 10)), _mm_set_ps1(1. / (1u << 11)),
      _mm_set_ps1(1. / (1u << 12)), _mm_set_ps1(1. / (1u << 13)), _mm_set_ps1(1. / (1u << 14)),
      _mm_set_ps1(1. / (1u << 15)), _mm_set_ps1(1. / (1u << 16)), _mm_set_ps1(1. / (1u << 17)),
      _mm_set_ps1(1. / (1u << 18)), _mm_set_ps1(1. / (1u << 19)), _mm_set_ps1(1. / (1u << 20)),
      _mm_set_ps1(1. / (1u << 21)), _mm_set_ps1(1. / (1u << 22)), _mm_set_ps1(1. / (1u << 23)),
      _mm_set_ps1(1. / (1u << 24)), _mm_set_ps1(1. / (1u << 25)), _mm_set_ps1(1. / (1u << 26)),
      _mm_set_ps1(1. / (1u << 27)), _mm_set_ps1(1. / (1u << 28)), _mm_set_ps1(1. / (1u << 29)),
      _mm_set_ps1(1. / (1u << 30)), _mm_set_ps1(1. / (1u << 31)),
  };

  X64Reg coords = XMM0;

  const auto write_zfreeze = [&]() {  // zfreeze
    if (native_format == &m_native_vtx_decl.position)
    {
      CMP(32, R(remaining_reg), Imm8(3));
      FixupBranch dont_store = J_CC(CC_AE);
      // The position cache is composed of 3 rows of 4 floats each; since each float is 4 bytes,
      // we need to scale by 4 twice to cover the 4 floats.
      LEA(32, scratch3, MScaled(remaining_reg, SCALE_4, 0));
      MOVUPS(MPIC(VertexLoaderManager::position_cache.data(), scratch3, SCALE_4), coords);
      SetJumpTarget(dont_store);
    }
    else if (native_format == &m_native_vtx_decl.normals[1])
    {
      TEST(32, R(remaining_reg), R(remaining_reg));
      FixupBranch dont_store = J_CC(CC_NZ);
      // For similar reasons, the cached tangent and binormal are 4 floats each
      MOVUPS(MPIC(VertexLoaderManager::tangent_cache.data()), coords);
      SetJumpTarget(dont_store);
    }
    else if (native_format == &m_native_vtx_decl.normals[2])
    {
      CMP(32, R(remaining_reg), R(remaining_reg));
      FixupBranch dont_store = J_CC(CC_NZ);
      // For similar reasons, the cached tangent and binormal are 4 floats each
      MOVUPS(MPIC(VertexLoaderManager::binormal_cache.data()), coords);
      SetJumpTarget(dont_store);
    }
  };

  int elem_size = GetElementSize(format);
  int load_bytes = elem_size * count_in;
  OpArg dest = MDisp(dst_reg, m_dst_ofs);

  native_format->components = count_out;
  native_format->enable = true;
  native_format->offset = m_dst_ofs;
  native_format->type = ComponentFormat::Float;
  native_format->integer = false;

  m_dst_ofs += sizeof(float) * count_out;

  if (attribute == VertexComponentFormat::Direct)
    m_src_ofs += load_bytes;

  if (cpu_info.bSSSE3)
  {
    if (load_bytes > 8)
      MOVDQU(coords, data);
    else if (load_bytes > 4)
      MOVQ_xmm(coords, data);
    else
      MOVD_xmm(coords, data);

    PSHUFB(coords, MPIC(&shuffle_lut[format][count_in - 1]));

    // Sign-extend.
    if (format == ComponentFormat::Byte)
      PSRAD(coords, 24);
    if (format == ComponentFormat::Short)
      PSRAD(coords, 16);
  }
  else
  {
    // SSE2
    X64Reg temp = XMM1;
    switch (format)
    {
    case ComponentFormat::UByte:
      MOVD_xmm(coords, data);
      PXOR(temp, R(temp));
      PUNPCKLBW(coords, R(temp));
      PUNPCKLWD(coords, R(temp));
      break;
    case ComponentFormat::Byte:
      MOVD_xmm(coords, data);
      PUNPCKLBW(coords, R(coords));
      PUNPCKLWD(coords, R(coords));
      PSRAD(coords, 24);
      break;
    case ComponentFormat::UShort:
    case ComponentFormat::Short:
      switch (count_in)
      {
      case 1:
        LoadAndSwap(32, scratch3, data);
        MOVD_xmm(coords, R(scratch3));  // ......X.
        break;
      case 2:
        LoadAndSwap(32, scratch3, data);
        MOVD_xmm(coords, R(scratch3));     // ......XY
        PSHUFLW(coords, R(coords), 0x24);  // ....Y.X.
        break;
      case 3:
        LoadAndSwap(64, scratch3, data);
        MOVQ_xmm(coords, R(scratch3));     // ....XYZ.
        PUNPCKLQDQ(coords, R(coords));     // ..Z.XYZ.
        PSHUFLW(coords, R(coords), 0xAC);  // ..Z.Y.X.
        break;
      }
      if (format == ComponentFormat::Short)
        PSRAD(coords, 16);
      else
        PSRLD(coords, 16);
      break;
    case ComponentFormat::Float:
    case ComponentFormat::InvalidFloat5:
    case ComponentFormat::InvalidFloat6:
    case ComponentFormat::InvalidFloat7:
      // Floats don't need to be scaled or converted,
      // so we can just load/swap/store them directly
      // and return early.
      // (In SSSE3 we still need to store them.)
      for (int i = 0; i < count_in; i++)
      {
        LoadAndSwap(32, scratch3, data);
        MOV(32, dest, R(scratch3));
        data.AddMemOffset(sizeof(float));
        dest.AddMemOffset(sizeof(float));

        // zfreeze
        if (native_format == &m_native_vtx_decl.position ||
            native_format == &m_native_vtx_decl.normals[1] ||
            native_format == &m_native_vtx_decl.normals[2])
        {
          if (cpu_info.bSSE4_1)
          {
            PINSRD(coords, R(scratch3), i);
          }
          else
          {
            PINSRW(coords, R(scratch3), 2 * i + 0);
            SHR(32, R(scratch3), Imm8(16));
            PINSRW(coords, R(scratch3), 2 * i + 1);
          }
        }
      }

      write_zfreeze();
    }
  }

  if (format < ComponentFormat::Float)
  {
    CVTDQ2PS(coords, R(coords));

    if (dequantize && scaling_exponent)
      MULPS(coords, MPIC(&scale_factors[scaling_exponent]));
  }

  switch (count_out)
  {
  case 1:
    MOVSS(dest, coords);
    break;
  case 2:
    MOVLPS(dest, coords);
    break;
  case 3:
    MOVUPS(dest, coords);
    break;
  }

  write_zfreeze();
}

void VertexLoaderX64::ReadColor(OpArg data, VertexComponentFormat attribute, ColorFormat format)
{
  int load_bytes = 0;
  switch (format)
  {
  case ColorFormat::RGB888:
  case ColorFormat::RGB888x:
  case ColorFormat::RGBA8888:
    MOV(32, R(scratch1), data);
    if (format != ColorFormat::RGBA8888)
      OR(32, R(scratch1), Imm32(0xFF000000));
    MOV(32, MDisp(dst_reg, m_dst_ofs), R(scratch1));
    load_bytes = format == ColorFormat::RGB888 ? 3 : 4;
    break;

  case ColorFormat::RGB565:
    //                   RRRRRGGG GGGBBBBB
    // AAAAAAAA BBBBBBBB GGGGGGGG RRRRRRRR
    LoadAndSwap(16, scratch1, data);
    if (cpu_info.bBMI1 && cpu_info.bBMI2FastParallelBitOps)
    {
      MOV(32, R(scratch2), Imm32(0x07C3F7C0));
      PDEP(32, scratch3, scratch1, R(scratch2));

      MOV(32, R(scratch2), Imm32(0xF8FCF800));
      PDEP(32, scratch1, scratch1, R(scratch2));
      ANDN(32, scratch2, scratch2, R(scratch3));

      OR(32, R(scratch1), R(scratch2));
    }
    else
    {
      SHL(32, R(scratch1), Imm8(11));
      LEA(32, scratch2, MScaled(scratch1, SCALE_4, 0));
      LEA(32, scratch3, MScaled(scratch2, SCALE_8, 0));
      AND(32, R(scratch1), Imm32(0x0000F800));
      AND(32, R(scratch2), Imm32(0x00FC0000));
      AND(32, R(scratch3), Imm32(0xF8000000));
      OR(32, R(scratch1), R(scratch2));
      OR(32, R(scratch1), R(scratch3));

      MOV(32, R(scratch2), R(scratch1));
      SHR(32, R(scratch1), Imm8(5));
      AND(32, R(scratch1), Imm32(0x07000700));
      OR(32, R(scratch1), R(scratch2));

      SHR(32, R(scratch2), Imm8(6));
      AND(32, R(scratch2), Imm32(0x00030000));
      OR(32, R(scratch1), R(scratch2));
    }
    OR(32, R(scratch1), Imm32(0x000000FF));
    SwapAndStore(32, MDisp(dst_reg, m_dst_ofs), scratch1);
    load_bytes = 2;
    break;

  case ColorFormat::RGBA4444:
    //                   RRRRGGGG BBBBAAAA
    // AAAAAAAA BBBBBBBB GGGGGGGG RRRRRRRR
    LoadAndSwap(16, scratch1, data);
    if (cpu_info.bBMI2FastParallelBitOps)
    {
      MOV(32, R(scratch2), Imm32(0x0F0F0F0F));
      PDEP(32, scratch1, scratch1, R(scratch2));
    }
    else
    {
      MOV(32, R(scratch2), R(scratch1));
      SHL(32, R(scratch1), Imm8(8));
      OR(32, R(scratch1), R(scratch2));
      AND(32, R(scratch1), Imm32(0x00FF00FF));

      MOV(32, R(scratch2), R(scratch1));
      SHL(32, R(scratch1), Imm8(4));
      OR(32, R(scratch1), R(scratch2));
      AND(32, R(scratch1), Imm32(0x0F0F0F0F));
    }
    MOV(32, R(scratch2), R(scratch1));
    SHL(32, R(scratch1), Imm8(4));
    OR(32, R(scratch1), R(scratch2));
    SwapAndStore(32, MDisp(dst_reg, m_dst_ofs), scratch1);
    load_bytes = 2;
    break;

  case ColorFormat::RGBA6666:
    //          RRRRRRGG GGGGBBBB BBAAAAAA
    // AAAAAAAA BBBBBBBB GGGGGGGG RRRRRRRR
    data.AddMemOffset(-1);  // subtract one from address so we can use a 32bit load and bswap
    LoadAndSwap(32, scratch1, data);
    if (cpu_info.bBMI2FastParallelBitOps)
    {
      MOV(32, R(scratch2), Imm32(0xFCFCFCFC));
      PDEP(32, scratch1, scratch1, R(scratch2));
      MOV(32, R(scratch2), R(scratch1));
    }
    else
    {
      LEA(32, scratch2, MScaled(scratch1, SCALE_4, 0));  // ______RR RRRRGGGG GGBBBBBB AAAAAA__
      AND(32, R(scratch2), Imm32(0x00003FFC));           // ________ ________ __BBBBBB AAAAAA__
      SHL(32, R(scratch1), Imm8(6));                     // __RRRRRR GGGGGGBB BBBBAAAA AA______
      AND(32, R(scratch1), Imm32(0x3FFC0000));           // __RRRRRR GGGGGG__ ________ ________
      OR(32, R(scratch1), R(scratch2));                  // __RRRRRR GGGGGG__ __BBBBBB AAAAAA__

      LEA(32, scratch2, MScaled(scratch1, SCALE_4, 0));  // RRRRRRGG GGGG____ BBBBBBAA AAAA____
      AND(32, R(scratch2), Imm32(0xFC00FC00));           // RRRRRR__ ________ BBBBBB__ ________
      AND(32, R(scratch1), Imm32(0x00FC00FC));           // ________ GGGGGG__ ________ AAAAAA__
      OR(32, R(scratch1), R(scratch2));                  // RRRRRR__ GGGGGG__ BBBBBB__ AAAAAA__
      MOV(32, R(scratch2), R(scratch1));
    }
    SHR(32, R(scratch1), Imm8(6));
    AND(32, R(scratch1), Imm32(0x03030303));
    OR(32, R(scratch1), R(scratch2));
    SwapAndStore(32, MDisp(dst_reg, m_dst_ofs), scratch1);
    load_bytes = 3;
    break;
  }
  if (attribute == VertexComponentFormat::Direct)
    m_src_ofs += load_bytes;
}

void VertexLoaderX64::GenerateVertexLoader()
{
  BitSet32 regs = {src_reg,  dst_reg,       scratch1,    scratch2,
                   scratch3, remaining_reg, skipped_reg, base_reg};
  regs &= ABI_ALL_CALLEE_SAVED;
  regs[RBP] = true;  // Give us a stack frame
  ABI_PushRegistersAndAdjustStack(regs, 0);

  // Backup count since we're going to count it down.
  PUSH(32, R(ABI_PARAM3));

  // ABI_PARAM3 is one of the lower registers, so free it for scratch2.
  // We also have it end at a value of 0, to simplify indexing for zfreeze;
  // this requires subtracting 1 at the start.
  LEA(32, remaining_reg, MDisp(ABI_PARAM3, -1));

  MOV(64, R(base_reg), R(ABI_PARAM4));

  if (IsIndexed(m_VtxDesc.low.Position))
    XOR(32, R(skipped_reg), R(skipped_reg));

  // TODO: load constants into registers outside the main loop

  const u8* loop_start = GetCodePtr();

  if (m_VtxDesc.low.PosMatIdx)
  {
    MOVZX(32, 8, scratch1, MDisp(src_reg, m_src_ofs));
    AND(32, R(scratch1), Imm8(0x3F));
    MOV(32, MDisp(dst_reg, m_dst_ofs), R(scratch1));

    // zfreeze
    CMP(32, R(remaining_reg), Imm8(3));
    FixupBranch dont_store = J_CC(CC_AE);
    MOV(32, MPIC(VertexLoaderManager::position_matrix_index_cache.data(), remaining_reg, SCALE_4),
        R(scratch1));
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

  OpArg data = GetVertexAddr(CPArray::Position, m_VtxDesc.low.Position);
  int pos_elements = m_VtxAttr.g0.PosElements == CoordComponentCount::XY ? 2 : 3;
  ReadVertex(data, m_VtxDesc.low.Position, m_VtxAttr.g0.PosFormat, pos_elements, pos_elements,
             m_VtxAttr.g0.ByteDequant, m_VtxAttr.g0.PosFrac, &m_native_vtx_decl.position);

  if (m_VtxDesc.low.Normal != VertexComponentFormat::NotPresent)
  {
    static constexpr Common::EnumMap<u8, ComponentFormat::InvalidFloat7> SCALE_MAP = {7, 6, 15, 14,
                                                                                      0, 0, 0,  0};
    const u8 scaling_exponent = SCALE_MAP[m_VtxAttr.g0.NormalFormat];

    // Normal
    data = GetVertexAddr(CPArray::Normal, m_VtxDesc.low.Normal);
    ReadVertex(data, m_VtxDesc.low.Normal, m_VtxAttr.g0.NormalFormat, 3, 3, true, scaling_exponent,
               &m_native_vtx_decl.normals[0]);

    if (m_VtxAttr.g0.NormalElements == NormalComponentCount::NTB)
    {
      const bool index3 = IsIndexed(m_VtxDesc.low.Normal) && m_VtxAttr.g0.NormalIndex3;
      const int elem_size = GetElementSize(m_VtxAttr.g0.NormalFormat);
      const int load_bytes = elem_size * 3;

      // Tangent
      // If in Index3 mode, and indexed components are used, replace the index with a new index.
      if (index3)
        data = GetVertexAddr(CPArray::Normal, m_VtxDesc.low.Normal);
      // The tangent comes after the normal; even in index3 mode, this offset is applied.
      // Note that this is different from adding 1 to the index, as the stride for indices may be
      // different from the size of the tangent itself.
      data.AddMemOffset(load_bytes);

      ReadVertex(data, m_VtxDesc.low.Normal, m_VtxAttr.g0.NormalFormat, 3, 3, true,
                 scaling_exponent, &m_native_vtx_decl.normals[1]);

      // Undo the offset above so that data points to the normal instead of the tangent.
      // This way, we can add 2*elem_size below to always point to the binormal, even if we replace
      // data with a new index (which would point to the normal).
      data.AddMemOffset(-load_bytes);

      // Binormal
      if (index3)
        data = GetVertexAddr(CPArray::Normal, m_VtxDesc.low.Normal);
      data.AddMemOffset(load_bytes * 2);

      ReadVertex(data, m_VtxDesc.low.Normal, m_VtxAttr.g0.NormalFormat, 3, 3, true,
                 scaling_exponent, &m_native_vtx_decl.normals[2]);
    }
  }

  for (u8 i = 0; i < m_VtxDesc.low.Color.Size(); i++)
  {
    if (m_VtxDesc.low.Color[i] != VertexComponentFormat::NotPresent)
    {
      data = GetVertexAddr(CPArray::Color0 + i, m_VtxDesc.low.Color[i]);
      ReadColor(data, m_VtxDesc.low.Color[i], m_VtxAttr.GetColorFormat(i));
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
    int elements = m_VtxAttr.GetTexElements(i) == TexComponentCount::ST ? 2 : 1;
    if (m_VtxDesc.high.TexCoord[i] != VertexComponentFormat::NotPresent)
    {
      data = GetVertexAddr(CPArray::TexCoord0 + i, m_VtxDesc.high.TexCoord[i]);
      u8 scaling_exponent = m_VtxAttr.GetTexFrac(i);
      ReadVertex(data, m_VtxDesc.high.TexCoord[i], m_VtxAttr.GetTexFormat(i), elements,
                 m_VtxDesc.low.TexMatIdx[i] ? 2 : elements, m_VtxAttr.g0.ByteDequant,
                 scaling_exponent, &m_native_vtx_decl.texcoords[i]);
    }
    if (m_VtxDesc.low.TexMatIdx[i])
    {
      m_native_vtx_decl.texcoords[i].components = 3;
      m_native_vtx_decl.texcoords[i].enable = true;
      m_native_vtx_decl.texcoords[i].type = ComponentFormat::Float;
      m_native_vtx_decl.texcoords[i].integer = false;
      MOVZX(64, 8, scratch1, MDisp(src_reg, texmatidx_ofs[i]));
      if (m_VtxDesc.high.TexCoord[i] != VertexComponentFormat::NotPresent)
      {
        CVTSI2SS(XMM0, R(scratch1));
        MOVSS(MDisp(dst_reg, m_dst_ofs), XMM0);
        m_dst_ofs += sizeof(float);
      }
      else
      {
        m_native_vtx_decl.texcoords[i].offset = m_dst_ofs;
        PXOR(XMM0, R(XMM0));
        CVTSI2SS(XMM0, R(scratch1));
        SHUFPS(XMM0, R(XMM0), 0x45);  // 000X -> 0X00
        MOVUPS(MDisp(dst_reg, m_dst_ofs), XMM0);
        m_dst_ofs += sizeof(float) * 3;
      }
    }
  }

  // Prepare for the next vertex.
  ADD(64, R(dst_reg), Imm32(m_dst_ofs));
  const u8* cont = GetCodePtr();
  ADD(64, R(src_reg), Imm32(m_src_ofs));

  SUB(32, R(remaining_reg), Imm8(1));
  J_CC(CC_AE, loop_start);

  // Get the original count.
  POP(32, R(ABI_RETURN));

  ABI_PopRegistersAndAdjustStack(regs, 0);

  if (IsIndexed(m_VtxDesc.low.Position))
  {
    SUB(32, R(ABI_RETURN), R(skipped_reg));
    RET();

    SetJumpTarget(m_skip_vertex);
    ADD(32, R(skipped_reg), Imm8(1));
    JMP(cont);
  }
  else
  {
    RET();
  }

  ASSERT_MSG(VIDEO, m_vertex_size == m_src_ofs,
             "Vertex size from vertex loader ({}) does not match expected vertex size ({})!\nVtx "
             "desc: {:08x} {:08x}\nVtx attr: {:08x} {:08x} {:08x}",
             m_src_ofs, m_vertex_size, m_VtxDesc.low.Hex, m_VtxDesc.high.Hex, m_VtxAttr.g0.Hex,
             m_VtxAttr.g1.Hex, m_VtxAttr.g2.Hex);
  m_native_vtx_decl.stride = m_dst_ofs;
}

int VertexLoaderX64::RunVertices(const u8* src, u8* dst, int count)
{
  m_numLoadedVertices += count;
  return ((int (*)(const u8* src, u8* dst, int count, const void* base))region)(src, dst, count,
                                                                                memory_base_ptr);
}
