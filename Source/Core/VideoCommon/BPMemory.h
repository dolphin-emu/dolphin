// Copyright 2009 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <string>
#include <utility>

#include "Common/BitFieldView.h"
#include "Common/BitUtils.h"
#include "Common/CommonTypes.h"
#include "Common/EnumFormatter.h"
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
  u32 hex;

  BFVIEW_M(hex, s32, 0, 11, ma);
  BFVIEW_M(hex, s32, 11, 11, mb);
  BFVIEW_M(hex, u8, 22, 2, s0);  // bits 0-1 of scale factor
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
  u32 hex;

  BFVIEW_M(hex, s32, 0, 11, mc);
  BFVIEW_M(hex, s32, 11, 11, md);
  BFVIEW_M(hex, u8, 22, 2, s1);  // bits 2-3 of scale factor
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
  u32 hex;

  BFVIEW_M(hex, s32, 0, 11, me);
  BFVIEW_M(hex, s32, 11, 11, mf);
  BFVIEW_M(hex, u8, 22, 1, s2);  // bit 4 of scale factor
  // The SDK treats the scale factor as 6 bits, 2 on each column; however, hardware seems to ignore
  // the top bit.
  BFVIEW_M(hex, u8, 22, 2, sdk_s2);
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
  u32 hex;

  BFVIEW_M(hex, u32, 0, 24, mask);
};

struct TevStageCombiner
{
  struct ColorCombiner
  {
    u32 hex;

    // abc=8bit,d=10bit
    BFVIEW_M(hex, TevColorArg, 0, 4, d);
    BFVIEW_M(hex, TevColorArg, 4, 4, c);
    BFVIEW_M(hex, TevColorArg, 8, 4, b);
    BFVIEW_M(hex, TevColorArg, 12, 4, a);

    BFVIEW_M(hex, TevBias, 16, 2, bias);
    BFVIEW_M(hex, TevOp, 18, 1, op);                  // Applies when bias is not compare
    BFVIEW_M(hex, TevComparison, 18, 1, comparison);  // Applies when bias is compare
    BFVIEW_M(hex, bool, 19, 1, clamp);

    BFVIEW_M(hex, TevScale, 20, 2, scale);               // Applies when bias is not compare
    BFVIEW_M(hex, TevCompareMode, 20, 2, compare_mode);  // Applies when bias is compare
    BFVIEW_M(hex, TevOutput, 22, 2, dest);
  };
  struct AlphaCombiner
  {
    u32 hex;

    BFVIEW_M(hex, u32, 0, 2, rswap);
    BFVIEW_M(hex, u32, 2, 2, tswap);
    BFVIEW_M(hex, TevAlphaArg, 4, 3, d);
    BFVIEW_M(hex, TevAlphaArg, 7, 3, c);
    BFVIEW_M(hex, TevAlphaArg, 10, 3, b);
    BFVIEW_M(hex, TevAlphaArg, 13, 3, a);

    BFVIEW_M(hex, TevBias, 16, 2, bias);
    BFVIEW_M(hex, TevOp, 18, 1, op);                  // Applies when bias is not compare
    BFVIEW_M(hex, TevComparison, 18, 1, comparison);  // Applies when bias is compare
    BFVIEW_M(hex, bool, 19, 1, clamp);

    BFVIEW_M(hex, TevScale, 20, 2, scale);               // Applies when bias is not compare
    BFVIEW_M(hex, TevCompareMode, 20, 2, compare_mode);  // Applies when bias is compare
    BFVIEW_M(hex, TevOutput, 22, 2, dest);
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
        out = fmt::format_to(out, "{:n}.rgb = ", cc.dest().Get());

