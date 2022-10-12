// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <utility>

#include "Common/BitField.h"
#include "Common/BitFieldView.h"
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"
#include "Common/EnumMap.h"
#include "Common/Inline.h"

// X.h defines None to be 0 and Always to be 2, which causes problems with some of the enums
#undef None
#undef Always

enum class TextureFormat;
enum class EFBCopyFormat;
enum class TLUTFormat;

#pragma pack(4)

enum
{
  BPMEM_GENMODE = 0x00,
  BPMEM_DISPLAYCOPYFILTER = 0x01,  // 0x01 + 4
  BPMEM_IND_MTXA = 0x06,           // 0x06 + (3 * 3)
  BPMEM_IND_MTXB = 0x07,           // 0x07 + (3 * 3)
  BPMEM_IND_MTXC = 0x08,           // 0x08 + (3 * 3)
  BPMEM_IND_IMASK = 0x0F,
  BPMEM_IND_CMD = 0x10,  // 0x10 + 16
  BPMEM_SCISSORTL = 0x20,
  BPMEM_SCISSORBR = 0x21,
  BPMEM_LINEPTWIDTH = 0x22,
  BPMEM_PERF0_TRI = 0x23,
  BPMEM_PERF0_QUAD = 0x24,
  BPMEM_RAS1_SS0 = 0x25,
  BPMEM_RAS1_SS1 = 0x26,
  BPMEM_IREF = 0x27,
  BPMEM_TREF = 0x28,      // 0x28 + 8
  BPMEM_SU_SSIZE = 0x30,  // 0x30 + (2 * 8)
  BPMEM_SU_TSIZE = 0x31,  // 0x31 + (2 * 8)
  BPMEM_ZMODE = 0x40,
  BPMEM_BLENDMODE = 0x41,
  BPMEM_CONSTANTALPHA = 0x42,
  BPMEM_ZCOMPARE = 0x43,
  BPMEM_FIELDMASK = 0x44,
  BPMEM_SETDRAWDONE = 0x45,
  BPMEM_BUSCLOCK0 = 0x46,
  BPMEM_PE_TOKEN_ID = 0x47,
  BPMEM_PE_TOKEN_INT_ID = 0x48,
  BPMEM_EFB_TL = 0x49,
  BPMEM_EFB_WH = 0x4A,
  BPMEM_EFB_ADDR = 0x4B,
  BPMEM_MIPMAP_STRIDE = 0x4D,
  BPMEM_COPYYSCALE = 0x4E,
  BPMEM_CLEAR_AR = 0x4F,
  BPMEM_CLEAR_GB = 0x50,
  BPMEM_CLEAR_Z = 0x51,
  BPMEM_TRIGGER_EFB_COPY = 0x52,
  BPMEM_COPYFILTER0 = 0x53,
  BPMEM_COPYFILTER1 = 0x54,
  BPMEM_CLEARBBOX1 = 0x55,
  BPMEM_CLEARBBOX2 = 0x56,
  BPMEM_CLEAR_PIXEL_PERF = 0x57,
  BPMEM_REVBITS = 0x58,
  BPMEM_SCISSOROFFSET = 0x59,
  BPMEM_PRELOAD_ADDR = 0x60,
  BPMEM_PRELOAD_TMEMEVEN = 0x61,
  BPMEM_PRELOAD_TMEMODD = 0x62,
  BPMEM_PRELOAD_MODE = 0x63,
  BPMEM_LOADTLUT0 = 0x64,
  BPMEM_LOADTLUT1 = 0x65,
  BPMEM_TEXINVALIDATE = 0x66,
  BPMEM_PERF1 = 0x67,
  BPMEM_FIELDMODE = 0x68,
  BPMEM_BUSCLOCK1 = 0x69,
  BPMEM_TX_SETMODE0 = 0x80,     // 0x80 + 4
  BPMEM_TX_SETMODE1 = 0x84,     // 0x84 + 4
  BPMEM_TX_SETIMAGE0 = 0x88,    // 0x88 + 4
  BPMEM_TX_SETIMAGE1 = 0x8C,    // 0x8C + 4
  BPMEM_TX_SETIMAGE2 = 0x90,    // 0x90 + 4
  BPMEM_TX_SETIMAGE3 = 0x94,    // 0x94 + 4
  BPMEM_TX_SETTLUT = 0x98,      // 0x98 + 4
  BPMEM_TX_SETMODE0_4 = 0xA0,   // 0xA0 + 4
  BPMEM_TX_SETMODE1_4 = 0xA4,   // 0xA4 + 4
  BPMEM_TX_SETIMAGE0_4 = 0xA8,  // 0xA8 + 4
  BPMEM_TX_SETIMAGE1_4 = 0xAC,  // 0xA4 + 4
  BPMEM_TX_SETIMAGE2_4 = 0xB0,  // 0xB0 + 4
  BPMEM_TX_SETIMAGE3_4 = 0xB4,  // 0xB4 + 4
  BPMEM_TX_SETTLUT_4 = 0xB8,    // 0xB8 + 4
  BPMEM_TEV_COLOR_ENV = 0xC0,   // 0xC0 + (2 * 16)
  BPMEM_TEV_ALPHA_ENV = 0xC1,   // 0xC1 + (2 * 16)
  BPMEM_TEV_COLOR_RA = 0xE0,    // 0xE0 + (2 * 4)
  BPMEM_TEV_COLOR_BG = 0xE1,    // 0xE1 + (2 * 4)
  BPMEM_FOGRANGE = 0xE8,        // 0xE8 + 6
  BPMEM_FOGPARAM0 = 0xEE,
  BPMEM_FOGBMAGNITUDE = 0xEF,
  BPMEM_FOGBEXPONENT = 0xF0,
  BPMEM_FOGPARAM3 = 0xF1,
  BPMEM_FOGCOLOR = 0xF2,
  BPMEM_ALPHACOMPARE = 0xF3,
  BPMEM_BIAS = 0xF4,
  BPMEM_ZTEX2 = 0xF5,
  BPMEM_TEV_KSEL = 0xF6,  // 0xF6 + 8
  BPMEM_BP_MASK = 0xFE,
};

// Tev/combiner things

// TEV scaling type
enum class TevScale : u32
{
  Scale1 = 0,
  Scale2 = 1,
  Scale4 = 2,
  Divide2 = 3
};
template <>
struct fmt::formatter<TevScale> : EnumFormatter<TevScale::Divide2>
{
  constexpr formatter() : EnumFormatter({"1", "2", "4", "0.5"}) {}
};

// TEV combiner operator
enum class TevOp : u32
{
  Add = 0,
  Sub = 1,
};
template <>
struct fmt::formatter<TevOp> : EnumFormatter<TevOp::Sub>
{
  constexpr formatter() : EnumFormatter({"Add", "Subtract"}) {}
};

enum class TevCompareMode : u32
{
  R8 = 0,
  GR16 = 1,
  BGR24 = 2,
  RGB8 = 3,
  A8 = RGB8,
};
template <>
struct fmt::formatter<TevCompareMode> : EnumFormatter<TevCompareMode::RGB8>
{
  constexpr formatter() : EnumFormatter({"R8", "GR16", "BGR24", "RGB8 / A8"}) {}
};

enum class TevComparison : u32
{
  GT = 0,
  EQ = 1,
};
template <>
struct fmt::formatter<TevComparison> : EnumFormatter<TevComparison::EQ>
{
  constexpr formatter() : EnumFormatter({"Greater than", "Equal to"}) {}
};

// TEV color combiner input
enum class TevColorArg : u32
{
  PrevColor = 0,
  PrevAlpha = 1,
  Color0 = 2,
  Alpha0 = 3,
  Color1 = 4,
  Alpha1 = 5,
  Color2 = 6,
  Alpha2 = 7,
  TexColor = 8,
  TexAlpha = 9,
  RasColor = 10,
  RasAlpha = 11,
  One = 12,
  Half = 13,
  Konst = 14,
  Zero = 15
};
template <>
struct fmt::formatter<TevColorArg> : EnumFormatter<TevColorArg::Zero>
{
  static constexpr array_type names = {
      "prev.rgb", "prev.aaa", "c0.rgb",  "c0.aaa",  "c1.rgb", "c1.aaa", "c2.rgb",    "c2.aaa",
      "tex.rgb",  "tex.aaa",  "ras.rgb", "ras.aaa", "ONE",    "HALF",   "konst.rgb", "ZERO",
  };
  constexpr formatter() : EnumFormatter(names) {}
};

// TEV alpha combiner input
enum class TevAlphaArg : u32
{
  PrevAlpha = 0,
  Alpha0 = 1,
  Alpha1 = 2,
  Alpha2 = 3,
  TexAlpha = 4,
  RasAlpha = 5,
  Konst = 6,
  Zero = 7
};
template <>
struct fmt::formatter<TevAlphaArg> : EnumFormatter<TevAlphaArg::Zero>
{
  static constexpr array_type names = {
      "prev", "c0", "c1", "c2", "tex", "ras", "konst", "ZERO",
  };
  constexpr formatter() : EnumFormatter(names) {}
};

// TEV output registers
enum class TevOutput : u32
{
  Prev = 0,
  Color0 = 1,
  Color1 = 2,
  Color2 = 3,
};
template <>
struct fmt::formatter<TevOutput> : EnumFormatter<TevOutput::Color2>
{
  constexpr formatter() : EnumFormatter({"prev", "c0", "c1", "c2"}) {}
};

// Z-texture formats
enum class ZTexFormat : u32
{
  U8 = 0,
  U16 = 1,
  U24 = 2
};
template <>
struct fmt::formatter<ZTexFormat> : EnumFormatter<ZTexFormat::U24>
{
  constexpr formatter() : EnumFormatter({"u8", "u16", "u24"}) {}
};

// Z texture operator
enum class ZTexOp : u32
{
  Disabled = 0,
  Add = 1,
  Replace = 2
};
template <>
struct fmt::formatter<ZTexOp> : EnumFormatter<ZTexOp::Replace>
{
  constexpr formatter() : EnumFormatter({"Disabled", "Add", "Replace"}) {}
};

// TEV bias value
enum class TevBias : u32
{
  Zero = 0,
  AddHalf = 1,
  SubHalf = 2,
  Compare = 3
};
template <>
struct fmt::formatter<TevBias> : EnumFormatter<TevBias::Compare>
{
  constexpr formatter() : EnumFormatter({"0", "+0.5", "-0.5", "compare"}) {}
};

