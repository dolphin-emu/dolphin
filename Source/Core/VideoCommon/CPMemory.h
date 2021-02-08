// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/BitField.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"
#include "Common/MsgHandler.h"

enum
{
  // These commands use the high nybble for the command itself, and the lower nybble is an argument.
  // TODO: However, Dolphin's implementation (in LoadCPReg) and YAGCD disagree about what values are
  // valid for the lower nybble.

  // YAGCD says 0x30 only; LoadCPReg allows any
  MATINDEX_A = 0x30,
  // YAGCD says 0x40 only; LoadCPReg allows any
  MATINDEX_B = 0x40,
  // YAGCD says 0x50-0x57 for distinct VCDs; LoadCPReg allows any for a single VCD
  VCD_LO = 0x50,
  // YAGCD says 0x60-0x67 for distinct VCDs; LoadCPReg allows any for a single VCD
  VCD_HI = 0x60,
  // YAGCD and LoadCPReg both agree that only 0x70-0x77 are valid
  CP_VAT_REG_A = 0x70,
  // YAGCD and LoadCPReg both agree that only 0x80-0x87 are valid
  CP_VAT_REG_B = 0x80,
  // YAGCD and LoadCPReg both agree that only 0x90-0x97 are valid
  CP_VAT_REG_C = 0x90,
  // YAGCD and LoadCPReg agree that 0xa0-0xaf are valid
  ARRAY_BASE = 0xa0,
  // YAGCD and LoadCPReg agree that 0xb0-0xbf are valid
  ARRAY_STRIDE = 0xb0,

  CP_COMMAND_MASK = 0xf0,
  CP_NUM_VAT_REG = 0x08,
  CP_VAT_MASK = 0x07,
  CP_NUM_ARRAYS = 0x10,
  CP_ARRAY_MASK = 0x0f,
};

// Vertex array numbers
enum
{
  ARRAY_POSITION = 0,
  ARRAY_NORMAL = 1,
  ARRAY_COLOR = 2,
  ARRAY_COLOR2 = 3,
  ARRAY_TEXCOORD0 = 4,
};

// Vertex components
enum class VertexComponentFormat
{
  NotPresent = 0,
  Direct = 1,
  Index8 = 2,
  Index16 = 3,
};
template <>
struct fmt::formatter<VertexComponentFormat> : EnumFormatter<VertexComponentFormat::Index16>
{
  formatter() : EnumFormatter({"Not present", "Direct", "8-bit index", "16-bit index"}) {}
};

constexpr bool IsIndexed(VertexComponentFormat format)
{
  return format == VertexComponentFormat::Index8 || format == VertexComponentFormat::Index16;
}

enum class ComponentFormat
{
  UByte = 0,  // Invalid for normals
  Byte = 1,
  UShort = 2,  // Invalid for normals
  Short = 3,
  Float = 4,
};
template <>
struct fmt::formatter<ComponentFormat> : EnumFormatter<ComponentFormat::Float>
{
  formatter() : EnumFormatter({"Unsigned Byte", "Byte", "Unsigned Short", "Short", "Float"}) {}
};

constexpr u32 GetElementSize(ComponentFormat format)
{
  switch (format)
  {
  case ComponentFormat::UByte:
  case ComponentFormat::Byte:
    return 1;
  case ComponentFormat::UShort:
  case ComponentFormat::Short:
    return 2;
  case ComponentFormat::Float:
    return 4;
  default:
    PanicAlertFmt("Unknown format {}", format);
    return 0;
  }
}

enum class CoordComponentCount
{
  XY = 0,
  XYZ = 1,
};
template <>
struct fmt::formatter<CoordComponentCount> : EnumFormatter<CoordComponentCount::XYZ>
{
  formatter() : EnumFormatter({"2 (x, y)", "3 (x, y, z)"}) {}
};

enum class NormalComponentCount
{
  N = 0,
  NBT = 1,
};
template <>
struct fmt::formatter<NormalComponentCount> : EnumFormatter<NormalComponentCount::NBT>
{
  formatter() : EnumFormatter({"1 (n)", "3 (n, b, t)"}) {}
};

enum class ColorComponentCount
{
  RGB = 0,
  RGBA = 1,
};
template <>
struct fmt::formatter<ColorComponentCount> : EnumFormatter<ColorComponentCount::RGBA>
{
  formatter() : EnumFormatter({"3 (r, g, b)", "4 (r, g, b, a)"}) {}
};

