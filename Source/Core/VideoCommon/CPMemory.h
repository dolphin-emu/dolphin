// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <type_traits>
#include <utility>

#include "Common/BitField.h"
#include "Common/BitFieldView.h"
#include "Common/BitSet.h"
#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"
#include "Common/EnumMap.h"
#include "Common/MsgHandler.h"

enum
{
  // These commands use the high nybble for the command itself, and the lower nybble is an argument.
  // TODO: However, Dolphin's implementation (in LoadCPReg) and YAGCD disagree about what values are
  // valid for the lower nybble.

  // YAGCD mentions 0x20 as "?", and does not mention the others
  // Libogc has 0x00 and 0x20, where 0x00 is tied to GX_ClearVCacheMetric and 0x20 related to
  // cpPerfMode. 0x10 may be GX_SetVCacheMetric, but that function is empty. In any case, these all
  // are probably for perf queries, and no title seems to actually need a full implementation.
  UNKNOWN_00 = 0x00,
  UNKNOWN_10 = 0x10,
  UNKNOWN_20 = 0x20,
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
enum class CPArray : u8
{
  Position = 0,
  Normal = 1,

  Color0 = 2,
  Color1 = 3,

  TexCoord0 = 4,
  TexCoord1 = 5,
  TexCoord2 = 6,
  TexCoord3 = 7,
  TexCoord4 = 8,
  TexCoord5 = 9,
  TexCoord6 = 10,
  TexCoord7 = 11,

  XF_A = 12,  // Usually used for position matrices
  XF_B = 13,  // Usually used for normal matrices
  XF_C = 14,  // Usually used for tex coord matrices
  XF_D = 15,  // Usually used for light objects
};
template <>
struct fmt::formatter<CPArray> : EnumFormatter<CPArray::XF_D>
{
  static constexpr array_type names = {"Position",    "Normal",      "Color 0",     "Color 1",
                                       "Tex Coord 0", "Tex Coord 1", "Tex Coord 2", "Tex Coord 3",
                                       "Tex Coord 4", "Tex Coord 5", "Tex Coord 6", "Tex Coord 7",
                                       "XF A",        "XF B",        "XF C",        "XF D"};
  constexpr formatter() : EnumFormatter(names) {}
};
// Intended for offsetting from Color0/TexCoord0
constexpr CPArray operator+(CPArray array, u8 offset)
{
  return static_cast<CPArray>(static_cast<u8>(array) + offset);
}

// Number of arrays related to vertex components (position, normal, color, tex coord)
// Excludes the 4 arrays used for indexed XF loads
constexpr u8 NUM_VERTEX_COMPONENT_ARRAYS = 12;

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
  constexpr formatter() : EnumFormatter({"Not present", "Direct", "8-bit index", "16-bit index"}) {}
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
  static constexpr array_type names = {"Unsigned Byte", "Byte", "Unsigned Short", "Short", "Float"};
  constexpr formatter() : EnumFormatter(names) {}
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

enum class CoordComponentCount : bool
{
  XY = 0,
  XYZ = 1,
};
template <>
struct fmt::formatter<CoordComponentCount> : EnumFormatter<CoordComponentCount::XYZ>
{
  constexpr formatter() : EnumFormatter({"2 (x, y)", "3 (x, y, z)"}) {}
};

enum class NormalComponentCount : bool
{
  N = 0,
  NTB = 1,
};
template <>
struct fmt::formatter<NormalComponentCount> : EnumFormatter<NormalComponentCount::NTB>
{
  constexpr formatter() : EnumFormatter({"1 (normal)", "3 (normal, tangent, binormal)"}) {}
};

enum class ColorComponentCount : bool
{
  RGB = 0,
  RGBA = 1,
};
template <>
struct fmt::formatter<ColorComponentCount> : EnumFormatter<ColorComponentCount::RGBA>
{
  constexpr formatter() : EnumFormatter({"3 (r, g, b)", "4 (r, g, b, a)"}) {}
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
  constexpr formatter() : EnumFormatter(names) {}
};

enum class TexComponentCount : bool
{
  S = 0,
  ST = 1,
};
template <>
struct fmt::formatter<TexComponentCount> : EnumFormatter<TexComponentCount::ST>
{
  constexpr formatter() : EnumFormatter({"1 (s)", "2 (s, t)"}) {}
};

struct TVtxDesc
{
  struct Low
  {
    // false: not present
    // true: present
    BFVIEW(bool, 1, 0, PosMatIdx)
    BFVIEW(bool, 1, 1, Tex0MatIdx)
    BFVIEW(bool, 1, 2, Tex1MatIdx)
    BFVIEW(bool, 1, 3, Tex2MatIdx)
    BFVIEW(bool, 1, 4, Tex3MatIdx)
    BFVIEW(bool, 1, 5, Tex4MatIdx)
    BFVIEW(bool, 1, 6, Tex5MatIdx)
    BFVIEW(bool, 1, 7, Tex6MatIdx)
    BFVIEW(bool, 1, 8, Tex7MatIdx)
    BFVIEW(bool, 1, 1, TexMatIdx, 8)  // 8x1 bit