// Indirect texture format
enum class IndTexFormat : u32
{
  ITF_8 = 0,
  ITF_5 = 1,
  ITF_4 = 2,
  ITF_3 = 3
};
template <>
struct fmt::formatter<IndTexFormat> : EnumFormatter<IndTexFormat::ITF_3>
{
  constexpr formatter() : EnumFormatter({"ITF_8", "ITF_5", "ITF_4", "ITF_3"}) {}
};

// Indirect texture bias
enum class IndTexBias : u32
{
  None = 0,
  S = 1,
  T = 2,
  ST = 3,
  U = 4,
  SU = 5,
  TU_ = 6,  // conflicts with define in PowerPC.h
  STU = 7
};
template <>
struct fmt::formatter<IndTexBias> : EnumFormatter<IndTexBias::STU>
{
  constexpr formatter() : EnumFormatter({"None", "S", "T", "ST", "U", "SU", "TU", "STU"}) {}
};

enum class IndMtxIndex : u32
{
  Off = 0,
  Matrix0 = 1,
  Matrix1 = 2,
  Matrix2 = 3,
};
template <>
struct fmt::formatter<IndMtxIndex> : EnumFormatter<IndMtxIndex::Matrix2>
{
  constexpr formatter() : EnumFormatter({"Off", "Matrix 0", "Matrix 1", "Matrix 2"}) {}
};

enum class IndMtxId : u32
{
  Indirect = 0,
  S = 1,
  T = 2,
};
template <>
struct fmt::formatter<IndMtxId> : EnumFormatter<IndMtxId::T>
{
  constexpr formatter() : EnumFormatter({"Indirect", "S", "T"}) {}
};

// Indirect texture bump alpha
enum class IndTexBumpAlpha : u32
{
  Off = 0,
  S = 1,
  T = 2,
  U = 3
};
template <>
struct fmt::formatter<IndTexBumpAlpha> : EnumFormatter<IndTexBumpAlpha::U>
{
  constexpr formatter() : EnumFormatter({"Off", "S", "T", "U"}) {}
};

// Indirect texture wrap value
enum class IndTexWrap : u32
{
  ITW_OFF = 0,
  ITW_256 = 1,
  ITW_128 = 2,
  ITW_64 = 3,
  ITW_32 = 4,
  ITW_16 = 5,
  ITW_0 = 6
};
template <>
struct fmt::formatter<IndTexWrap> : EnumFormatter<IndTexWrap::ITW_0>
{
  constexpr formatter() : EnumFormatter({"Off", "256", "128", "64", "32", "16", "0"}) {}
};

struct IND_MTXA
{
  BFVIEW(s32, 11, 0, ma)
  BFVIEW(s32, 11, 11, mb)
  BFVIEW(u8, 2, 22, s0)  // bits 0-1 of scale factor
  u32 hex;
};
template <>
struct fmt::formatter<IND_MTXA>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const IND_MTXA& col, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Row 0 (ma): {} ({})\n"
                          "Row 1 (mb): {} ({})\n"
                          "Scale bits: {} (shifted: {})",
                          col.ma() / 1024.0f, col.ma(), col.mb() / 1024.0f, col.mb(), col.s0(),
                          col.s0());
  }
};

struct IND_MTXB
{
  BFVIEW(s32, 11, 0, mc)
  BFVIEW(s32, 11, 11, md)
  BFVIEW(u8, 2, 22, s1)  // bits 2-3 of scale factor
  u32 hex;
};
template <>
struct fmt::formatter<IND_MTXB>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const IND_MTXB& col, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Row 0 (mc): {} ({})\n"
                          "Row 1 (md): {} ({})\n"
                          "Scale bits: {} (shifted: {})",
                          col.mc() / 1024.0f, col.mc(), col.md() / 1024.0f, col.md(), col.s1(),
                          col.s1() << 2);
  }
};

struct IND_MTXC
{
  BFVIEW(s32, 11, 0, me)
  BFVIEW(s32, 11, 11, mf)
  BFVIEW(u8, 1, 22, s2)  // bit 4 of scale factor
  // The SDK treats the scale factor as 6 bits, 2 on each column; however, hardware seems to ignore
  // the top bit.
  BFVIEW(u8, 2, 22, sdk_s2)
  u32 hex;
};
template <>
struct fmt::formatter<IND_MTXC>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const IND_MTXC& col, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Row 0 (me): {} ({})\n"
                          "Row 1 (mf): {} ({})\n"
                          "Scale bits: {} (shifted: {}), given to SDK as {} ({})",
                          col.me() / 1024.0f, col.me(), col.mf() / 1024.0f, col.mf(), col.s2(),
                          col.s2() << 4, col.sdk_s2(), col.sdk_s2() << 4);
  }
};

struct IND_MTX
{
  IND_MTXA col0;
  IND_MTXB col1;
  IND_MTXC col2;
  u8 GetScale() const { return (col0.s0() << 0) | (col1.s1() << 2) | (col2.s2() << 4); }
};

struct IND_IMASK
{
  BFVIEW(u32, 24, 0, mask)
  u32 hex;
};

struct TevStageCombiner
{
  struct ColorCombiner
  {
    // abc=8bit,d=10bit
    BFVIEW(TevColorArg, 4, 0, d)
    BFVIEW(TevColorArg, 4, 4, c)
    BFVIEW(TevColorArg, 4, 8, b)
    BFVIEW(TevColorArg, 4, 12, a)

    BFVIEW(TevBias, 2, 16, bias)
    BFVIEW(TevOp, 1, 18, op)                  // Applies when bias is not compare
    BFVIEW(TevComparison, 1, 18, comparison)  // Applies when bias is compare
    BFVIEW(bool, 1, 19, clamp)

    BFVIEW(TevScale, 2, 20, scale)               // Applies when bias is not compare
    BFVIEW(TevCompareMode, 2, 20, compare_mode)  // Applies when bias is compare
    BFVIEW(TevOutput, 2, 22, dest)

    u32 hex;
  };
  struct AlphaCombiner
  {
    BFVIEW(u32, 2, 0, rswap)
    BFVIEW(u32, 2, 2, tswap)
    BFVIEW(TevAlphaArg, 3, 4, d)
    BFVIEW(TevAlphaArg, 3, 7, c)
    BFVIEW(TevAlphaArg, 3, 10, b)
    BFVIEW(TevAlphaArg, 3, 13, a)

    BFVIEW(TevBias, 2, 16, bias)
    BFVIEW(TevOp, 1, 18, op)                  // Applies when bias is not compare
    BFVIEW(TevComparison, 1, 18, comparison)  // Applies when bias is compare
    BFVIEW(bool, 1, 19, clamp)

    BFVIEW(TevScale, 2, 20, scale)               // Applies when bias is not compare
    BFVIEW(TevCompareMode, 2, 20, compare_mode)  // Applies when bias is compare
    BFVIEW(TevOutput, 2, 22, dest)

    u32 hex;
  };