      if (has_scale)
        out = fmt::format_to(out, "(");
      if (has_d)
        out = fmt::format_to(out, "{}", alt_names[cc.d().Get()]);
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
            out = fmt::format_to(out, "({} + {})/2", alt_names[cc.a().Get()],
                                 alt_names[cc.b().Get()]);
          }
          else
          {
            out = fmt::format_to(out, "lerp({}, {}, {})", alt_names[cc.a().Get()],
                                 alt_names[cc.b().Get()], alt_names[cc.c().Get()]);
          }
        }
        else if (has_ac)
        {
          if (cc.c() == TevColorArg::Zero)
            out = fmt::format_to(out, "{}", alt_names[cc.a().Get()]);
          else if (cc.c() == TevColorArg::Half)  // 1 - .5 is .5
            out = fmt::format_to(out, ".5*{}", alt_names[cc.a().Get()]);
          else
            out = fmt::format_to(out, "(1 - {})*{}", alt_names[cc.c().Get()],
                                 alt_names[cc.a().Get()]);
        }
        else  // has_bc
        {
          if (cc.c() == TevColorArg::One)
            out = fmt::format_to(out, "{}", alt_names[cc.b().Get()]);
          else
            out = fmt::format_to(out, "{}*{}", alt_names[cc.c().Get()], alt_names[cc.b().Get()]);
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
        out = fmt::format_to(out, ") * {:n}", cc.scale().Get());
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
                          cc.a().Get(), cc.b().Get(), cc.c().Get(), cc.d().Get(), cc.bias().Get(),
                          cc.op().Get(), cc.comparison().Get(), cc.clamp() ? "Yes" : "No",
                          cc.scale().Get(), cc.compare_mode().Get(), cc.dest().Get());
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
        out = fmt::format_to(out, "{:n}.a = ", ac.dest().Get());

      if (has_scale)
        out = fmt::format_to(out, "(");
      if (has_d)
        out = fmt::format_to(out, "{:n}.a", ac.d().Get());
      if (has_ac || has_bc)
      {
        if (has_d)
          out = fmt::format_to(out, " {} ", op);
        else if (ac.op() == TevOp::Sub)
          out = fmt::format_to(out, "{}", op);
        if (has_ac && has_bc)
        {
          out = fmt::format_to(out, "lerp({:n}.a, {:n}.a, {:n}.a)", ac.a().Get(), ac.b().Get(),
                               ac.c().Get());
        }
        else if (has_ac)
        {
          if (ac.c() == TevAlphaArg::Zero)
            out = fmt::format_to(out, "{:n}.a", ac.a().Get());
          else
            out = fmt::format_to(out, "(1 - {:n}.a)*{:n}.a", ac.c().Get(), ac.a().Get());
        }
        else  // has_bc
        {
          out = fmt::format_to(out, "{:n}.a*{:n}.a", ac.c().Get(), ac.b().Get());
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
        out = fmt::format_to(out, ") * {:n}", ac.scale().Get());
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
                          "Ras sel: {}\n"
                          "Tex sel: {}",
                          ac.a().Get(), ac.b().Get(), ac.c().Get(), ac.d().Get(), ac.bias().Get(),
                          ac.op().Get(), ac.comparison().Get(), ac.clamp() ? "Yes" : "No",
                          ac.scale().Get(), ac.compare_mode().Get(), ac.dest().Get(), ac.rswap(),
                          ac.tswap());
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
  union
  {
    u32 fullhex;
    struct
    {
      u32 hex : 21;
      u32 unused : 11;
    };
  };

  // Indirect tex stage ID
  BFVIEW_M(fullhex, u32, 0, 2, bt);
  BFVIEW_M(fullhex, IndTexFormat, 2, 2, fmt);
  BFVIEW_M(fullhex, IndTexBias, 4, 3, bias);
  BFVIEW_M(fullhex, bool, 4, 1, bias_s);
  BFVIEW_M(fullhex, bool, 5, 1, bias_t);
  BFVIEW_M(fullhex, bool, 6, 1, bias_u);
  // Indicates which coordinate will become the 'bump alpha'
  BFVIEW_M(fullhex, IndTexBumpAlpha, 7, 2, bs);
  // Indicates which indirect matrix is used when matrix_id is Indirect.  Also always indicates
  // which indirect matrix to use for the scale factor, even with S or T.
  BFVIEW_M(fullhex, IndMtxIndex, 9, 2, matrix_index);
  // Should be set to Indirect (0) if matrix_index is Off (0)
  BFVIEW_M(fullhex, IndMtxId, 11, 2, matrix_id);
  // Wrapping factor for S of regular coord
  BFVIEW_M(fullhex, IndTexWrap, 13, 3, sw);
  // Wrapping factor for T of regular coord
  BFVIEW_M(fullhex, IndTexWrap, 16, 3, tw);
  // Use modified or unmodified texture coordinates for LOD computation
  BFVIEW_M(fullhex, bool, 19, 1, lb_utclod);
  // True if the texture coordinate results from the previous TEV stage should be added
  BFVIEW_M(fullhex, bool, 20, 1, fb_addprev);

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
                          tevind.bt(), tevind.fmt().Get(), tevind.bias().Get(), tevind.bs().Get(),
                          tevind.matrix_index().Get(), tevind.matrix_id().Get(), tevind.sw().Get(),
                          tevind.tw().Get(), tevind.lb_utclod() ? "Yes" : "No",
                          tevind.fb_addprev() ? "Yes" : "No");
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
  u32 hex;

  BFVIEW_M(hex, u32, 0, 3, texmap0);  // Indirect tex stage texmap
  BFVIEW_M(hex, u32, 3, 3, texcoord0);
  BFVIEW_M(hex, bool, 6, 1, enable0);  // true if should read from texture
  BFVIEW_M(hex, RasColorChan, 7, 3, colorchan0);

  BFVIEW_M(hex, u32, 12, 3, texmap1);
  BFVIEW_M(hex, u32, 15, 3, texcoord1);
  BFVIEW_M(hex, bool, 18, 1, enable1);  // true if should read from texture
  BFVIEW_M(hex, RasColorChan, 19, 3, colorchan1);

  u32 getTexMap(int i) const { return i ? texmap1() : texmap0(); }
  u32 getTexCoord(int i) const { return i ? texcoord1() : texcoord0(); }
  u32 getEnable(int i) const { return i ? enable1() : enable0(); }
  RasColorChan getColorChan(int i) const { return i ? colorchan1().Get() : colorchan0().Get(); }
};
template <>
struct fmt::formatter<TwoTevStageOrders>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TwoTevStageOrders& stages, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Stage 0 texmap: {}\nStage 0 tex coord: {}\n"
                          "Stage 0 enable texmap: {}\nStage 0 color channel: {}\n"
                          "Stage 1 texmap: {}\nStage 1 tex coord: {}\n"
                          "Stage 1 enable texmap: {}\nStage 1 color channel: {}\n",
                          stages.texmap0(), stages.texcoord0(), stages.enable0() ? "Yes" : "No",
                          stages.colorchan0().Get(), stages.texmap1(), stages.texcoord1(),
                          stages.enable1() ? "Yes" : "No", stages.colorchan1().Get());
  }
};