enum class ColorFormat
{
  RGB565 = 0,    // 16b
  RGB888 = 1,    // 24b
  RGB888x = 2,   // 32b
  RGBA4444 = 3,  // 16b
  RGBA6666 = 4,  // 24b
  RGBA8888 = 5,  // 32b
};
template <>
struct fmt::formatter<ColorFormat> : EnumFormatter<ColorFormat::RGBA8888>
{
  static constexpr array_type names = {
      "RGB 16 bits 565",   "RGB 24 bits 888",   "RGB 32 bits 888x",
      "RGBA 16 bits 4444", "RGBA 24 bits 6666", "RGBA 32 bits 8888",
  };
  formatter() : EnumFormatter(names) {}
};

enum class TexComponentCount
{
  S = 0,
  ST = 1,
};
template <>
struct fmt::formatter<TexComponentCount> : EnumFormatter<TexComponentCount::ST>
{
  formatter() : EnumFormatter({"1 (s)", "2 (s, t)"}) {}
};

struct TVtxDesc
{
  union Low
  {
    // false: not present
    // true: present
    BitField<0, 1, bool, u32> PosMatIdx;
    BitField<1, 1, bool, u32> Tex0MatIdx;
    BitField<2, 1, bool, u32> Tex1MatIdx;
    BitField<3, 1, bool, u32> Tex2MatIdx;
    BitField<4, 1, bool, u32> Tex3MatIdx;
    BitField<5, 1, bool, u32> Tex4MatIdx;
    BitField<6, 1, bool, u32> Tex5MatIdx;
    BitField<7, 1, bool, u32> Tex6MatIdx;
    BitField<8, 1, bool, u32> Tex7MatIdx;
    BitFieldArray<1, 1, 8, bool, u32> TexMatIdx;

    BitField<9, 2, VertexComponentFormat> Position;
    BitField<11, 2, VertexComponentFormat> Normal;
    BitField<13, 2, VertexComponentFormat> Color0;
    BitField<15, 2, VertexComponentFormat> Color1;
    BitFieldArray<13, 2, 2, VertexComponentFormat> Color;

    u32 Hex;
  };
  union High
  {
    BitField<0, 2, VertexComponentFormat> Tex0Coord;
    BitField<2, 2, VertexComponentFormat> Tex1Coord;
    BitField<4, 2, VertexComponentFormat> Tex2Coord;
    BitField<6, 2, VertexComponentFormat> Tex3Coord;
    BitField<8, 2, VertexComponentFormat> Tex4Coord;
    BitField<10, 2, VertexComponentFormat> Tex5Coord;
    BitField<12, 2, VertexComponentFormat> Tex6Coord;
    BitField<14, 2, VertexComponentFormat> Tex7Coord;
    BitFieldArray<0, 2, 8, VertexComponentFormat> TexCoord;

    u32 Hex;
  };

  Low low;
  High high;

  // This structure was originally packed into bits 0..32, using 33 total bits.
  // The actual format has 17 bits in the low one and 16 bits in the high one,
  // but the old format is still supported for compatibility.
  u64 GetLegacyHex() const { return (low.Hex & 0x1FFFF) | (u64(high.Hex) << 17); }
  u32 GetLegacyHex0() const { return static_cast<u32>(GetLegacyHex()); }
  // Only *1* bit is used in this
  u32 GetLegacyHex1() const { return static_cast<u32>(GetLegacyHex() >> 32); }
  void SetLegacyHex(u64 value)
  {
    low.Hex = value & 0x1FFFF;
    high.Hex = value >> 17;
  }
};

union UVAT_group0
{
  u32 Hex;
  // 0:8
  BitField<0, 1, CoordComponentCount> PosElements;
  BitField<1, 3, ComponentFormat> PosFormat;
  BitField<4, 5, u32> PosFrac;
  // 9:12
  BitField<9, 1, NormalComponentCount> NormalElements;
  BitField<10, 3, ComponentFormat> NormalFormat;
  // 13:16
  BitField<13, 1, ColorComponentCount> Color0Elements;
  BitField<14, 3, ColorFormat> Color0Comp;
  // 17:20
  BitField<17, 1, ColorComponentCount> Color1Elements;
  BitField<18, 3, ColorFormat> Color1Comp;
  // 21:29
  BitField<21, 1, TexComponentCount> Tex0CoordElements;
  BitField<22, 3, ComponentFormat> Tex0CoordFormat;
  BitField<25, 5, u32> Tex0Frac;
  // 30:31
  BitField<30, 1, u32> ByteDequant;
  BitField<31, 1, u32> NormalIndex3;
};