  ColorCombiner colorC;
  AlphaCombiner alphaC;
};
template <>
struct fmt::formatter<TevStageCombiner::ColorCombiner>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TevStageCombiner::ColorCombiner& cc, FormatContext& ctx) const
  {
    auto out = ctx.out();
    if (cc.bias() != TevBias::Compare)
    {
      // Generate an equation view, simplifying out addition of zero and multiplication by 1
      // dest = (d (OP) ((1 - c)*a + c*b) + bias) * scale
      // or equivalently and more readably when the terms are not constants:
      // dest = (d (OP) lerp(a, b, c) + bias) * scale
      // Note that lerping is more complex than the first form shows; see PixelShaderGen's
      // WriteTevRegular for more details.

      static constexpr Common::EnumMap<const char*, TevColorArg::Zero> alt_names = {
          "prev.rgb", "prev.aaa", "c0.rgb",  "c0.aaa",  "c1.rgb", "c1.aaa", "c2.rgb",    "c2.aaa",
          "tex.rgb",  "tex.aaa",  "ras.rgb", "ras.aaa", "1",      ".5",     "konst.rgb", "0",
      };

      const bool has_d = cc.d() != TevColorArg::Zero;
      // If c is one, (1 - c) is zero, so (1-c)*a is zero
      const bool has_ac = cc.a() != TevColorArg::Zero && cc.c() != TevColorArg::One;
      // If either b or c is zero, b*c is zero
      const bool has_bc = cc.b() != TevColorArg::Zero && cc.c() != TevColorArg::Zero;
      const bool has_bias = cc.bias() != TevBias::Zero;  // != Compare is already known
      const bool has_scale = cc.scale() != TevScale::Scale1;

      const char op = (cc.op() == TevOp::Sub ? '-' : '+');

      if (cc.dest() == TevOutput::Prev)
        out = fmt::format_to(out, "dest.rgb = ");
      else
        out = fmt::format_to(out, "{:n}.rgb = ", cc.dest());

      if (has_scale)
        out = fmt::format_to(out, "(");
      if (has_d)
        out = fmt::format_to(out, "{}", alt_names[cc.d()]);
      if (has_ac || has_bc)
      {
        if (has_d)
          out = fmt::format_to(out, " {} ", op);
        else if (cc.op() == TevOp::Sub)
          out = fmt::format_to(out, "{}", op);
        if (has_ac && has_bc)
        {
          if (cc.c() == TevColorArg::Half)
          {
            // has_a and has_b imply that c is not Zero or One, and Half is the only remaining
            // numeric constant.  This results in an average.
            out = fmt::format_to(out, "({} + {})/2", alt_names[cc.a()], alt_names[cc.b()]);
          }
          else
          {
            out = fmt::format_to(out, "lerp({}, {}, {})", alt_names[cc.a()], alt_names[cc.b()],
                                 alt_names[cc.c()]);
          }
        }
        else if (has_ac)
        {
          if (cc.c() == TevColorArg::Zero)
            out = fmt::format_to(out, "{}", alt_names[cc.a()]);
          else if (cc.c() == TevColorArg::Half)  // 1 - .5 is .5
            out = fmt::format_to(out, ".5*{}", alt_names[cc.a()]);
          else
            out = fmt::format_to(out, "(1 - {})*{}", alt_names[cc.c()], alt_names[cc.a()]);
        }
        else  // has_bc
        {
          if (cc.c() == TevColorArg::One)
            out = fmt::format_to(out, "{}", alt_names[cc.b()]);
          else
            out = fmt::format_to(out, "{}*{}", alt_names[cc.c()], alt_names[cc.b()]);
        }
      }
      if (has_bias)
      {
        if (has_ac || has_bc || has_d)
          out = fmt::format_to(out, "{}", cc.bias() == TevBias::AddHalf ? " + .5" : " - .5");
        else
          out = fmt::format_to(out, "{}", cc.bias() == TevBias::AddHalf ? ".5" : "-.5");
      }
      else
      {
        // If nothing has been written so far, add a zero
        if (!(has_ac || has_bc || has_d))
          out = fmt::format_to(out, "0");
      }
      if (has_scale)
        out = fmt::format_to(out, ") * {:n}", cc.scale());
      out = fmt::format_to(out, "\n\n");
    }
    return fmt::format_to(ctx.out(),
                          "a: {}\n"
                          "b: {}\n"
                          "c: {}\n"
                          "d: {}\n"
                          "Bias: {}\n"
                          "Op: {} / Comparison: {}\n"
                          "Clamp: {}\n"
                          "Scale factor: {} / Compare mode: {}\n"
                          "Dest: {}",
                          cc.a(), cc.b(), cc.c(), cc.d(), cc.bias(), cc.op(), cc.comparison(),
                          cc.clamp() ? "Yes" : "No", cc.scale(), cc.compare_mode(), cc.dest());
  }
};
template <>
struct fmt::formatter<TevStageCombiner::AlphaCombiner>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TevStageCombiner::AlphaCombiner& ac, FormatContext& ctx) const
  {
    auto out = ctx.out();
    if (ac.bias() != TevBias::Compare)
    {
      // Generate an equation view, simplifying out addition of zero and multiplication by 1
      // dest = (d (OP) ((1 - c)*a + c*b) + bias) * scale
      // or equivalently and more readably when the terms are not constants:
      // dest = (d (OP) lerp(a, b, c) + bias) * scale
      // Note that lerping is more complex than the first form shows; see PixelShaderGen's
      // WriteTevRegular for more details.

      // We don't need an alt_names map here, unlike the color combiner, as the only special term is
      // Zero, and we we filter that out below.  However, we do need to append ".a" to all
      // parameters, to make it explicit that these are operations on the alpha term instead of the
      // 4-element vector.  We also need to use the :n specifier so that the numeric ID isn't shown.

      const bool has_d = ac.d() != TevAlphaArg::Zero;
      // There is no c value for alpha that results in (1 - c) always being zero
      const bool has_ac = ac.a() != TevAlphaArg::Zero;
      // If either b or c is zero, b*c is zero
      const bool has_bc = ac.b() != TevAlphaArg::Zero && ac.c() != TevAlphaArg::Zero;
      const bool has_bias = ac.bias() != TevBias::Zero;  // != Compare is already known
      const bool has_scale = ac.scale() != TevScale::Scale1;

      const char op = (ac.op() == TevOp::Sub ? '-' : '+');

      if (ac.dest() == TevOutput::Prev)
        out = fmt::format_to(out, "dest.a = ");
      else
        out = fmt::format_to(out, "{:n}.a = ", ac.dest());

      if (has_scale)
        out = fmt::format_to(out, "(");
      if (has_d)
        out = fmt::format_to(out, "{:n}.a", ac.d());
      if (has_ac || has_bc)
      {
        if (has_d)
          out = fmt::format_to(out, " {} ", op);
        else if (ac.op() == TevOp::Sub)
          out = fmt::format_to(out, "{}", op);
        if (has_ac && has_bc)
        {
          out = fmt::format_to(out, "lerp({:n}.a, {:n}.a, {:n}.a)", ac.a(), ac.b(), ac.c());
        }
        else if (has_ac)
        {
          if (ac.c() == TevAlphaArg::Zero)
            out = fmt::format_to(out, "{:n}.a", ac.a());
          else
            out = fmt::format_to(out, "(1 - {:n}.a)*{:n}.a", ac.c(), ac.a());
        }
        else  // has_bc
        {
          out = fmt::format_to(out, "{:n}.a*{:n}.a", ac.c(), ac.b());
        }
      }
      if (has_bias)
      {
        if (has_ac || has_bc || has_d)
          out = fmt::format_to(out, "{}", ac.bias() == TevBias::AddHalf ? " + .5" : " - .5");
        else
          out = fmt::format_to(out, "{}", ac.bias() == TevBias::AddHalf ? ".5" : "-.5");
      }
      else
      {
        // If nothing has been written so far, add a zero
        if (!(has_ac || has_bc || has_d))
          out = fmt::format_to(out, "0");
      }
      if (has_scale)
        out = fmt::format_to(out, ") * {:n}", ac.scale());
      out = fmt::format_to(out, "\n\n");
    }
    return fmt::format_to(out,
                          "a: {}\n"
                          "b: {}\n"
                          "c: {}\n"
                          "d: {}\n"
                          "Bias: {}\n"
                          "Op: {} / Comparison: {}\n"
                          "Clamp: {}\n"
                          "Scale factor: {} / Compare mode: {}\n"
                          "Dest: {}\n"
                          "Rasterized color swaptable: {}\n"
                          "Texture color swaptable: {}",
                          ac.a(), ac.b(), ac.c(), ac.d(), ac.bias(), ac.op(), ac.comparison(),
                          ac.clamp() ? "Yes" : "No", ac.scale(), ac.compare_mode(), ac.dest(),
                          ac.rswap(), ac.tswap());
  }
};

// several discoveries:
// GXSetTevIndBumpST(tevstage, indstage, matrixind)
//  if ( matrix == 2 ) realmat = 6; // 10
//  else if ( matrix == 3 ) realmat = 7; // 11
//  else if ( matrix == 1 ) realmat = 5; // 9
//  GXSetTevIndirect(tevstage, indstage, 0, 3, realmat, 6, 6, 0, 0, 0)
//  GXSetTevIndirect(tevstage+1, indstage, 0, 3, realmat+4, 6, 6, 1, 0, 0)
//  GXSetTevIndirect(tevstage+2, indstage, 0, 0, 0, 0, 0, 1, 0, 0)

struct TevStageIndirect
{
  BFVIEW(u32, 2, 0, bt)  // Indirect tex stage ID
  BFVIEW(IndTexFormat, 2, 2, fmt)
  BFVIEW(IndTexBias, 3, 4, bias)
  BFVIEW(bool, 1, 4, bias_s)
  BFVIEW(bool, 1, 5, bias_t)
  BFVIEW(bool, 1, 6, bias_u)
  BFVIEW(IndTexBumpAlpha, 2, 7, bs)  // Indicates which coordinate will become the 'bump alpha'
  // Indicates which indirect matrix is used when matrix_id is Indirect.  Also always indicates
  // which indirect matrix to use for the scale factor, even with S or T.
  BFVIEW(IndMtxIndex, 2, 9, matrix_index)
  // Should be set to Indirect (0) if matrix_index is Off (0)
  BFVIEW(IndMtxId, 2, 11, matrix_id)
  BFVIEW(IndTexWrap, 3, 13, sw)    // Wrapping factor for S of regular coord
  BFVIEW(IndTexWrap, 3, 16, tw)    // Wrapping factor for T of regular coord
  BFVIEW(bool, 1, 19, lb_utclod)   // Use modified or unmodified texture
                                   // coordinates for LOD computation
  BFVIEW(bool, 1, 20, fb_addprev)  // True if the texture coordinate results from the
                                   // previous TEV stage should be added

  u32 hex;
  BFVIEW(u32, 11, 21, unused)

  // If bs and matrix are zero, the result of the stage is independent of
  // the texture sample data, so we can skip sampling the texture.
  bool IsActive() const
  {
    return bs() != IndTexBumpAlpha::Off || matrix_index() != IndMtxIndex::Off;
  }
};
template <>
struct fmt::formatter<TevStageIndirect>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TevStageIndirect& tevind, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Indirect tex stage ID: {}\n"
                          "Format: {}\n"
                          "Bias: {}\n"
                          "Bump alpha: {}\n"
                          "Offset matrix index: {}\n"
                          "Offset matrix ID: {}\n"
                          "Regular coord S wrapping factor: {}\n"
                          "Regular coord T wrapping factor: {}\n"
                          "Use modified texture coordinates for LOD computation: {}\n"
                          "Add texture coordinates from previous TEV stage: {}",
                          tevind.bt(), tevind.fmt(), tevind.bias(), tevind.bs(),
                          tevind.matrix_index(), tevind.matrix_id(), tevind.sw(), tevind.tw(),
                          tevind.lb_utclod() ? "Yes" : "No", tevind.fb_addprev() ? "Yes" : "No");
  }
};

enum class RasColorChan : u32
{
  Color0 = 0,
  Color1 = 1,
  AlphaBump = 5,
  NormalizedAlphaBump = 6,
  Zero = 7,
};
template <>
struct fmt::formatter<RasColorChan> : EnumFormatter<RasColorChan::Zero>
{
  static constexpr array_type names = {
      "Color chan 0", "Color chan 1", nullptr,           nullptr,
      nullptr,        "Alpha bump",   "Norm alpha bump", "Zero",
  };
  constexpr formatter() : EnumFormatter(names) {}
};

struct TwoTevStageOrders
{
  BFVIEW(u32, 3, 0, texmap_even)
  BFVIEW(u32, 3, 3, texcoord_even)
  BFVIEW(bool, 1, 6, enable_tex_even)  // true if should read from texture
  BFVIEW(RasColorChan, 3, 7, colorchan_even)

  BFVIEW(u32, 3, 12, texmap_odd)
  BFVIEW(u32, 3, 15, texcoord_odd)
  BFVIEW(bool, 1, 18, enable_tex_odd)  // true if should read from texture
  BFVIEW(RasColorChan, 3, 19, colorchan_odd)

  u32 hex;
  u32 getTexMap(int i) const { return i ? texmap_odd() : texmap_even(); }
  u32 getTexCoord(int i) const { return i ? texcoord_odd() : texcoord_even(); }
  u32 getEnable(int i) const { return i ? enable_tex_odd() : enable_tex_even(); }
  RasColorChan getColorChan(int i) const
  {
    return i ? colorchan_odd().Get() : colorchan_even().Get();
  }
};
template <>
struct fmt::formatter<std::pair<u8, TwoTevStageOrders>>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const std::pair<u8, TwoTevStageOrders>& p, FormatContext& ctx) const
  {
    const auto& [cmd, stages] = p;
    const u8 stage_even = (cmd - BPMEM_TREF) * 2;
    const u8 stage_odd = stage_even + 1;

    return fmt::format_to(ctx.out(),
                          "Stage {0} texmap: {1}\nStage {0} tex coord: {2}\n"
                          "Stage {0} enable texmap: {3}\nStage {0} rasterized color channel: {4}\n"
                          "Stage {5} texmap: {6}\nStage {5} tex coord: {7}\n"
                          "Stage {5} enable texmap: {8}\nStage {5} rasterized color channel: {9}\n",
                          stage_even, stages.texmap_even(), stages.texcoord_even(),
                          stages.enable_tex_even() ? "Yes" : "No", stages.colorchan_even(),
                          stage_odd, stages.texmap_odd(), stages.texcoord_odd(),
                          stages.enable_tex_odd() ? "Yes" : "No", stages.colorchan_odd());
  }
};