struct TEXSCALE
{
  u32 hex;

  BFVIEW_M(hex, u32, 0, 4, ss0);   // Indirect tex stage 0, 2^(-ss0)
  BFVIEW_M(hex, u32, 4, 4, ts0);   // Indirect tex stage 0
  BFVIEW_M(hex, u32, 8, 4, ss1);   // Indirect tex stage 1
  BFVIEW_M(hex, u32, 12, 4, ts1);  // Indirect tex stage 1
};
template <>
struct fmt::formatter<TEXSCALE>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TEXSCALE& scale, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Even stage S scale: {} ({})\n"
                          "Even stage T scale: {} ({})\n"
                          "Odd stage S scale: {} ({})\n"
                          "Odd stage T scale: {} ({})",
                          scale.ss0(), 1.f / (1 << scale.ss0()), scale.ts0(),
                          1.f / (1 << scale.ts0()), scale.ss1(), 1.f / (1 << scale.ss1()),
                          scale.ts1(), 1.f / (1 << scale.ts1()));
  }
};

struct RAS1_IREF
{
  u32 hex;

  BFVIEW_M(hex, u32, 0, 3, bi0);  // Indirect tex stage 0 ntexmap
  BFVIEW_M(hex, u32, 3, 3, bc0);  // Indirect tex stage 0 ntexcoord
  BFVIEW_M(hex, u32, 6, 3, bi1);
  BFVIEW_M(hex, u32, 9, 3, bc1);
  BFVIEW_M(hex, u32, 12, 3, bi2);
  BFVIEW_M(hex, u32, 15, 3, bc2);
  BFVIEW_M(hex, u32, 18, 3, bi3);
  BFVIEW_M(hex, u32, 21, 3, bc3);

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
    // The field names here are suspicious, since there is no bi3 or bc2
    return fmt::format_to(ctx.out(),
                          "Stage 0 ntexmap: {}\nStage 0 ntexcoord: {}\n"
                          "Stage 1 ntexmap: {}\nStage 1 ntexcoord: {}\n"
                          "Stage 2 ntexmap: {}\nStage 2 ntexcoord: {}\n"
                          "Stage 3 ntexmap: {}\nStage 3 ntexcoord: {}",
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
  u32 hex;

  BFVIEW_M(hex, WrapMode, 0, 2, wrap_s);
  BFVIEW_M(hex, WrapMode, 2, 2, wrap_t);
  BFVIEW_M(hex, FilterMode, 4, 1, mag_filter);
  BFVIEW_M(hex, MipMode, 5, 2, mipmap_filter);
  BFVIEW_M(hex, FilterMode, 7, 1, min_filter);
  BFVIEW_M(hex, LODType, 8, 1, diag_lod);
  BFVIEW_M(hex, s32, 9, 8, lod_bias);
  BFVIEW_M(hex, MaxAniso, 19, 2, max_aniso);
  BFVIEW_M(hex, bool, 21, 1, lod_clamp);
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
                          mode.wrap_s().Get(), mode.wrap_t().Get(), mode.mag_filter().Get(),
                          mode.mipmap_filter().Get(), mode.min_filter().Get(),
                          mode.diag_lod().Get(), mode.lod_bias(), mode.lod_bias() / 32.f,
                          mode.max_aniso().Get(), mode.lod_clamp() ? "Yes" : "No");
  }
};