    BFVIEW(VertexComponentFormat, 2, 9, Position)
    BFVIEW(VertexComponentFormat, 2, 11, Normal)
    BFVIEW(VertexComponentFormat, 2, 13, Color0)
    BFVIEW(VertexComponentFormat, 2, 15, Color1)
    BFVIEW(VertexComponentFormat, 2, 13, Color, 2)  // 2x2 bits

    u32 Hex = 0;
  };
  struct High
  {
    BFVIEW(VertexComponentFormat, 2, 0, Tex0Coord)
    BFVIEW(VertexComponentFormat, 2, 2, Tex1Coord)
    BFVIEW(VertexComponentFormat, 2, 4, Tex2Coord)
    BFVIEW(VertexComponentFormat, 2, 6, Tex3Coord)
    BFVIEW(VertexComponentFormat, 2, 8, Tex4Coord)
    BFVIEW(VertexComponentFormat, 2, 10, Tex5Coord)
    BFVIEW(VertexComponentFormat, 2, 12, Tex6Coord)
    BFVIEW(VertexComponentFormat, 2, 14, Tex7Coord)
    BFVIEW(VertexComponentFormat, 2, 0, TexCoord, 8)  // 8x2 bits

    u32 Hex = 0;
  };

  Low low;
  High high;
};
template <>
struct fmt::formatter<TVtxDesc::Low>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TVtxDesc::Low& desc, FormatContext& ctx) const
  {
    static constexpr std::array<const char*, 2> present = {"Not present", "Present"};

    return fmt::format_to(
        ctx.out(),
        "Position and normal matrix index: {}\n"
        "Texture Coord 0 matrix index: {}\n"
        "Texture Coord 1 matrix index: {}\n"
        "Texture Coord 2 matrix index: {}\n"
        "Texture Coord 3 matrix index: {}\n"
        "Texture Coord 4 matrix index: {}\n"
        "Texture Coord 5 matrix index: {}\n"
        "Texture Coord 6 matrix index: {}\n"
        "Texture Coord 7 matrix index: {}\n"
        "Position: {}\n"
        "Normal: {}\n"
        "Color 0: {}\n"
        "Color 1: {}",
        present[desc.PosMatIdx()], present[desc.Tex0MatIdx()], present[desc.Tex1MatIdx()],
        present[desc.Tex2MatIdx()], present[desc.Tex3MatIdx()], present[desc.Tex4MatIdx()],
        present[desc.Tex5MatIdx()], present[desc.Tex6MatIdx()], present[desc.Tex7MatIdx()],
        desc.Position(), desc.Normal(), desc.Color0(), desc.Color1());
  }
};
template <>
struct fmt::formatter<TVtxDesc::High>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TVtxDesc::High& desc, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Texture Coord 0: {}\n"
                          "Texture Coord 1: {}\n"
                          "Texture Coord 2: {}\n"
                          "Texture Coord 3: {}\n"
                          "Texture Coord 4: {}\n"
                          "Texture Coord 5: {}\n"
                          "Texture Coord 6: {}\n"
                          "Texture Coord 7: {}",
                          desc.Tex0Coord(), desc.Tex1Coord(), desc.Tex2Coord(), desc.Tex3Coord(),
                          desc.Tex4Coord(), desc.Tex5Coord(), desc.Tex6Coord(), desc.Tex7Coord());
  }
};
template <>
struct fmt::formatter<TVtxDesc>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TVtxDesc& desc, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "{}\n{}", desc.low, desc.high);
  }
};