struct TEXSCALE
{
  BFVIEW(u32, 4, 0, ss0)   // Indirect tex stage 0 or 2, 2^(-ss0)
  BFVIEW(u32, 4, 4, ts0)   // Indirect tex stage 0 or 2
  BFVIEW(u32, 4, 8, ss1)   // Indirect tex stage 1 or 3
  BFVIEW(u32, 4, 12, ts1)  // Indirect tex stage 1 or 3
  u32 hex;
};
template <>
struct fmt::formatter<std::pair<u8, TEXSCALE>>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const std::pair<u8, TEXSCALE>& p, FormatContext& ctx) const
  {
    const auto& [cmd, scale] = p;
    const u8 even = (cmd - BPMEM_RAS1_SS0) * 2;
    const u8 odd_ = even + 1;

    return fmt::format_to(ctx.out(),
                          "Indirect stage {0} S coord scale: {1} ({2})\n"
                          "Indirect stage {0} T coord scale: {3} ({4})\n"
                          "Indirect stage {5} S coord scale: {6} ({7})\n"
                          "Indirect stage {5} T coord scale: {8} ({9})",
                          even, 1.f / (1 << scale.ss0()), scale.ss0(), 1.f / (1 << scale.ts0()),
                          scale.ts0(), odd_, 1.f / (1 << scale.ss1()), scale.ss1(),
                          1.f / (1 << scale.ts1()), scale.ts1());
  }
};

struct RAS1_IREF
{
  BFVIEW(u32, 3, 0, bi0)  // Indirect tex stage 0 texmap
  BFVIEW(u32, 3, 3, bc0)  // Indirect tex stage 0 tex coord
  BFVIEW(u32, 3, 6, bi1)
  BFVIEW(u32, 3, 9, bc1)
  BFVIEW(u32, 3, 12, bi2)
  BFVIEW(u32, 3, 15, bc2)
  BFVIEW(u32, 3, 18, bi3)
  BFVIEW(u32, 3, 21, bc3)
  u32 hex;

  u32 getTexCoord(int i) const { return (hex >> (6 * i + 3)) & 7; }
  u32 getTexMap(int i) const { return (hex >> (6 * i)) & 7; }
};
template <>
struct fmt::formatter<RAS1_IREF>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const RAS1_IREF& indref, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Indirect stage 0 texmap: {}\nIndirect stage 0 tex coord: {}\n"
                          "Indirect stage 1 texmap: {}\nIndirect stage 1 tex coord: {}\n"
                          "Indirect stage 2 texmap: {}\nIndirect stage 2 tex coord: {}\n"
                          "Indirect stage 3 texmap: {}\nIndirect stage 3 tex coord: {}",
                          indref.bi0(), indref.bc0(), indref.bi1(), indref.bc1(), indref.bi2(),
                          indref.bc2(), indref.bi3(), indref.bc3());
  }
};

// Texture structs
enum class WrapMode : u32
{
  Clamp = 0,
  Repeat = 1,
  Mirror = 2,
  // Hardware testing indicates that WrapMode set to 3 behaves the same as clamp, though this is an
  // invalid value
};
template <>
struct fmt::formatter<WrapMode> : EnumFormatter<WrapMode::Mirror>
{
  constexpr formatter() : EnumFormatter({"Clamp", "Repeat", "Mirror"}) {}
};

enum class MipMode : u32
{
  None = 0,
  Point = 1,
  Linear = 2,
};
template <>
struct fmt::formatter<MipMode> : EnumFormatter<MipMode::Linear>
{
  constexpr formatter() : EnumFormatter({"None", "Mip point", "Mip linear"}) {}
};

enum class FilterMode : u32
{
  Near = 0,
  Linear = 1,
};
template <>
struct fmt::formatter<FilterMode> : EnumFormatter<FilterMode::Linear>
{
  constexpr formatter() : EnumFormatter({"Near", "Linear"}) {}
};

enum class LODType : u32
{
  Edge = 0,
  Diagonal = 1,
};
template <>
struct fmt::formatter<LODType> : EnumFormatter<LODType::Diagonal>
{
  constexpr formatter() : EnumFormatter({"Edge LOD", "Diagonal LOD"}) {}
};

enum class MaxAniso
{
  One = 0,
  Two = 1,
  Four = 2,
};
template <>
struct fmt::formatter<MaxAniso> : EnumFormatter<MaxAniso::Four>
{
  constexpr formatter() : EnumFormatter({"1", "2", "4"}) {}
};

struct TexMode0
{
  BFVIEW(WrapMode, 2, 0, wrap_s)
  BFVIEW(WrapMode, 2, 2, wrap_t)
  BFVIEW(FilterMode, 1, 4, mag_filter)
  BFVIEW(MipMode, 2, 5, mipmap_filter)
  BFVIEW(FilterMode, 1, 7, min_filter)
  BFVIEW(LODType, 1, 8, diag_lod)
  BFVIEW(s32, 8, 9, lod_bias)
  BFVIEW(MaxAniso, 2, 19, max_aniso)
  BFVIEW(bool, 1, 21, lod_clamp)
  u32 hex;
};
template <>
struct fmt::formatter<TexMode0>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TexMode0& mode, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Wrap S: {}\n"
                          "Wrap T: {}\n"
                          "Mag filter: {}\n"
                          "Mipmap filter: {}\n"
                          "Min filter: {}\n"
                          "LOD type: {}\n"
                          "LOD bias: {} ({})\n"
                          "Max anisotropic filtering: {}\n"
                          "LOD/bias clamp: {}",
                          mode.wrap_s(), mode.wrap_t(), mode.mag_filter(), mode.mipmap_filter(),
                          mode.min_filter(), mode.diag_lod(), mode.lod_bias(),
                          mode.lod_bias() / 32.f, mode.max_aniso(),
                          mode.lod_clamp() ? "Yes" : "No");
  }
};

struct TexMode1
{
  BFVIEW(u8, 8, 0, min_lod)
  BFVIEW(u8, 8, 8, max_lod)
  u32 hex;
};
template <>
struct fmt::formatter<TexMode1>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TexMode1& mode, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Min LOD: {} ({})\nMax LOD: {} ({})", mode.min_lod(),
                          mode.min_lod() / 16.f, mode.max_lod(), mode.max_lod() / 16.f);
  }
};

struct TexImage0
{
  BFVIEW(u32, 10, 0, width)    // Actually w-1
  BFVIEW(u32, 10, 10, height)  // Actually h-1
  BFVIEW(TextureFormat, 4, 20, format)
  u32 hex;
};
template <>
struct fmt::formatter<TexImage0>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TexImage0& teximg, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Width: {}\n"
                          "Height: {}\n"
                          "Format: {}",
                          teximg.width() + 1, teximg.height() + 1, teximg.format());
  }
};

struct TexImage1
{
  BFVIEW(u32, 15, 0, tmem_even)  // TMEM line index for even LODs
  BFVIEW(u32, 3, 15, cache_width)
  BFVIEW(u32, 3, 18, cache_height)
  // true if this texture is managed manually (false means we'll
  // autofetch the texture data whenever it changes)
  BFVIEW(bool, 1, 21, cache_manually_managed)
  u32 hex;
};
template <>
struct fmt::formatter<TexImage1>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TexImage1& teximg, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Even TMEM Offset: {:x}\n"
                          "Even TMEM Width: {}\n"
                          "Even TMEM Height: {}\n"
                          "Cache is manually managed: {}",
                          teximg.tmem_even(), teximg.cache_width(), teximg.cache_height(),
                          teximg.cache_manually_managed() ? "Yes" : "No");
  }
};

struct TexImage2
{
  BFVIEW(u32, 15, 0, tmem_odd)  // tmem line index for odd LODs
  BFVIEW(u32, 3, 15, cache_width)
  BFVIEW(u32, 3, 18, cache_height)
  u32 hex;
};
template <>
struct fmt::formatter<TexImage2>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TexImage2& teximg, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Odd TMEM Offset: {:x}\n"
                          "Odd TMEM Width: {}\n"
                          "Odd TMEM Height: {}",
                          teximg.tmem_odd(), teximg.cache_width(), teximg.cache_height());
  }
};

struct TexImage3
{
  BFVIEW(u32, 24, 0, image_base)  // address in memory >> 5 (was 20 for GC)
  u32 hex;
};
template <>
struct fmt::formatter<TexImage3>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TexImage3& teximg, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Source address (32 byte aligned): 0x{:06X}",
                          teximg.image_base() << 5);
  }
};

struct TexTLUT
{
  BFVIEW(u32, 10, 0, tmem_offset)
  BFVIEW(TLUTFormat, 2, 10, tlut_format)
  u32 hex;
};
template <>
struct fmt::formatter<TexTLUT>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TexTLUT& tlut, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Address: {:08x}\nFormat: {}", tlut.tmem_offset() << 9,
                          tlut.tlut_format());
  }
};

struct ZTex1
{
  BFVIEW(u32, 24, 0, bias)
  u32 hex;
};

struct ZTex2
{
  BFVIEW(ZTexFormat, 2, 0, type)
  BFVIEW(ZTexOp, 2, 2, op)
  u32 hex;
};
template <>
struct fmt::formatter<ZTex2>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const ZTex2& ztex2, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Type: {}\nOperation: {}", ztex2.type(), ztex2.op());
  }
};

// Geometry/other structs
enum class CullMode : u32
{
  None = 0,
  Back = 1,   // cull back-facing primitives
  Front = 2,  // cull front-facing primitives
  All = 3,    // cull all primitives
};
template <>
struct fmt::formatter<CullMode> : EnumFormatter<CullMode::All>
{
  static constexpr array_type names = {
      "None",
      "Back-facing primitives only",
      "Front-facing primitives only",
      "All primitives",
  };
  constexpr formatter() : EnumFormatter(names) {}
};

struct GenMode
{
  BFVIEW(u32, 4, 0, numtexgens)
  BFVIEW(u32, 3, 4, numcolchans)
  BFVIEW(u32, 1, 7, unused)         // 1 bit unused?
  BFVIEW(bool, 1, 8, flat_shading)  // unconfirmed
  BFVIEW(bool, 1, 9, multisampling)
  // This value is 1 less than the actual number (0-15 map to 1-16).
  // In other words there is always at least 1 tev stage
  BFVIEW(u32, 4, 10, numtevstages)
  BFVIEW(CullMode, 2, 14, cullmode)
  BFVIEW(u32, 3, 16, numindstages)
  BFVIEW(bool, 1, 19, zfreeze)