struct TexMode1
{
  u32 hex;

  BFVIEW_M(hex, u8, 0, 8, min_lod);
  BFVIEW_M(hex, u8, 8, 8, max_lod);
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
  u32 hex;

  BFVIEW_M(hex, u32, 0, 10, width);    // Actually w-1
  BFVIEW_M(hex, u32, 10, 10, height);  // Actually h-1
  BFVIEW_M(hex, TextureFormat, 20, 4, format);
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
                          teximg.width() + 1, teximg.height() + 1, teximg.format().Get());
  }
};

struct TexImage1
{
  u32 hex;

  BFVIEW_M(hex, u32, 0, 15, tmem_even);  // TMEM line index for even LODs
  BFVIEW_M(hex, u32, 15, 3, cache_width);
  BFVIEW_M(hex, u32, 18, 3, cache_height);
  // true if this texture is managed manually (false means we'll
  // autofetch the texture data whenever it changes)
  BFVIEW_M(hex, bool, 21, 1, cache_manually_managed);
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
  u32 hex;

  BFVIEW_M(hex, u32, 0, 15, tmem_odd);  // tmem line index for odd LODs
  BFVIEW_M(hex, u32, 15, 3, cache_width);
  BFVIEW_M(hex, u32, 18, 3, cache_height);
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
  u32 hex;

  BFVIEW_M(hex, u32, 0, 24, image_base);  // address in memory >> 5 (was 20 for GC)
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
  u32 hex;

  BFVIEW_M(hex, u32, 0, 10, tmem_offset);
  BFVIEW_M(hex, TLUTFormat, 10, 2, tlut_format);
};
template <>
struct fmt::formatter<TexTLUT>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TexTLUT& tlut, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Address: {:08x}\nFormat: {}", tlut.tmem_offset() << 9,
                          tlut.tlut_format().Get());
  }
};

struct ZTex1
{
  u32 hex;

  BFVIEW_M(hex, u32, 0, 24, bias);
};

struct ZTex2
{
  u32 hex;

  BFVIEW_M(hex, ZTexFormat, 0, 2, type);
  BFVIEW_M(hex, ZTexOp, 2, 2, op);
};
template <>
struct fmt::formatter<ZTex2>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const ZTex2& ztex2, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Type: {}\nOperation: {}", ztex2.type().Get(),
                          ztex2.op().Get());
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
  u32 hex;

  BFVIEW_M(hex, u32, 0, 4, numtexgens);
  BFVIEW_M(hex, u32, 4, 3, numcolchans);
  BFVIEW_M(hex, u32, 7, 1, unused);         // 1 bit unused?
  BFVIEW_M(hex, bool, 8, 1, flat_shading);  // unconfirmed
  BFVIEW_M(hex, bool, 9, 1, multisampling);
  // This value is 1 less than the actual number (0-15 map to 1-16).
  // In other words there is always at least 1 tev stage
  BFVIEW_M(hex, u32, 10, 4, numtevstages);
  BFVIEW_M(hex, CullMode, 14, 2, cullmode);
  BFVIEW_M(hex, u32, 16, 3, numindstages);
  BFVIEW_M(hex, bool, 19, 1, zfreeze);
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
                          mode.numtevstages() + 1, mode.cullmode().Get(), mode.numindstages(),
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
  u32 hex;

  BFVIEW_M(hex, u32, 0, 8, linesize);   // in 1/6th pixels
  BFVIEW_M(hex, u32, 8, 8, pointsize);  // in 1/6th pixels
  BFVIEW_M(hex, u32, 16, 3, lineoff);
  BFVIEW_M(hex, u32, 19, 3, pointoff);
  // interlacing: adjust for pixels having AR of 1/2
  BFVIEW_M(hex, AspectRatioAdjustment, 22, 1, adjust_for_aspect_ratio);
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
                          lp.lineoff(), lp.pointoff(), lp.adjust_for_aspect_ratio().Get());
  }
};