struct VAT_group0
{
  u32 Hex = 0;
  // 0:8
  BFVIEW(CoordComponentCount, 1, 0, PosElements)
  BFVIEW(ComponentFormat, 3, 1, PosFormat)
  BFVIEW(u32, 5, 4, PosFrac)
  // 9:12
  BFVIEW(NormalComponentCount, 1, 9, NormalElements)
  BFVIEW(ComponentFormat, 3, 10, NormalFormat)
  // 13:16
  BFVIEW(ColorComponentCount, 1, 13, Color0Elements)
  BFVIEW(ColorFormat, 3, 14, Color0Comp)
  // 17:20
  BFVIEW(ColorComponentCount, 1, 17, Color1Elements)
  BFVIEW(ColorFormat, 3, 18, Color1Comp)
  // 21:29
  BFVIEW(TexComponentCount, 1, 21, Tex0CoordElements)
  BFVIEW(ComponentFormat, 3, 22, Tex0CoordFormat)
  BFVIEW(u8, 5, 25, Tex0Frac)
  // 30:31
  BFVIEW(bool, 1, 30, ByteDequant)
  BFVIEW(bool, 1, 31, NormalIndex3)
};
template <>
struct fmt::formatter<VAT_group0>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const VAT_group0& g0, FormatContext& ctx) const
  {
    static constexpr std::array<const char*, 2> byte_dequant = {
        "shift does not apply to u8/s8 components", "shift applies to u8/s8 components"};
    static constexpr std::array<const char*, 2> normalindex3 = {
        "single index shared by normal, tangent, and binormal",
        "three indices, one each for normal, tangent, and binormal"};

    return fmt::format_to(ctx.out(),
                          "Position elements: {}\n"
                          "Position format: {}\n"
                          "Position shift: {} ({})\n"
                          "Normal elements: {}\n"
                          "Normal format: {}\n"
                          "Color 0 elements: {}\n"
                          "Color 0 format: {}\n"
                          "Color 1 elements: {}\n"
                          "Color 1 format: {}\n"
                          "Texture coord 0 elements: {}\n"
                          "Texture coord 0 format: {}\n"
                          "Texture coord 0 shift: {} ({})\n"
                          "Byte dequant: {}\n"
                          "Normal index 3: {}",
                          g0.PosElements(), g0.PosFormat(), g0.PosFrac(), 1.f / (1 << g0.PosFrac()),
                          g0.NormalElements(), g0.NormalFormat(), g0.Color0Elements(),
                          g0.Color0Comp(), g0.Color1Elements(), g0.Color1Comp(),
                          g0.Tex0CoordElements(), g0.Tex0CoordFormat(), g0.Tex0Frac(),
                          1.f / (1 << g0.Tex0Frac()), byte_dequant[g0.ByteDequant()],
                          normalindex3[g0.NormalIndex3()]);
  }
};

struct VAT_group1
{
  u32 Hex = 0;
  // 0:8
  BFVIEW(TexComponentCount, 1, 0, Tex1CoordElements)
  BFVIEW(ComponentFormat, 3, 1, Tex1CoordFormat)
  BFVIEW(u8, 5, 4, Tex1Frac)
  // 9:17
  BFVIEW(TexComponentCount, 1, 9, Tex2CoordElements)
  BFVIEW(ComponentFormat, 3, 10, Tex2CoordFormat)
  BFVIEW(u8, 5, 13, Tex2Frac)
  // 18:26
  BFVIEW(TexComponentCount, 1, 18, Tex3CoordElements)
  BFVIEW(ComponentFormat, 3, 19, Tex3CoordFormat)
  BFVIEW(u8, 5, 22, Tex3Frac)
  // 27:30
  BFVIEW(TexComponentCount, 1, 27, Tex4CoordElements)
  BFVIEW(ComponentFormat, 3, 28, Tex4CoordFormat)
  // 31
  BFVIEW(bool, 1, 31, VCacheEnhance)
};
template <>
struct fmt::formatter<VAT_group1>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const VAT_group1& g1, FormatContext& ctx) const
  {
    return fmt::format_to(
        ctx.out(),
        "Texture coord 1 elements: {}\n"
        "Texture coord 1 format: {}\n"
        "Texture coord 1 shift: {} ({})\n"
        "Texture coord 2 elements: {}\n"
        "Texture coord 2 format: {}\n"
        "Texture coord 2 shift: {} ({})\n"
        "Texture coord 3 elements: {}\n"
        "Texture coord 3 format: {}\n"
        "Texture coord 3 shift: {} ({})\n"
        "Texture coord 4 elements: {}\n"
        "Texture coord 4 format: {}\n"
        "Enhance VCache (must always be on): {}",
        g1.Tex1CoordElements(), g1.Tex1CoordFormat(), g1.Tex1Frac(), 1.f / (1 << g1.Tex1Frac()),
        g1.Tex2CoordElements(), g1.Tex2CoordFormat(), g1.Tex2Frac(), 1.f / (1 << g1.Tex2Frac()),
        g1.Tex3CoordElements(), g1.Tex3CoordFormat(), g1.Tex3Frac(), 1.f / (1 << g1.Tex3Frac()),
        g1.Tex4CoordElements(), g1.Tex4CoordFormat(), g1.VCacheEnhance() ? "Yes" : "No");
  }
};