  u32 hex;
};
template <>
struct fmt::formatter<GenMode>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const GenMode& mode, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Num tex gens: {}\n"
                          "Num color channels: {}\n"
                          "Unused bit: {}\n"
                          "Flat shading (unconfirmed): {}\n"
                          "Multisampling: {}\n"
                          "Num TEV stages: {}\n"
                          "Cull mode: {}\n"
                          "Num indirect stages: {}\n"
                          "ZFreeze: {}",
                          mode.numtexgens(), mode.numcolchans(), mode.unused(),
                          mode.flat_shading() ? "Yes" : "No", mode.multisampling() ? "Yes" : "No",
                          mode.numtevstages() + 1, mode.cullmode(), mode.numindstages(),
                          mode.zfreeze() ? "Yes" : "No");
  }
};

enum class AspectRatioAdjustment
{
  DontAdjust = 0,
  Adjust = 1,
};
template <>
struct fmt::formatter<AspectRatioAdjustment> : EnumFormatter<AspectRatioAdjustment::Adjust>
{
  constexpr formatter() : EnumFormatter({"Don't adjust", "Adjust"}) {}
};

struct LPSize
{
  BFVIEW(u32, 8, 0, linesize)   // in 1/6th pixels
  BFVIEW(u32, 8, 8, pointsize)  // in 1/6th pixels
  BFVIEW(u32, 3, 16, lineoff)
  BFVIEW(u32, 3, 19, pointoff)
  // interlacing: adjust for pixels having AR of 1/2
  BFVIEW(AspectRatioAdjustment, 1, 22, adjust_for_aspect_ratio)
  u32 hex;
};
template <>
struct fmt::formatter<LPSize>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const LPSize& lp, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Line size: {} ({:.3} pixels)\n"
                          "Point size: {} ({:.3} pixels)\n"
                          "Line offset: {}\n"
                          "Point offset: {}\n"
                          "Adjust line aspect ratio: {}",
                          lp.linesize(), lp.linesize() / 6.f, lp.pointsize(), lp.pointsize() / 6.f,
                          lp.lineoff(), lp.pointoff(), lp.adjust_for_aspect_ratio());
  }
};

struct ScissorPos
{
  // The top bit is ignored, and not included in the mask used by GX SDK functions
  // (though libogc includes it for the bottom coordinate (only) for some reason)
  // x_full and y_full include that bit for the FIFO analyzer, though it is usually unset.
  // The SDK also adds 342 to these values.
  BFVIEW(u32, 11, 0, y)
  BFVIEW(u32, 12, 0, y_full)
  BFVIEW(u32, 11, 12, x)
  BFVIEW(u32, 12, 12, x_full)
  u32 hex;
};
template <>
struct fmt::formatter<ScissorPos>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const ScissorPos& pos, FormatContext& ctx)
  {
    return fmt::format_to(ctx.out(),
                          "X: {} (raw: {})\n"
                          "Y: {} (raw: {})",
                          pos.x() - 342, pos.x_full(), pos.y() - 342, pos.y_full());
  }
};

struct ScissorOffset
{
  // The scissor offset ignores the top bit (though it isn't masked off by the GX SDK).
  // Each value is also divided by 2 (so 0-511 map to 0-1022).
  // x_full and y_full include that top bit for the FIFO analyzer, though it is usually unset.
  // The SDK also adds 342 to each value (before dividing it).
  BFVIEW(u32, 9, 0, x)
  BFVIEW(u32, 10, 0, x_full)
  BFVIEW(u32, 9, 10, y)
  BFVIEW(u32, 10, 10, y_full)
  u32 hex;
};
template <>
struct fmt::formatter<ScissorOffset>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const ScissorOffset& off, FormatContext& ctx)
  {
    return fmt::format_to(ctx.out(),
                          "X: {} (raw: {})\n"
                          "Y: {} (raw: {})",
                          (off.x() << 1) - 342, off.x_full(), (off.y() << 1) - 342, off.y_full());
  }
};

struct X10Y10
{
  BFVIEW(u32, 10, 0, x)
  BFVIEW(u32, 10, 10, y)
  u32 hex;
};

// Framebuffer/pixel stuff (incl fog)
enum class SrcBlendFactor : u32
{
  Zero = 0,
  One = 1,
  DstClr = 2,
  InvDstClr = 3,
  SrcAlpha = 4,
  InvSrcAlpha = 5,
  DstAlpha = 6,
  InvDstAlpha = 7
};
template <>
struct fmt::formatter<SrcBlendFactor> : EnumFormatter<SrcBlendFactor::InvDstAlpha>
{
  static constexpr array_type names = {"0",         "1",           "dst_color", "1-dst_color",
                                       "src_alpha", "1-src_alpha", "dst_alpha", "1-dst_alpha"};
  constexpr formatter() : EnumFormatter(names) {}
};

enum class DstBlendFactor : u32
{
  Zero = 0,
  One = 1,
  SrcClr = 2,
  InvSrcClr = 3,
  SrcAlpha = 4,
  InvSrcAlpha = 5,
  DstAlpha = 6,
  InvDstAlpha = 7
};
template <>
struct fmt::formatter<DstBlendFactor> : EnumFormatter<DstBlendFactor::InvDstAlpha>
{
  static constexpr array_type names = {"0",         "1",           "src_color", "1-src_color",
                                       "src_alpha", "1-src_alpha", "dst_alpha", "1-dst_alpha"};
  constexpr formatter() : EnumFormatter(names) {}
};

enum class LogicOp : u32
{
  Clear = 0,
  And = 1,
  AndReverse = 2,
  Copy = 3,
  AndInverted = 4,
  NoOp = 5,
  Xor = 6,
  Or = 7,
  Nor = 8,
  Equiv = 9,
  Invert = 10,
  OrReverse = 11,
  CopyInverted = 12,
  OrInverted = 13,
  Nand = 14,
  Set = 15
};
template <>
struct fmt::formatter<LogicOp> : EnumFormatter<LogicOp::Set>
{
  static constexpr array_type names = {
      "Clear (0)",
      "And (src & dst)",
      "And Reverse (src & ~dst)",
      "Copy (src)",
      "And Inverted (~src & dst)",
      "NoOp (dst)",
      "Xor (src ^ dst)",
      "Or (src | dst)",
      "Nor (~(src | dst))",
      "Equiv (~(src ^ dst))",
      "Invert (~dst)",
      "Or Reverse (src | ~dst)",
      "Copy Inverted (~src)",
      "Or Inverted (~src | dst)",
      "Nand (~(src & dst))",
      "Set (1)",
  };
  constexpr formatter() : EnumFormatter(names) {}
};

struct BlendMode
{
  BFVIEW(bool, 1, 0, blendenable)
  BFVIEW(bool, 1, 1, logicopenable)
  BFVIEW(bool, 1, 2, dither)
  BFVIEW(bool, 1, 3, colorupdate)
  BFVIEW(bool, 1, 4, alphaupdate)
  BFVIEW(DstBlendFactor, 3, 5, dstfactor)
  BFVIEW(SrcBlendFactor, 3, 8, srcfactor)
  BFVIEW(bool, 1, 11, subtract)
  BFVIEW(LogicOp, 4, 12, logicmode)

  u32 hex;

  bool UseLogicOp() const;
};
template <>
struct fmt::formatter<BlendMode>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const BlendMode& mode, FormatContext& ctx) const
  {
    static constexpr std::array<const char*, 2> no_yes = {"No", "Yes"};
    return fmt::format_to(ctx.out(),
                          "Enable: {}\n"
                          "Logic ops: {}\n"
                          "Dither: {}\n"
                          "Color write: {}\n"
                          "Alpha write: {}\n"
                          "Dest factor: {}\n"
                          "Source factor: {}\n"
                          "Subtract: {}\n"
                          "Logic mode: {}",
                          no_yes[mode.blendenable()], no_yes[mode.logicopenable()],
                          no_yes[mode.dither()], no_yes[mode.colorupdate()],
                          no_yes[mode.alphaupdate()], mode.dstfactor(), mode.srcfactor(),
                          no_yes[mode.subtract()], mode.logicmode());
  }
};

struct FogParam0
{
  BFVIEW(u32, 11, 0, mant)
  BFVIEW(u32, 8, 11, exp)
  BFVIEW(bool, 1, 19, sign)

  u32 hex;
  float FloatValue() const;
};
template <>
struct fmt::formatter<FogParam0>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const FogParam0& param, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "A value: {}\nMantissa: {}\nExponent: {}\nSign: {}",
                          param.FloatValue(), param.mant(), param.exp(), param.sign() ? '-' : '+');
  }
};

enum class FogProjection : u32
{
  Perspective = 0,
  Orthographic = 1,
};
template <>
struct fmt::formatter<FogProjection> : EnumFormatter<FogProjection::Orthographic>
{
  constexpr formatter() : EnumFormatter({"Perspective", "Orthographic"}) {}
};

enum class FogType : u32
{
  Off = 0,
  Linear = 2,
  Exp = 4,
  ExpSq = 5,
  BackwardsExp = 6,
  BackwardsExpSq = 7,
};
template <>
struct fmt::formatter<FogType> : EnumFormatter<FogType::BackwardsExpSq>
{
  static constexpr array_type names = {
      "Off (no fog)",
      nullptr,
      "Linear fog",
      nullptr,
      "Exponential fog",
      "Exponential-squared fog",
      "Backwards exponential fog",
      "Backwards exponenential-sequared fog",
  };
  constexpr formatter() : EnumFormatter(names) {}
};

struct FogParam3
{
  BFVIEW(u32, 11, 0, c_mant)
  BFVIEW(u32, 8, 11, c_exp)
  BFVIEW(bool, 1, 19, c_sign)
  BFVIEW(FogProjection, 1, 20, proj)
  BFVIEW(FogType, 3, 21, fsel)

  u32 hex;
  float FloatValue() const;
};
template <>
struct fmt::formatter<FogParam3>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const FogParam3& param, FormatContext& ctx) const
  {
    return fmt::format_to(
        ctx.out(), "C value: {}\nMantissa: {}\nExponent: {}\nSign: {}\nProjection: {}\nFsel: {}",
        param.FloatValue(), param.c_mant(), param.c_exp(), param.c_sign() ? '-' : '+', param.proj(),
        param.fsel());
  }
};