struct ScissorPos
{
  u32 hex;

  // The top bit is ignored, and not included in the mask used by GX SDK functions
  // (though libogc includes it for the bottom coordinate (only) for some reason)
  // x_full and y_full include that bit for the FIFO analyzer, though it is usually unset.
  // The SDK also adds 342 to these values.
  BFVIEW_M(hex, u32, 0, 11, y);
  BFVIEW_M(hex, u32, 0, 12, y_full);
  BFVIEW_M(hex, u32, 12, 11, x);
  BFVIEW_M(hex, u32, 12, 12, x_full);
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
  u32 hex;

  // The scissor offset ignores the top bit (though it isn't masked off by the GX SDK).
  // Each value is also divided by 2 (so 0-511 map to 0-1022).
  // x_full and y_full include that top bit for the FIFO analyzer, though it is usually unset.
  // The SDK also adds 342 to each value (before dividing it).
  BFVIEW_M(hex, u32, 0, 9, x);
  BFVIEW_M(hex, u32, 0, 10, x_full);
  BFVIEW_M(hex, u32, 10, 9, y);
  BFVIEW_M(hex, u32, 10, 10, y_full);
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
  u32 hex;

  BFVIEW_M(hex, u32, 0, 10, x);
  BFVIEW_M(hex, u32, 10, 10, y);
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
  u32 hex;
  BFVIEW_M(hex, bool, 0, 1, blendenable);
  BFVIEW_M(hex, bool, 1, 1, logicopenable);
  BFVIEW_M(hex, bool, 2, 1, dither);
  BFVIEW_M(hex, bool, 3, 1, colorupdate);
  BFVIEW_M(hex, bool, 4, 1, alphaupdate);
  BFVIEW_M(hex, DstBlendFactor, 5, 3, dstfactor);
  BFVIEW_M(hex, SrcBlendFactor, 8, 3, srcfactor);
  BFVIEW_M(hex, bool, 11, 1, subtract);
  BFVIEW_M(hex, LogicOp, 12, 4, logicmode);

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
                          no_yes[mode.blendenable().Get()], no_yes[mode.logicopenable().Get()],
                          no_yes[mode.dither().Get()], no_yes[mode.colorupdate().Get()],
                          no_yes[mode.alphaupdate().Get()], mode.dstfactor().Get(),
                          mode.srcfactor().Get(), no_yes[mode.subtract().Get()],
                          mode.logicmode().Get());
  }
};

struct FogParam0
{
  u32 hex;

  BFVIEW_M(hex, u32, 0, 11, mant);
  BFVIEW_M(hex, u32, 11, 8, exp);
  BFVIEW_M(hex, bool, 19, 1, sign);

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
  u32 hex;

  BFVIEW_M(hex, u32, 0, 11, c_mant);
  BFVIEW_M(hex, u32, 11, 8, c_exp);
  BFVIEW_M(hex, bool, 19, 1, c_sign);
  BFVIEW_M(hex, FogProjection, 20, 1, proj);
  BFVIEW_M(hex, FogType, 21, 3, fsel);

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
        param.FloatValue(), param.c_mant(), param.c_exp(), param.c_sign() ? '-' : '+',
        param.proj().Get(), param.fsel().Get());
  }
};

struct FogRangeKElement
{
  u32 HEX;

  BFVIEW_M(HEX, u32, 0, 12, HI);
  BFVIEW_M(HEX, u32, 12, 12, LO);

  // TODO: Which scaling coefficient should we use here? This is just a guess!
  float GetValue(int i) const { return (i ? HI() : LO()) / 256.f; }
};

struct FogRangeParams
{
  struct RangeBase
  {
    u32 hex;

    BFVIEW_M(hex, u32, 0, 10, Center);  // viewport center + 342
    BFVIEW_M(hex, bool, 10, 1, Enabled);
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
    u32 hex;

    BFVIEW_M(hex, u8, 0, 8, b);
    BFVIEW_M(hex, u8, 8, 8, g);
    BFVIEW_M(hex, u8, 16, 8, r);
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
  u32 hex;

  BFVIEW_M(hex, bool, 0, 1, testenable);
  BFVIEW_M(hex, CompareMode, 1, 3, func);
  BFVIEW_M(hex, bool, 4, 1, updateenable);
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
                          mode.testenable() ? "Yes" : "No", mode.func().Get(),
                          mode.updateenable() ? "Yes" : "No");
  }
};

struct ConstantAlpha
{
  u32 hex;