struct VAT_group2
{
  u32 Hex = 0;
  // 0:4
  BFVIEW(u8, 5, 0, Tex4Frac)
  // 5:13
  BFVIEW(TexComponentCount, 1, 5, Tex5CoordElements)
  BFVIEW(ComponentFormat, 3, 6, Tex5CoordFormat)
  BFVIEW(u8, 5, 9, Tex5Frac)
  // 14:22
  BFVIEW(TexComponentCount, 1, 14, Tex6CoordElements)
  BFVIEW(ComponentFormat, 3, 15, Tex6CoordFormat)
  BFVIEW(u8, 5, 18, Tex6Frac)
  // 23:31
  BFVIEW(TexComponentCount, 1, 23, Tex7CoordElements)
  BFVIEW(ComponentFormat, 3, 24, Tex7CoordFormat)
  BFVIEW(u8, 5, 27, Tex7Frac)
};
template <>
struct fmt::formatter<VAT_group2>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const VAT_group2& g2, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Texture coord 4 shift: {} ({})\n"
                          "Texture coord 5 elements: {}\n"
                          "Texture coord 5 format: {}\n"
                          "Texture coord 5 shift: {} ({})\n"
                          "Texture coord 6 elements: {}\n"
                          "Texture coord 6 format: {}\n"
                          "Texture coord 6 shift: {} ({})\n"
                          "Texture coord 7 elements: {}\n"
                          "Texture coord 7 format: {}\n"
                          "Texture coord 7 shift: {} ({})",
                          g2.Tex4Frac(), 1.f / (1 << g2.Tex4Frac()), g2.Tex5CoordElements(),
                          g2.Tex5CoordFormat(), g2.Tex5Frac(), 1.f / (1 << g2.Tex5Frac()),
                          g2.Tex6CoordElements(), g2.Tex6CoordFormat(), g2.Tex6Frac(),
                          1.f / (1 << g2.Tex6Frac()), g2.Tex7CoordElements(), g2.Tex7CoordFormat(),
                          g2.Tex7Frac(), 1.f / (1 << g2.Tex7Frac()));
  }
};

struct VAT
{
  VAT_group0 g0;
  VAT_group1 g1;
  VAT_group2 g2;