struct FogRangeKElement
{
  BFVIEW(u32, 12, 0, HI)
  BFVIEW(u32, 12, 12, LO)

  // TODO: Which scaling coefficient should we use here? This is just a guess!
  float GetValue(int i) const { return (i ? HI() : LO()) / 256.f; }
  u32 HEX;
};

struct FogRangeParams
{
  struct RangeBase
  {
    BFVIEW(u32, 10, 0, Center)  // viewport center + 342
    BFVIEW(bool, 1, 10, Enabled)
    u32 hex;
  };
  RangeBase Base;
  FogRangeKElement K[5];
};
template <>
struct fmt::formatter<FogRangeParams::RangeBase>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const FogRangeParams::RangeBase& range, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Center: {}\nEnabled: {}", range.Center(),
                          range.Enabled() ? "Yes" : "No");
  }
};
template <>
struct fmt::formatter<FogRangeKElement>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const FogRangeKElement& range, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "High: {}\nLow: {}", range.HI(), range.LO());
  }
};

// final eq: ze = A/(B_MAG - (Zs>>B_SHF));
struct FogParams
{
  FogParam0 a;
  u32 b_magnitude;
  u32 b_shift;  // b's exp + 1?
  FogParam3 c_proj_fsel;

  struct FogColor
  {
    BFVIEW(u8, 8, 0, b)
    BFVIEW(u8, 8, 8, g)
    BFVIEW(u8, 8, 16, r)
    u32 hex;
  };

  FogColor color;  // 0:b 8:g 16:r - nice!

  // Special case where a and c are infinite and the sign matches, resulting in a result of NaN.
  bool IsNaNCase() const;
  float GetA() const;

  // amount to subtract from eyespacez after range adjustment
  float GetC() const;
};
template <>
struct fmt::formatter<FogParams::FogColor>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const FogParams::FogColor& color, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Red: {}\nGreen: {}\nBlue: {}", color.r(), color.g(),
                          color.b());
  }
};

enum class CompareMode : u32
{
  Never = 0,
  Less = 1,
  Equal = 2,
  LEqual = 3,
  Greater = 4,
  NEqual = 5,
  GEqual = 6,
  Always = 7
};
template <>
struct fmt::formatter<CompareMode> : EnumFormatter<CompareMode::Always>
{
  static constexpr array_type names = {"Never",   "Less",   "Equal",  "LEqual",
                                       "Greater", "NEqual", "GEqual", "Always"};
  constexpr formatter() : EnumFormatter(names) {}
};

struct ZMode
{
  BFVIEW(bool, 1, 0, testenable)
  BFVIEW(CompareMode, 3, 1, func)
  BFVIEW(bool, 1, 4, updateenable)

  u32 hex;
};
template <>
struct fmt::formatter<ZMode>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const ZMode& mode, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Enable test: {}\n"
                          "Compare function: {}\n"
                          "Enable updates: {}",
                          mode.testenable() ? "Yes" : "No", mode.func(),
                          mode.updateenable() ? "Yes" : "No");
  }
};

struct ConstantAlpha
{
  BFVIEW(u8, 8, 0, alpha)
  BFVIEW(bool, 1, 8, enable)
  u32 hex;
};
template <>
struct fmt::formatter<ConstantAlpha>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const ConstantAlpha& c, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Enable: {}\n"
                          "Alpha value: {:02x}",
                          c.enable() ? "Yes" : "No", c.alpha());
  }
};

struct FieldMode
{
  // adjust vertex tex LOD computation to account for interlacing
  BFVIEW(AspectRatioAdjustment, 1, 0, texLOD)
  u32 hex;
};
template <>
struct fmt::formatter<FieldMode>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const FieldMode& mode, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Adjust vertex tex LOD computation to account for interlacing: {}",
                          mode.texLOD());
  }
};

enum class FieldMaskState : u32
{
  Skip = 0,
  Write = 1,
};
template <>
struct fmt::formatter<FieldMaskState> : EnumFormatter<FieldMaskState::Write>
{
  constexpr formatter() : EnumFormatter({"Skipped", "Written"}) {}
};

struct FieldMask
{
  // Fields are written to the EFB only if their bit is set to write.
  BFVIEW(FieldMaskState, 1, 0, odd)
  BFVIEW(FieldMaskState, 1, 1, even)
  u32 hex;
};
template <>
struct fmt::formatter<FieldMask>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const FieldMask& mask, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Odd field: {}\nEven field: {}", mask.odd(), mask.even());
  }
};

enum class PixelFormat : u32
{
  RGB8_Z24 = 0,
  RGBA6_Z24 = 1,
  RGB565_Z16 = 2,
  Z24 = 3,
  Y8 = 4,
  U8 = 5,
  V8 = 6,
  YUV420 = 7,
  INVALID_FMT = 0xffffffff,  // Used by Dolphin to represent a missing value.
};
template <>
struct fmt::formatter<PixelFormat> : EnumFormatter<PixelFormat::YUV420>
{
  static constexpr array_type names = {"RGB8_Z24", "RGBA6_Z24", "RGB565_Z16", "Z24",
                                       "Y8",       "U8",        "V8",         "YUV420"};
  constexpr formatter() : EnumFormatter(names) {}
};

enum class DepthFormat : u32
{
  ZLINEAR = 0,
  ZNEAR = 1,
  ZMID = 2,
  ZFAR = 3,

  // It seems these Z formats aren't supported/were removed ?
  ZINV_LINEAR = 4,
  ZINV_NEAR = 5,
  ZINV_MID = 6,
  ZINV_FAR = 7
};
template <>
struct fmt::formatter<DepthFormat> : EnumFormatter<DepthFormat::ZINV_FAR>
{
  static constexpr array_type names = {
      "linear",     "compressed (near)",     "compressed (mid)",     "compressed (far)",
      "inv linear", "compressed (inv near)", "compressed (inv mid)", "compressed (inv far)",
  };
  constexpr formatter() : EnumFormatter(names) {}
};

struct PEControl
{
  BFVIEW(PixelFormat, 3, 0, pixel_format)
  BFVIEW(DepthFormat, 3, 3, zformat)
  BFVIEW(bool, 1, 6, early_ztest)

  u32 hex;
};
template <>
struct fmt::formatter<PEControl>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const PEControl& config, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "EFB pixel format: {}\n"
                          "Depth format: {}\n"
                          "Early depth test: {}",
                          config.pixel_format(), config.zformat(),
                          config.early_ztest() ? "Yes" : "No");
  }
};

// Texture coordinate stuff

struct TCInfo
{
  BFVIEW(u32, 16, 0, scale_minus_1)
  BFVIEW(bool, 1, 16, range_bias)
  BFVIEW(bool, 1, 17, cylindric_wrap)
  // These bits only have effect in the s field of TCoordInfo
  BFVIEW(bool, 1, 18, line_offset)
  BFVIEW(bool, 1, 19, point_offset)
  u32 hex;
};
template <>
struct fmt::formatter<std::pair<bool, TCInfo>>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const std::pair<bool, TCInfo>& p, FormatContext& ctx) const
  {
    const auto& [is_s, info] = p;
    auto out =
        fmt::format_to(ctx.out(),
                       "{0} coord scale: {1}\n"
                       "{0} coord range bias: {2}\n"
                       "{0} coord cylindric wrap: {3}",
                       is_s ? 'S' : 'T', info.scale_minus_1() + 1, info.range_bias() ? "Yes" : "No",
                       info.cylindric_wrap() ? "Yes" : "No");
    if (is_s)
    {
      out = fmt::format_to(out,
                           "\nUse line offset: {}"
                           "\nUse point offset: {}",
                           info.line_offset() ? "Yes" : "No", info.point_offset() ? "Yes" : "No");
    }
    return out;
  }
};

struct TCoordInfo
{
  TCInfo s;
  TCInfo t;
};

enum class TevRegType : u32
{
  Color = 0,
  Constant = 1,
};
template <>
struct fmt::formatter<TevRegType> : EnumFormatter<TevRegType::Constant>
{
  constexpr formatter() : EnumFormatter({"Color", "Constant"}) {}
};

struct TevReg
{
  // TODO: Check if Konst uses all 11 bits or just 8
  struct RA
  {
    u32 hex;

    BFVIEW(s32, 11, 0, red)
    BFVIEW(s32, 11, 12, alpha)
    BFVIEW(TevRegType, 1, 23, type)
  };
  struct BG
  {
    u32 hex;

    BFVIEW(s32, 11, 0, blue)
    BFVIEW(s32, 11, 12, green)
    BFVIEW(TevRegType, 1, 23, type)
  };

  RA ra;
  BG bg;
};
template <>
struct fmt::formatter<TevReg::RA>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TevReg::RA& ra, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Type: {}\nAlpha: {:03x}\nRed: {:03x}", ra.type(), ra.alpha(),
                          ra.red());
  }
};
template <>
struct fmt::formatter<TevReg::BG>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TevReg::BG& bg, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Type: {}\nGreen: {:03x}\nBlue: {:03x}", bg.type(), bg.green(),
                          bg.blue());
  }
};
template <>
struct fmt::formatter<TevReg>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TevReg& reg, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "{}\n{}", reg.ra, reg.bg);
  }
};

enum class ColorChannel : u32
{
  Red = 0,
  Green = 1,
  Blue = 2,
  Alpha = 3,
};
template <>
struct fmt::formatter<ColorChannel> : EnumFormatter<ColorChannel::Alpha>
{
  formatter() : EnumFormatter({"Red", "Green", "Blue", "Alpha"}) {}
};

enum class KonstSel : u32
{
  V1 = 0,
  V7_8 = 1,
  V3_4 = 2,
  V5_8 = 3,
  V1_2 = 4,
  V3_8 = 5,
  V1_4 = 6,
  V1_8 = 7,
  // 8-11 are invalid values that output 0 (8-15 for alpha)
  K0 = 12,  // Color only
  K1 = 13,  // Color only
  K2 = 14,  // Color only
  K3 = 15,  // Color only
  K0_R = 16,
  K1_R = 17,
  K2_R = 18,
  K3_R = 19,
  K0_G = 20,
  K1_G = 21,
  K2_G = 22,
  K3_G = 23,
  K0_B = 24,
  K1_B = 25,
  K2_B = 26,
  K3_B = 27,
  K0_A = 28,
  K1_A = 29,
  K2_A = 30,
  K3_A = 31,
};
template <>
struct fmt::formatter<KonstSel> : EnumFormatter<KonstSel::K3_A>
{
  static constexpr array_type names = {
      "1",
      "7/8",
      "3/4",
      "5/8",
      "1/2",
      "3/8",
      "1/4",
      "1/8",
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      "Konst 0 RGB (invalid for alpha)",
      "Konst 1 RGB (invalid for alpha)",
      "Konst 2 RGB (invalid for alpha)",
      "Konst 3 RGB (invalid for alpha)",
      "Konst 0 Red",
      "Konst 1 Red",
      "Konst 2 Red",
      "Konst 3 Red",
      "Konst 0 Green",
      "Konst 1 Green",
      "Konst 2 Green",
      "Konst 3 Green",
      "Konst 0 Blue",
      "Konst 1 Blue",
      "Konst 2 Blue",
      "Konst 3 Blue",
      "Konst 0 Alpha",
      "Konst 1 Alpha",
      "Konst 2 Alpha",
      "Konst 3 Alpha",
  };
  constexpr formatter() : EnumFormatter(names) {}
};