  BFVIEW_M(hex, u32, 0, 8, alpha);
  BFVIEW_M(hex, bool, 8, 1, enable);
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
  u32 hex;

  // adjust vertex tex LOD computation to account for interlacing
  BFVIEW_M(hex, AspectRatioAdjustment, 0, 1, texLOD);
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
                          mode.texLOD().Get());
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
  u32 hex;

  // Fields are written to the EFB only if their bit is set to write.
  BFVIEW_M(hex, FieldMaskState, 0, 1, odd);
  BFVIEW_M(hex, FieldMaskState, 1, 1, even);
};
template <>
struct fmt::formatter<FieldMask>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const FieldMask& mask, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Odd field: {}\nEven field: {}", mask.odd().Get(),
                          mask.even().Get());
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
  u32 hex;

  BFVIEW_M(hex, PixelFormat, 0, 3, pixel_format);
  BFVIEW_M(hex, DepthFormat, 3, 3, zformat);
  BFVIEW_M(hex, bool, 6, 1, early_ztest);
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
                          config.pixel_format().Get(), config.zformat().Get(),
                          config.early_ztest() ? "Yes" : "No");
  }
};

// Texture coordinate stuff

struct TCInfo
{
  u32 hex;

  BFVIEW_M(hex, u32, 0, 16, scale_minus_1);
  BFVIEW_M(hex, bool, 16, 1, range_bias);
  BFVIEW_M(hex, bool, 17, 1, cylindric_wrap);
  // These bits only have effect in the s field of TCoordInfo
  BFVIEW_M(hex, bool, 18, 1, line_offset);
  BFVIEW_M(hex, bool, 19, 1, point_offset);
};
template <>
struct fmt::formatter<TCInfo>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TCInfo& info, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Scale: {}\n"
                          "Range bias: {}\n"
                          "Cylindric wrap: {}\n"
                          "Use line offset: {} (s only)\n"
                          "Use point offset: {} (s only)",
                          info.scale_minus_1() + 1, info.range_bias() ? "Yes" : "No",
                          info.cylindric_wrap() ? "Yes" : "No", info.line_offset() ? "Yes" : "No",
                          info.point_offset() ? "Yes" : "No");
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

    BFVIEW_M(hex, s32, 0, 11, red);
    BFVIEW_M(hex, s32, 12, 11, alpha);
    BFVIEW_M(hex, TevRegType, 23, 1, type);
  };
  struct BG
  {
    u32 hex;

    BFVIEW_M(hex, s32, 0, 11, blue);
    BFVIEW_M(hex, s32, 12, 11, green);
    BFVIEW_M(hex, TevRegType, 23, 1, type);
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
    return fmt::format_to(ctx.out(), "Type: {}\nAlpha: {:03x}\nRed: {:03x}", ra.type().Get(),
                          ra.alpha(), ra.red());
  }
};
template <>
struct fmt::formatter<TevReg::BG>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TevReg::BG& bg, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(), "Type: {}\nGreen: {:03x}\nBlue: {:03x}", bg.type().Get(),
                          bg.green(), bg.blue());
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
  u32 hex;

  BFVIEW_M(hex, u32, 0, 2, swap1);
  BFVIEW_M(hex, u32, 2, 2, swap2);
  BFVIEW_M(hex, KonstSel, 4, 5, kcsel0);
  BFVIEW_M(hex, KonstSel, 9, 5, kasel0);
  BFVIEW_M(hex, KonstSel, 14, 5, kcsel1);
  BFVIEW_M(hex, KonstSel, 19, 5, kasel1);

  KonstSel getKC(int i) const { return i ? kcsel1().Get() : kcsel0().Get(); }
  KonstSel getKA(int i) const { return i ? kasel1().Get() : kasel0().Get(); }
};
template <>
struct fmt::formatter<TevKSel>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename FormatContext>
  auto format(const TevKSel& ksel, FormatContext& ctx) const
  {
    return fmt::format_to(ctx.out(),
                          "Swap 1: {}\nSwap 2: {}\nColor sel 0: {}\nAlpha sel 0: {}\n"
                          "Color sel 1: {}\nAlpha sel 1: {}",
                          ksel.swap1(), ksel.swap2(), ksel.kcsel0().Get(), ksel.kasel0().Get(),
                          ksel.kcsel1().Get(), ksel.kasel1().Get());
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
  u32 hex;

  BFVIEW_M(hex, u8, 0, 8, ref0);
  BFVIEW_M(hex, u8, 8, 8, ref1);
  BFVIEW_M(hex, CompareMode, 16, 3, comp0);
  BFVIEW_M(hex, CompareMode, 19, 3, comp1);
  BFVIEW_M(hex, AlphaTestOp, 22, 2, logic);

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
                          test.comp0().Get(), test.ref0(), test.comp1().Get(), test.ref1(),
                          test.logic().Get());
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