union UVAT_group1
{
  u32 Hex;
  // 0:8
  BitField<0, 1, TexComponentCount> Tex1CoordElements;
  BitField<1, 3, ComponentFormat> Tex1CoordFormat;
  BitField<4, 5, u32> Tex1Frac;
  // 9:17
  BitField<9, 1, TexComponentCount> Tex2CoordElements;
  BitField<10, 3, ComponentFormat> Tex2CoordFormat;
  BitField<13, 5, u32> Tex2Frac;
  // 18:26
  BitField<18, 1, TexComponentCount> Tex3CoordElements;
  BitField<19, 3, ComponentFormat> Tex3CoordFormat;
  BitField<22, 5, u32> Tex3Frac;
  // 27:30
  BitField<27, 1, TexComponentCount> Tex4CoordElements;
  BitField<28, 3, ComponentFormat> Tex4CoordFormat;
  // 31
  BitField<31, 1, u32> VCacheEnhance;
};

union UVAT_group2
{
  u32 Hex;
  // 0:4
  BitField<0, 5, u32> Tex4Frac;
  // 5:13
  BitField<5, 1, TexComponentCount> Tex5CoordElements;
  BitField<6, 3, ComponentFormat> Tex5CoordFormat;
  BitField<9, 5, u32> Tex5Frac;
  // 14:22
  BitField<14, 1, TexComponentCount> Tex6CoordElements;
  BitField<15, 3, ComponentFormat> Tex6CoordFormat;
  BitField<18, 5, u32> Tex6Frac;
  // 23:31
  BitField<23, 1, TexComponentCount> Tex7CoordElements;
  BitField<24, 3, ComponentFormat> Tex7CoordFormat;
  BitField<27, 5, u32> Tex7Frac;
};

struct ColorAttr
{
  ColorComponentCount Elements;
  ColorFormat Comp;
};

struct TexAttr
{
  TexComponentCount Elements;
  ComponentFormat Format;
  u8 Frac;
};

struct TVtxAttr
{
  CoordComponentCount PosElements;
  ComponentFormat PosFormat;
  u8 PosFrac;
  NormalComponentCount NormalElements;
  ComponentFormat NormalFormat;
  ColorAttr color[2];
  TexAttr texCoord[8];
  bool ByteDequant;
  u8 NormalIndex3;
};

// Matrix indices
union TMatrixIndexA
{
  BitField<0, 6, u32> PosNormalMtxIdx;
  BitField<6, 6, u32> Tex0MtxIdx;
  BitField<12, 6, u32> Tex1MtxIdx;
  BitField<18, 6, u32> Tex2MtxIdx;
  BitField<24, 6, u32> Tex3MtxIdx;
  u32 Hex;
};

union TMatrixIndexB
{
  BitField<0, 6, u32> Tex4MtxIdx;
  BitField<6, 6, u32> Tex5MtxIdx;
  BitField<12, 6, u32> Tex6MtxIdx;
  BitField<18, 6, u32> Tex7MtxIdx;
  u32 Hex;
};

struct VAT
{
  UVAT_group0 g0;
  UVAT_group1 g1;
  UVAT_group2 g2;
};

class VertexLoaderBase;

// STATE_TO_SAVE
struct CPState final
{
  u32 array_bases[CP_NUM_ARRAYS];
  u32 array_strides[CP_NUM_ARRAYS];
  TMatrixIndexA matrix_index_a;
  TMatrixIndexB matrix_index_b;
  TVtxDesc vtx_desc;
  // Most games only use the first VtxAttr and simply reconfigure it all the time as needed.
  VAT vtx_attr[CP_NUM_VAT_REG];

  // Attributes that actually belong to VertexLoaderManager:
  BitSet32 attr_dirty;
  bool bases_dirty;
  VertexLoaderBase* vertex_loaders[CP_NUM_VAT_REG];
  int last_id;
};

class PointerWrap;

extern CPState g_main_cp_state;
extern CPState g_preprocess_cp_state;

// Might move this into its own file later.
void LoadCPReg(u32 SubCmd, u32 Value, bool is_preprocess = false);

// Fills memory with data from CP regs
void FillCPMemoryArray(u32* memory);

void DoCPState(PointerWrap& p);

void CopyPreprocessCPStateFromMain();