struct TevKSel
{
  BFVIEW(ColorChannel, 2, 0, swap_rb)  // Odd ksel number: red; even: blue
  BFVIEW(ColorChannel, 2, 2, swap_ga)  // Odd ksel number: green; even: alpha
  BFVIEW(KonstSel, 5, 4, kcsel_even)
  BFVIEW(KonstSel, 5, 9, kasel_even)
  BFVIEW(KonstSel, 5, 14, kcsel_odd)
  BFVIEW(KonstSel, 5, 19, kasel_odd)
  u32 hex;
};
template <>
struct fmt::formatter<std::pair<u8, TevKSel>>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const std::pair<u8, TevKSel>& p, FormatContext& ctx) const
  {
    const auto& [cmd, ksel] = p;
    const u8 swap_number = (cmd - BPMEM_TEV_KSEL) / 2;
    const bool swap_ba = (cmd - BPMEM_TEV_KSEL) & 1;
    const u8 even_stage = (cmd - BPMEM_TEV_KSEL) * 2;
    const u8 odd_stage = even_stage + 1;

    return fmt::format_to(ctx.out(),
                          "Swap table {0}: {1} channel comes from input's {2} channel\n"
                          "Swap table {0}: {3} channel comes from input's {4} channel\n"
                          "TEV stage {5} konst color: {6}\n"
                          "TEV stage {5} konst alpha: {7}\n"
                          "TEV stage {8} konst color: {9}\n"
                          "TEV stage {8} konst alpha: {10}",
                          swap_number, swap_ba ? "Blue" : "Red", ksel.swap_rb(),
                          swap_ba ? "Alpha" : "Green", ksel.swap_ga(), even_stage,
                          ksel.kcsel_even(), ksel.kasel_even(), odd_stage, ksel.kcsel_odd(),
                          ksel.kasel_odd());
  }
};

struct AllTevKSels
{
  std::array<TevKSel, 8> ksel;

  KonstSel GetKonstColor(u32 tev_stage) const
  {
    const u32 ksel_num = tev_stage >> 1;
    const bool odd = tev_stage & 1;
    const auto& cur_ksel = ksel[ksel_num];
    return odd ? cur_ksel.kcsel_odd().Get() : cur_ksel.kcsel_even().Get();
  }
  KonstSel GetKonstAlpha(u32 tev_stage) const
  {
    const u32 ksel_num = tev_stage >> 1;
    const bool odd = tev_stage & 1;
    const auto& cur_ksel = ksel[ksel_num];
    return odd ? cur_ksel.kasel_odd().Get() : cur_ksel.kasel_even().Get();
  }
  Common::EnumMap<ColorChannel, ColorChannel::Alpha> GetSwapTable(u32 swap_table_id) const
  {
    const u32 rg_ksel_num = swap_table_id << 1;
    const u32 ba_ksel_num = rg_ksel_num + 1;
    const auto& rg_ksel = ksel[rg_ksel_num];
    const auto& ba_ksel = ksel[ba_ksel_num];
    return {rg_ksel.swap_rb(), rg_ksel.swap_ga(), ba_ksel.swap_rb(), ba_ksel.swap_ga()};
  }
};

enum class AlphaTestOp : u32
{
  And = 0,
  Or = 1,
  Xor = 2,
  Xnor = 3
};
template <>
struct fmt::formatter<AlphaTestOp> : EnumFormatter<AlphaTestOp::Xnor>
{
  constexpr formatter() : EnumFormatter({"And", "Or", "Xor", "Xnor"}) {}
};

enum class AlphaTestResult
{
  Undetermined = 0,
  Fail = 1,
  Pass = 2,
};

struct AlphaTest
{
  BFVIEW(u8, 8, 0, ref0)
  BFVIEW(u8, 8, 8, ref1)
  BFVIEW(CompareMode, 3, 16, comp0)
  BFVIEW(CompareMode, 3, 19, comp1)
  BFVIEW(AlphaTestOp, 2, 22, logic)

  u32 hex;

  DOLPHIN_FORCE_INLINE AlphaTestResult TestResult() const
  {
    switch (logic())
    {
    case AlphaTestOp::And:
      if (comp0() == CompareMode::Always && comp1() == CompareMode::Always)
        return AlphaTestResult::Pass;
      if (comp0() == CompareMode::Never || comp1() == CompareMode::Never)
        return AlphaTestResult::Fail;
      break;

    case AlphaTestOp::Or:
      if (comp0() == CompareMode::Always || comp1() == CompareMode::Always)
        return AlphaTestResult::Pass;
      if (comp0() == CompareMode::Never && comp1() == CompareMode::Never)
        return AlphaTestResult::Fail;
      break;

    case AlphaTestOp::Xor:
      if ((comp0() == CompareMode::Always && comp1() == CompareMode::Never) ||
          (comp0() == CompareMode::Never && comp1() == CompareMode::Always))
        return AlphaTestResult::Pass;
      if ((comp0() == CompareMode::Always && comp1() == CompareMode::Always) ||
          (comp0() == CompareMode::Never && comp1() == CompareMode::Never))
        return AlphaTestResult::Fail;
      break;

    case AlphaTestOp::Xnor:
      if ((comp0() == CompareMode::Always && comp1() == CompareMode::Never) ||
          (comp0() == CompareMode::Never && comp1() == CompareMode::Always))
        return AlphaTestResult::Fail;
      if ((comp0() == CompareMode::Always && comp1() == CompareMode::Always) ||
          (comp0() == CompareMode::Never && comp1() == CompareMode::Never))
        return AlphaTestResult::Pass;
      break;

    default:
      return AlphaTestResult::Undetermined;
    }
    return AlphaTestResult::Undetermined;
  }
};
template <>
struct fmt::formatter<AlphaTest>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const AlphaTest& test, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Test 1: {} (ref: 0x{:02x})\n"
                          "Test 2: {} (ref: 0x{:02x})\n"
                          "Logic: {}\n",
                          test.comp0(), test.ref0(), test.comp1(), test.ref1(), test.logic());
  }
};

enum class FrameToField : u32
{
  Progressive = 0,
  InterlacedEven = 2,
  InterlacedOdd = 3,
};
template <>
struct fmt::formatter<FrameToField> : EnumFormatter<FrameToField::InterlacedOdd>
{
  static constexpr array_type names = {"Progressive", nullptr, "Interlaced (even lines)",
                                       "Interlaced (odd lines)"};
  constexpr formatter() : EnumFormatter(names) {}
};

enum class GammaCorrection : u32
{
  Gamma1_0 = 0,
  Gamma1_7 = 1,
  Gamma2_2 = 2,
  // Hardware testing indicates this behaves the same as Gamma2_2
  Invalid2_2 = 3,
};
template <>
struct fmt::formatter<GammaCorrection> : EnumFormatter<GammaCorrection::Invalid2_2>
{
  constexpr formatter() : EnumFormatter({"1.0", "1.7", "2.2", "Invalid 2.2"}) {}
};

struct PE_Copy
{
  u32 Hex;

  BFVIEW(bool, 1, 0, clamp_top)     // if set clamp top
  BFVIEW(bool, 1, 1, clamp_bottom)  // if set clamp bottom
  BFVIEW(u32, 1, 2, unknown_bit)
  BFVIEW(u32, 4, 3, target_pixel_format)  // realformat is (fmt/2)+((fmt&1)*8).... for some reason
                                          // the msb is the lsb (pattern: cycling right shift)
  BFVIEW(GammaCorrection, 2, 7, gamma)
  // "mipmap" filter... false = no filter (scale 1:1) ; true = box filter (scale 2:1)
  BFVIEW(bool, 1, 9, half_scale)
  BFVIEW(bool, 1, 10, scale_invert)  // if set vertical scaling is on
  BFVIEW(bool, 1, 11, clear)
  BFVIEW(FrameToField, 2, 12, frame_to_field)
  BFVIEW(bool, 1, 14, copy_to_xfb)
  BFVIEW(bool, 1, 15, intensity_fmt)  // if set, is an intensity format (I4,I8,IA4,IA8)
  // if false automatic color conversion by texture format and pixel type
  BFVIEW(bool, 1, 16, auto_conv)

  EFBCopyFormat tp_realFormat() const
  {
    return static_cast<EFBCopyFormat>(target_pixel_format() / 2 + (target_pixel_format() & 1) * 8);
  }
};
template <>
struct fmt::formatter<PE_Copy>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const PE_Copy& copy, FormatContext& ctx) const
  {
    static constexpr std::array<const char*, 2> no_yes = {"No", "Yes"};
    std::string_view clamp;
    if (copy.clamp_top())
    {
      if (copy.clamp_bottom())
        clamp = "Top and Bottom";
      else
        clamp = "Top only";
    }
    else
    {
      if (copy.clamp_bottom())
        clamp = "Bottom only";
      else
        clamp = "None";
    }

    return fmt::format_to(ctx.out(),
                          "Clamping: {}\n"
                          "Unknown bit: {}\n"
                          "Target pixel format: {}\n"
                          "Gamma correction: {}\n"
                          "Half scale: {}\n"
                          "Vertical scaling: {}\n"
                          "Clear: {}\n"
                          "Frame to field: {}\n"
                          "Copy to XFB: {}\n"
                          "Intensity format: {}\n"
                          "Automatic color conversion: {}",
                          clamp, copy.unknown_bit(), copy.tp_realFormat(), copy.gamma(),
                          no_yes[copy.half_scale()], no_yes[copy.scale_invert()],
                          no_yes[copy.clear()], copy.frame_to_field(), no_yes[copy.copy_to_xfb()],
                          no_yes[copy.intensity_fmt()], no_yes[copy.auto_conv()]);
  }
};

struct CopyFilterCoefficients
{
  using Values = std::array<u8, 7>;

  u64 Hex;