struct PE_Copy
{
  u32 Hex;

  // if set clamp top
  BFVIEW_M(Hex, bool, 0, 1, clamp_top);
  // if set clamp bottom
  BFVIEW_M(Hex, bool, 1, 1, clamp_bottom);
  // if set, color conversion from RGB to YUV
  BFVIEW_M(Hex, bool, 2, 1, yuv);
  // realformat is (fmt/2)+((fmt&1)*8).... for some reason the msb is the lsb
  // (pattern: cycling right shift)
  BFVIEW_M(Hex, u32, 3, 4, target_pixel_format);
  // gamma correction.. 0 = 1.0 ; 1 = 1.7 ; 2 = 2.2 ; 3 is reserved
  BFVIEW_M(Hex, u32, 7, 2, gamma);
  // "mipmap" filter... false = no filter (scale 1:1) ; true = box filter (scale 2:1)
  BFVIEW_M(Hex, bool, 9, 1, half_scale);
  BFVIEW_M(Hex, bool, 10, 1, scale_invert);  // if set vertical scaling is on
  BFVIEW_M(Hex, bool, 11, 1, clear);
  BFVIEW_M(Hex, FrameToField, 12, 2, frame_to_field);
  BFVIEW_M(Hex, bool, 14, 1, copy_to_xfb);
  // if set, is an intensity format (I4,I8,IA4,IA8)
  BFVIEW_M(Hex, bool, 15, 1, intensity_fmt);
  // if false automatic color conversion by texture format and pixel type
  BFVIEW_M(Hex, bool, 16, 1, auto_conv);

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
    std::string_view gamma = "Invalid";
    switch (copy.gamma())
    {
    case 0:
      gamma = "1.0";
      break;
    case 1:
      gamma = "1.7";
      break;
    case 2:
      gamma = "2.2";
      break;
    }

    return fmt::format_to(
        ctx.out(),
        "Clamping: {}\n"
        "Converting from RGB to YUV: {}\n"
        "Target pixel format: {}\n"
        "Gamma correction: {}\n"
        "Half scale: {}\n"
        "Vertical scaling: {}\n"
        "Clear: {}\n"
        "Frame to field: {}\n"
        "Copy to XFB: {}\n"
        "Intensity format: {}\n"
        "Automatic color conversion: {}",
        clamp, no_yes[copy.yuv()], copy.tp_realFormat(), gamma, no_yes[copy.half_scale()],
        no_yes[copy.scale_invert()], no_yes[copy.clear()], copy.frame_to_field().Get(),
        no_yes[copy.copy_to_xfb()], no_yes[copy.intensity_fmt()], no_yes[copy.auto_conv()]);
  }
};

struct CopyFilterCoefficients
{
  using Values = std::array<u8, 7>;

  u64 hex;

  BFVIEW_M(hex, u8, 0, 6, w0);
  BFVIEW_M(hex, u8, 6, 6, w1);
  BFVIEW_M(hex, u8, 12, 6, w2);
  BFVIEW_M(hex, u8, 18, 6, w3);
  BFVIEW_M(hex, u8, 32, 6, w4);
  BFVIEW_M(hex, u8, 38, 6, w5);
  BFVIEW_M(hex, u8, 44, 6, w6);

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
  u32 hex;

  BFVIEW_M(hex, u32, 0, 15, count);
  BFVIEW_M(hex, u32, 15, 2, type);
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
  u32 hex;

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

  BFVIEW_M(hex, u32, 0, 2, UnitIdLow);
  BFVIEW_M(hex, Register, 2, 3, Reg);
  BFVIEW_M(hex, u32, 5, 1, UnitIdHigh);

  BFVIEW_M(hex, u32, 0, 6, FullAddress);

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

  TexUnitPadding<1> pad1;
  TexUnit tex1;

  TexUnitPadding<2> pad2;
  TexUnit tex2;

  TexUnitPadding<3> pad3;
  TexUnit tex3;

  TexUnitPadding<4> pad4;
  TexUnit tex4;

  TexUnitPadding<5> pad5;
  TexUnit tex5;

  TexUnitPadding<6> pad6;
  TexUnit tex6;