  constexpr ColorComponentCount GetColorElements(size_t idx) const
  {
    switch (idx)
    {
    case 0:
      return g0.Color0Elements();
    case 1:
      return g0.Color1Elements();
    default:
      PanicAlertFmt("Invalid color index {}", idx);
      return ColorComponentCount::RGB;
    }
  }
  constexpr ColorFormat GetColorFormat(size_t idx) const
  {
    switch (idx)
    {
    case 0:
      return g0.Color0Comp();
    case 1:
      return g0.Color1Comp();
    default:
      PanicAlertFmt("Invalid color index {}", idx);
      return ColorFormat::RGB565;
    }
  }
  constexpr TexComponentCount GetTexElements(size_t idx) const
  {
    switch (idx)
    {
    case 0:
      return g0.Tex0CoordElements();
    case 1:
      return g1.Tex1CoordElements();
    case 2:
      return g1.Tex2CoordElements();
    case 3:
      return g1.Tex3CoordElements();
    case 4:
      return g1.Tex4CoordElements();
    case 5:
      return g2.Tex5CoordElements();
    case 6:
      return g2.Tex6CoordElements();
    case 7:
      return g2.Tex7CoordElements();
    default:
      PanicAlertFmt("Invalid tex coord index {}", idx);
      return TexComponentCount::S;
    }
  }
  constexpr ComponentFormat GetTexFormat(size_t idx) const
  {
    switch (idx)
    {
    case 0:
      return g0.Tex0CoordFormat();
    case 1:
      return g1.Tex1CoordFormat();
    case 2:
      return g1.Tex2CoordFormat();
    case 3:
      return g1.Tex3CoordFormat();
    case 4:
      return g1.Tex4CoordFormat();
    case 5:
      return g2.Tex5CoordFormat();
    case 6:
      return g2.Tex6CoordFormat();
    case 7:
      return g2.Tex7CoordFormat();
    default:
      PanicAlertFmt("Invalid tex coord index {}", idx);
      return ComponentFormat::UByte;
    }
  }
  constexpr u8 GetTexFrac(size_t idx) const
  {
    switch (idx)
    {
    case 0:
      return g0.Tex0Frac();
    case 1:
      return g1.Tex1Frac();
    case 2:
      return g1.Tex2Frac();
    case 3:
      return g1.Tex3Frac();
    case 4:
      return g2.Tex4Frac();
    case 5:
      return g2.Tex5Frac();
    case 6:
      return g2.Tex6Frac();
    case 7:
      return g2.Tex7Frac();
    default:
      PanicAlertFmt("Invalid tex coord index {}", idx);
      return 0;
    }
  }
};
template <>
struct fmt::formatter<VAT>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const VAT& vat, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "{}\n{}\n{}", vat.g0, vat.g1, vat.g2);
  }
};

// Matrix indices
struct TMatrixIndexA
{
  BFVIEW(u32, 6, 0, PosNormalMtxIdx)
  BFVIEW(u32, 6, 6, Tex0MtxIdx)
  BFVIEW(u32, 6, 12, Tex1MtxIdx)
  BFVIEW(u32, 6, 18, Tex2MtxIdx)
  BFVIEW(u32, 6, 24, Tex3MtxIdx)
  u32 Hex;
};
template <>
struct fmt::formatter<TMatrixIndexA>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TMatrixIndexA& m, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "PosNormal: {}\nTex0: {}\nTex1: {}\nTex2: {}\nTex3: {}",
                          m.PosNormalMtxIdx(), m.Tex0MtxIdx(), m.Tex1MtxIdx(), m.Tex2MtxIdx(),
                          m.Tex3MtxIdx());
  }
};

struct TMatrixIndexB
{
  BFVIEW(u32, 6, 0, Tex4MtxIdx)
  BFVIEW(u32, 6, 6, Tex5MtxIdx)
  BFVIEW(u32, 6, 12, Tex6MtxIdx)
  BFVIEW(u32, 6, 18, Tex7MtxIdx)
  u32 Hex;
};
template <>
struct fmt::formatter<TMatrixIndexB>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TMatrixIndexB& m, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Tex4: {}\nTex5: {}\nTex6: {}\nTex7: {}", m.Tex4MtxIdx(),
                          m.Tex5MtxIdx(), m.Tex6MtxIdx(), m.Tex7MtxIdx());
  }
};

class VertexLoaderBase;

// STATE_TO_SAVE
struct CPState final
{
  CPState() = default;
  explicit CPState(const u32* memory);

  // Mutates the CP state based on the given command and value.
  void LoadCPReg(u8 sub_cmd, u32 value);
  // Fills memory with data from CP regs.  There should be space for 0x100 values in memory.
  void FillCPMemoryArray(u32* memory) const;

  Common::EnumMap<u32, CPArray::XF_D> array_bases;
  Common::EnumMap<u32, CPArray::XF_D> array_strides;
  TMatrixIndexA matrix_index_a{};
  TMatrixIndexB matrix_index_b{};
  TVtxDesc vtx_desc;
  // Most games only use the first VtxAttr and simply reconfigure it all the time as needed.
  std::array<VAT, CP_NUM_VAT_REG> vtx_attr{};
};
static_assert(std::is_trivially_copyable_v<CPState>);

class PointerWrap;

extern CPState g_main_cp_state;
extern CPState g_preprocess_cp_state;

void DoCPState(PointerWrap& p);

void CopyPreprocessCPStateFromMain();

std::pair<std::string, std::string> GetCPRegInfo(u8 cmd, u32 value);