  BFVIEW(u32, 32, 0, Low)
  BFVIEW(u8, 6, 0, w0)
  BFVIEW(u8, 6, 6, w1)
  BFVIEW(u8, 6, 12, w2)
  BFVIEW(u8, 6, 18, w3)
  BFVIEW(u32, 32, 32, High)
  BFVIEW(u8, 6, 32, w4)
  BFVIEW(u8, 6, 38, w5)
  BFVIEW(u8, 6, 44, w6)

  Values GetCoefficients() const
  {
    return {{
        static_cast<u8>(w0()),
        static_cast<u8>(w1()),
        static_cast<u8>(w2()),
        static_cast<u8>(w3()),
        static_cast<u8>(w4()),
        static_cast<u8>(w5()),
        static_cast<u8>(w6()),
    }};
  }
};

struct BPU_PreloadTileInfo
{
  BFVIEW(u32, 15, 0, count)
  BFVIEW(u32, 2, 15, type)
  u32 hex;
};
template <>
struct fmt::formatter<BPU_PreloadTileInfo>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const BPU_PreloadTileInfo& info, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Type: {}\nCount: {}", info.type(), info.count());
  }
};

struct BPS_TmemConfig
{
  u32 preload_addr;
  u32 preload_tmem_even;
  u32 preload_tmem_odd;
  BPU_PreloadTileInfo preload_tile_info;
  u32 tlut_src;
  u32 tlut_dest;
  u32 texinvalidate;
};

union AllTexUnits;

// The addressing of the texture units is a bit non-obvious.
// This struct abstracts the complexity away.
struct TexUnitAddress
{
  enum class Register : u32
  {
    SETMODE0 = 0,
    SETMODE1 = 1,
    SETIMAGE0 = 2,
    SETIMAGE1 = 3,
    SETIMAGE2 = 4,
    SETIMAGE3 = 5,
    SETTLUT = 6,
    UNKNOWN = 7,
  };

  BFVIEW(u32, 2, 0, UnitIdLow)
  BFVIEW(Register, 3, 2, Reg)
  BFVIEW(u32, 1, 5, UnitIdHigh)

  BFVIEW(u32, 6, 0, FullAddress)
  u32 hex;

  TexUnitAddress() : hex(0) {}
  TexUnitAddress(u32 unit_id, Register reg = Register::SETMODE0) : hex(0)
  {
    UnitIdLow() = unit_id & 3;
    UnitIdHigh() = unit_id >> 2;
    Reg() = reg;
  }

  static TexUnitAddress FromBPAddress(u32 Address)
  {
    TexUnitAddress Val;
    // Clear upper two bits (which should always be 0x80)
    Val.FullAddress() = Address & 0x3f;
    return Val;
  }

  u32 GetUnitID() const { return UnitIdLow() | (UnitIdHigh() << 2); }

private:
  friend AllTexUnits;

  size_t GetOffset() const { return FullAddress(); }
  size_t GetBPAddress() const { return FullAddress() | 0x80; }

  static constexpr size_t ComputeOffset(u32 unit_id)
  {
    // FIXME: Would be nice to construct a TexUnitAddress and get its offset,
    // but that doesn't seem to be possible in c++17

    // So we manually re-implement the calculation
    return (unit_id & 3) | ((unit_id & 4) << 3);
  }
};
static_assert(sizeof(TexUnitAddress) == sizeof(u32));

// A view of the registers of a single TexUnit
struct TexUnit
{
  TexMode0 texMode0;
  u32 : 32;  // doing u32 : 96 is legal according to the standard, but msvc
  u32 : 32;  // doesn't like it. So we stack multiple lines of u32 : 32;
  u32 : 32;
  TexMode1 texMode1;
  u32 : 32;
  u32 : 32;
  u32 : 32;
  TexImage0 texImage0;
  u32 : 32;
  u32 : 32;
  u32 : 32;
  TexImage1 texImage1;
  u32 : 32;
  u32 : 32;
  u32 : 32;
  TexImage2 texImage2;
  u32 : 32;
  u32 : 32;
  u32 : 32;
  TexImage3 texImage3;
  u32 : 32;
  u32 : 32;
  u32 : 32;
  TexTLUT texTlut;
  u32 : 32;
  u32 : 32;
  u32 : 32;
  u32 unknown;
};
static_assert(sizeof(TexUnit) == sizeof(u32) * 4 * 7 + sizeof(u32));

union AllTexUnits
{
  std::array<u32, 8 * 8> AllRegisters;

  const TexUnit& GetUnit(u32 UnitId) const
  {
    auto address = TexUnitAddress(UnitId);
    const u32* ptr = &AllRegisters[address.GetOffset()];
    return *reinterpret_cast<const TexUnit*>(ptr);
  }

private:
  // For debuggers since GetUnit can be optimised out in release builds
  template <u32 UnitId>
  struct TexUnitPadding
  {
    static_assert(UnitId != 0, "Can't use 0 as sizeof(std::array<u32, 0>) != 0");
    std::array<u32, TexUnitAddress::ComputeOffset(UnitId)> pad;
  };

  TexUnit tex0;
  struct
  {
    TexUnitPadding<1> pad1;
    TexUnit tex1;
  };
  struct
  {
    TexUnitPadding<2> pad2;
    TexUnit tex2;
  };
  struct
  {
    TexUnitPadding<3> pad3;
    TexUnit tex3;
  };
  struct
  {
    TexUnitPadding<4> pad4;
    TexUnit tex4;
  };
  struct
  {
    TexUnitPadding<5> pad5;
    TexUnit tex5;
  };
  struct
  {
    TexUnitPadding<6> pad6;
    TexUnit tex6;
  };
  struct
  {
    TexUnitPadding<7> pad7;
    TexUnit tex7;
  };
};
static_assert(sizeof(AllTexUnits) == 8 * 8 * sizeof(u32));

// All of BP memory

struct BPCmd
{
  int address;
  int changes;
  int newvalue;
};

enum class EmulatedZ : u32
{
  Disabled = 0,
  Early = 1,
  Late = 2,
  ForcedEarly = 3,
  EarlyWithFBFetch = 4,
  EarlyWithZComplocHack = 5,
};

struct BPMemory
{
  GenMode genMode;             // 0x00
  u32 display_copy_filter[4];  // 0x01-0x04
  u32 unknown;                 // 0x05
  // indirect matrices (set by GXSetIndTexMtx, selected by TevStageIndirect::matrix_index)
  // abc form a 2x3 offset matrix, there's 3 such matrices
  // the 3 offset matrices can either be indirect type, S-type, or T-type
  // 6bit scale factor s is distributed across IND_MTXA/B/C.
  // before using matrices scale by 2^-(s-17)
  IND_MTX indmtx[3];               // 0x06-0x0e: GXSetIndTexMtx, 2x3 matrices
  IND_IMASK imask;                 // 0x0f
  TevStageIndirect tevind[16];     // 0x10-0x1f: GXSetTevIndirect
  ScissorPos scissorTL;            // 0x20
  ScissorPos scissorBR;            // 0x21
  LPSize lineptwidth;              // 0x22
  u32 sucounter;                   // 0x23
  u32 rascounter;                  // 0x24
  TEXSCALE texscale[2];            // 0x25,0x26: GXSetIndTexCoordScale
  RAS1_IREF tevindref;             // 0x27: GXSetIndTexOrder
  TwoTevStageOrders tevorders[8];  // 0x28-0x2f
  TCoordInfo texcoords[8];         // 0x30-0x4f: s,t,s,t,s,t,s,t...
  ZMode zmode;                     // 0x40
  BlendMode blendmode;             // 0x41
  ConstantAlpha dstalpha;          // 0x42
  PEControl zcontrol;              // 0x43: GXSetZCompLoc, GXPixModeSync
  FieldMask fieldmask;             // 0x44
  u32 drawdone;                    // 0x45: bit1=1 if end of list
  u32 unknown5;                    // 0x46: clock?
  u32 petoken;                     // 0x47
  u32 petokenint;                  // 0x48
  X10Y10 copyTexSrcXY;             // 0x49
  X10Y10 copyTexSrcWH;             // 0x4a
  u32 copyTexDest;                 // 0x4b: CopyAddress (GXDispCopy and GXTexCopy use it)
  u32 unknown6;                    // 0x4c
  // usually set to 4 when dest is single channel, 8 when dest is 2 channel, 16 when dest is RGBA
  // also, doubles whenever mipmap box filter option is set (excent on RGBA). Probably to do
  // with number of bytes to look at when smoothing
  u32 copyMipMapStrideChannels;       // 0x4d
  u32 dispcopyyscale;                 // 0x4e
  u32 clearcolorAR;                   // 0x4f
  u32 clearcolorGB;                   // 0x50
  u32 clearZValue;                    // 0x51
  PE_Copy triggerEFBCopy;             // 0x52
  CopyFilterCoefficients copyfilter;  // 0x53,0x54
  u32 boundbox0;                      // 0x55
  u32 boundbox1;                      // 0x56
  u32 unknown7[2];                    // 0x57,0x58
  ScissorOffset scissorOffset;        // 0x59
  u32 unknown8[6];                    // 0x5a-0x5f
  BPS_TmemConfig tmem_config;         // 0x60-0x66
  u32 metric;                         // 0x67
  FieldMode fieldmode;                // 0x68
  u32 unknown10[7];                   // 0x69-0x6f
  u32 unknown11[16];                  // 0x70-0x7f
  AllTexUnits tex;                    // 0x80-0xbf
  TevStageCombiner combiners[16];     // 0xc0-0xdf
  TevReg tevregs[4];                  // 0xe0-0xe7
  FogRangeParams fogRange;            // 0xe8-0xed
  FogParams fog;                      // 0xee-0xf2
  AlphaTest alpha_test;               // 0xf3
  ZTex1 ztex1;                        // 0xf4
  ZTex2 ztex2;                        // 0xf5
  AllTevKSels tevksel;                // 0xf6-0xfd
  u32 bpMask;                         // 0xfe
  u32 unknown18;                      // 0xff

  EmulatedZ GetEmulatedZ() const
  {
    if (!zmode.testenable())
      return EmulatedZ::Disabled;
    if (zcontrol.early_ztest())
      return EmulatedZ::Early;
    else
      return EmulatedZ::Late;
  }
};

#pragma pack()

extern BPMemory bpmem;

void LoadBPReg(u8 reg, u32 value, int cycles_into_future);
void LoadBPRegPreprocess(u8 reg, u32 value, int cycles_into_future);

std::pair<std::string, std::string> GetBPRegInfo(u8 cmd, u32 cmddata);