  TexUnitPadding<7> pad7;
  TexUnit tex7;
};
static_assert(sizeof(AllTexUnits) == 8 * 8 * sizeof(u32));

// All of BP memory

struct BPCmd
{
  int address;
  int changes;
  int newvalue;
};

struct BPMemory
{
  GenMode genMode;
  u32 display_copy_filter[4];  // 01-04
  u32 unknown;                 // 05
  // indirect matrices (set by GXSetIndTexMtx, selected by TevStageIndirect::matrix_index)
  // abc form a 2x3 offset matrix, there's 3 such matrices
  // the 3 offset matrices can either be indirect type, S-type, or T-type
  // 6bit scale factor s is distributed across IND_MTXA/B/C.
  // before using matrices scale by 2^-(s-17)
  IND_MTX indmtx[3];               // 06-0e GXSetIndTexMtx, 2x3 matrices
  IND_IMASK imask;                 // 0f
  TevStageIndirect tevind[16];     // 10 GXSetTevIndirect
  ScissorPos scissorTL;            // 20
  ScissorPos scissorBR;            // 21
  LPSize lineptwidth;              // 22 line and point width
  u32 sucounter;                   // 23
  u32 rascounter;                  // 24
  TEXSCALE texscale[2];            // 25-26 GXSetIndTexCoordScale
  RAS1_IREF tevindref;             // 27 GXSetIndTexOrder
  TwoTevStageOrders tevorders[8];  // 28-2F
  TCoordInfo texcoords[8];         // 0x30 s,t,s,t,s,t,s,t...
  ZMode zmode;                     // 40
  BlendMode blendmode;             // 41
  ConstantAlpha dstalpha;          // 42
  PEControl zcontrol;              // 43 GXSetZCompLoc, GXPixModeSync
  FieldMask fieldmask;             // 44
  u32 drawdone;                    // 45, bit1=1 if end of list
  u32 unknown5;                    // 46 clock?
  u32 petoken;                     // 47
  u32 petokenint;                  // 48
  X10Y10 copyTexSrcXY;             // 49
  X10Y10 copyTexSrcWH;             // 4a
  u32 copyTexDest;                 // 4b// 4b == CopyAddress (GXDispCopy and GXTexCopy use it)
  u32 unknown6;                    // 4c
  u32 copyMipMapStrideChannels;  // 4d usually set to 4 when dest is single channel, 8 when dest is
                                 // 2 channel, 16 when dest is RGBA
  // also, doubles whenever mipmap box filter option is set (excent on RGBA). Probably to do with
  // number of bytes to look at when smoothing
  u32 dispcopyyscale;                 // 4e
  u32 clearcolorAR;                   // 4f
  u32 clearcolorGB;                   // 50
  u32 clearZValue;                    // 51
  PE_Copy triggerEFBCopy;             // 52
  CopyFilterCoefficients copyfilter;  // 53,54
  u32 boundbox0;                      // 55
  u32 boundbox1;                      // 56
  u32 unknown7[2];                    // 57,58
  ScissorOffset scissorOffset;        // 59
  u32 unknown8[6];                    // 5a,5b,5c,5d, 5e,5f
  BPS_TmemConfig tmem_config;         // 60-66
  u32 metric;                         // 67
  FieldMode fieldmode;                // 68
  u32 unknown10[7];                   // 69-6F
  u32 unknown11[16];                  // 70-7F
  AllTexUnits tex;                    // 80-bf
  TevStageCombiner combiners[16];     // 0xC0-0xDF
  TevReg tevregs[4];                  // 0xE0
  FogRangeParams fogRange;            // 0xE8
  FogParams fog;                      // 0xEE,0xEF,0xF0,0xF1,0xF2
  AlphaTest alpha_test;               // 0xF3
  ZTex1 ztex1;                        // 0xf4,0xf5
  ZTex2 ztex2;
  TevKSel tevksel[8];  // 0xf6,0xf7,f8,f9,fa,fb,fc,fd
  u32 bpMask;          // 0xFE
  u32 unknown18;       // ff

  bool UseEarlyDepthTest() const { return zcontrol.early_ztest() && zmode.testenable(); }
  bool UseLateDepthTest() const { return !zcontrol.early_ztest() && zmode.testenable(); }
};

#pragma pack()

extern BPMemory bpmem;

void LoadBPReg(u8 reg, u32 value, int cycles_into_future);
void LoadBPRegPreprocess(u8 reg, u32 value, int cycles_into_future);

std::pair<std::string, std::string> GetBPRegInfo(u8 cmd, u32 cmddata);
