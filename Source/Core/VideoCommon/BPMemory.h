// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "Common/Compiler.h"

enum class EFBCopyFormat;

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
  BPMEM_EFB_BR = 0x4A,
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
enum : u32
{
  TEVSCALE_1 = 0,
  TEVSCALE_2 = 1,
  TEVSCALE_4 = 2,
  TEVDIVIDE_2 = 3
};

enum : u32
{
  TEVCMP_R8 = 0,
  TEVCMP_GR16 = 1,
  TEVCMP_BGR24 = 2,
  TEVCMP_RGB8 = 3
};

// TEV combiner operator
enum : u32
{
  TEVOP_ADD = 0,
  TEVOP_SUB = 1,
  TEVCMP_R8_GT = 8,
  TEVCMP_R8_EQ = 9,
  TEVCMP_GR16_GT = 10,
  TEVCMP_GR16_EQ = 11,
  TEVCMP_BGR24_GT = 12,
  TEVCMP_BGR24_EQ = 13,
  TEVCMP_RGB8_GT = 14,
  TEVCMP_RGB8_EQ = 15,
  TEVCMP_A8_GT = TEVCMP_RGB8_GT,
  TEVCMP_A8_EQ = TEVCMP_RGB8_EQ
};

// TEV color combiner input
enum : u32
{
  TEVCOLORARG_CPREV = 0,
  TEVCOLORARG_APREV = 1,
  TEVCOLORARG_C0 = 2,
  TEVCOLORARG_A0 = 3,
  TEVCOLORARG_C1 = 4,
  TEVCOLORARG_A1 = 5,
  TEVCOLORARG_C2 = 6,
  TEVCOLORARG_A2 = 7,
  TEVCOLORARG_TEXC = 8,
  TEVCOLORARG_TEXA = 9,
  TEVCOLORARG_RASC = 10,
  TEVCOLORARG_RASA = 11,
  TEVCOLORARG_ONE = 12,
  TEVCOLORARG_HALF = 13,
  TEVCOLORARG_KONST = 14,
  TEVCOLORARG_ZERO = 15
};

// TEV alpha combiner input
enum : u32
{
  TEVALPHAARG_APREV = 0,
  TEVALPHAARG_A0 = 1,
  TEVALPHAARG_A1 = 2,
  TEVALPHAARG_A2 = 3,
  TEVALPHAARG_TEXA = 4,
  TEVALPHAARG_RASA = 5,
  TEVALPHAARG_KONST = 6,
  TEVALPHAARG_ZERO = 7
};

// TEV output registers
enum : u32
{
  GX_TEVPREV = 0,
  GX_TEVREG0 = 1,
  GX_TEVREG1 = 2,
  GX_TEVREG2 = 3
};

// Z-texture formats
enum : u32
{
  TEV_ZTEX_TYPE_U8 = 0,
  TEV_ZTEX_TYPE_U16 = 1,
  TEV_ZTEX_TYPE_U24 = 2
};

// Z texture operator
enum : u32
{
  ZTEXTURE_DISABLE = 0,
  ZTEXTURE_ADD = 1,
  ZTEXTURE_REPLACE = 2
};

// TEV bias value
enum : u32
{
  TEVBIAS_ZERO = 0,
  TEVBIAS_ADDHALF = 1,
  TEVBIAS_SUBHALF = 2,
  TEVBIAS_COMPARE = 3
};

// Indirect texture format
enum : u32
{
  ITF_8 = 0,
  ITF_5 = 1,
  ITF_4 = 2,
  ITF_3 = 3
};

// Indirect texture bias
enum : u32
{
  ITB_NONE = 0,
  ITB_S = 1,
  ITB_T = 2,
  ITB_ST = 3,
  ITB_U = 4,
  ITB_SU = 5,
  ITB_TU = 6,
  ITB_STU = 7
};

// Indirect texture bump alpha
enum : u32
{
  ITBA_OFF = 0,
  ITBA_S = 1,
  ITBA_T = 2,
  ITBA_U = 3
};

// Indirect texture wrap value
enum : u32
{
  ITW_OFF = 0,
  ITW_256 = 1,
  ITW_128 = 2,
  ITW_64 = 3,
  ITW_32 = 4,
  ITW_16 = 5,
  ITW_0 = 6
};

union IND_MTXA
{
  struct
  {
    s32 ma : 11;
    s32 mb : 11;
    u32 s0 : 2;  // bits 0-1 of scale factor
    u32 rid : 8;
  };
  u32 hex;
};

union IND_MTXB
{
  struct
  {
    s32 mc : 11;
    s32 md : 11;
    u32 s1 : 2;  // bits 2-3 of scale factor
    u32 rid : 8;
  };
  u32 hex;
};

union IND_MTXC
{
  struct
  {
    s32 me : 11;
    s32 mf : 11;
    u32 s2 : 2;  // bits 4-5 of scale factor
    u32 rid : 8;
  };
  u32 hex;
};

struct IND_MTX
{
  IND_MTXA col0;
  IND_MTXB col1;
  IND_MTXC col2;
};

union IND_IMASK
{
  struct
  {
    u32 mask : 24;
    u32 rid : 8;
  };
  u32 hex;
};

struct TevStageCombiner
{
  union ColorCombiner
  {
    // abc=8bit,d=10bit
    BitField<0, 4, u32> d;   // TEVSELCC_X
    BitField<4, 4, u32> c;   // TEVSELCC_X
    BitField<8, 4, u32> b;   // TEVSELCC_X
    BitField<12, 4, u32> a;  // TEVSELCC_X

    BitField<16, 2, u32> bias;
    BitField<18, 1, u32> op;
    BitField<19, 1, u32> clamp;

    BitField<20, 2, u32> shift;
    BitField<22, 2, u32> dest;  // 1,2,3

    u32 hex;
  };
  union AlphaCombiner
  {
    BitField<0, 2, u32> rswap;
    BitField<2, 2, u32> tswap;
    BitField<4, 3, u32> d;   // TEVSELCA_
    BitField<7, 3, u32> c;   // TEVSELCA_
    BitField<10, 3, u32> b;  // TEVSELCA_
    BitField<13, 3, u32> a;  // TEVSELCA_

    BitField<16, 2, u32> bias;  // GXTevBias
    BitField<18, 1, u32> op;
    BitField<19, 1, u32> clamp;

    BitField<20, 2, u32> shift;
    BitField<22, 2, u32> dest;  // 1,2,3

    u32 hex;
  };

  ColorCombiner colorC;
  AlphaCombiner alphaC;
};

// several discoveries:
// GXSetTevIndBumpST(tevstage, indstage, matrixind)
//  if ( matrix == 2 ) realmat = 6; // 10
//  else if ( matrix == 3 ) realmat = 7; // 11
//  else if ( matrix == 1 ) realmat = 5; // 9
//  GXSetTevIndirect(tevstage, indstage, 0, 3, realmat, 6, 6, 0, 0, 0)
//  GXSetTevIndirect(tevstage+1, indstage, 0, 3, realmat+4, 6, 6, 1, 0, 0)
//  GXSetTevIndirect(tevstage+2, indstage, 0, 0, 0, 0, 0, 1, 0, 0)

union TevStageIndirect
{
  BitField<0, 2, u32> bt;    // Indirect tex stage ID
  BitField<2, 2, u32> fmt;   // Format: ITF_X
  BitField<4, 3, u32> bias;  // ITB_X
  BitField<7, 2, u32> bs;    // ITBA_X, indicates which coordinate will become the 'bump alpha'
  BitField<9, 4, u32> mid;   // Matrix ID to multiply offsets with
  BitField<13, 3, u32> sw;   // ITW_X, wrapping factor for S of regular coord
  BitField<16, 3, u32> tw;   // ITW_X, wrapping factor for T of regular coord
  BitField<19, 1, u32> lb_utclod;   // Use modified or unmodified texture
                                    // coordinates for LOD computation
  BitField<20, 1, u32> fb_addprev;  // 1 if the texture coordinate results from the previous TEV
                                    // stage should be added

  struct
  {
    u32 hex : 21;
    u32 unused : 11;
  };

  // If bs and mid are zero, the result of the stage is independent of
  // the texture sample data, so we can skip sampling the texture.
  bool IsActive() const { return bs != ITBA_OFF || mid != 0; }
};

union TwoTevStageOrders
{
  BitField<0, 3, u32> texmap0;  // Indirect tex stage texmap
  BitField<3, 3, u32> texcoord0;
  BitField<6, 1, u32> enable0;     // 1 if should read from texture
  BitField<7, 3, u32> colorchan0;  // RAS1_CC_X

  BitField<12, 3, u32> texmap1;
  BitField<15, 3, u32> texcoord1;
  BitField<18, 1, u32> enable1;     // 1 if should read from texture
  BitField<19, 3, u32> colorchan1;  // RAS1_CC_X

  BitField<24, 8, u32> rid;

  u32 hex;
  u32 getTexMap(int i) const { return i ? texmap1.Value() : texmap0.Value(); }
  u32 getTexCoord(int i) const { return i ? texcoord1.Value() : texcoord0.Value(); }
  u32 getEnable(int i) const { return i ? enable1.Value() : enable0.Value(); }
  u32 getColorChan(int i) const { return i ? colorchan1.Value() : colorchan0.Value(); }
};

union TEXSCALE
{
  struct
  {
    u32 ss0 : 4;  // Indirect tex stage 0, 2^(-ss0)
    u32 ts0 : 4;  // Indirect tex stage 0
    u32 ss1 : 4;  // Indirect tex stage 1
    u32 ts1 : 4;  // Indirect tex stage 1
    u32 pad : 8;
    u32 rid : 8;
  };
  u32 hex;
};

union RAS1_IREF
{
  struct
  {
    u32 bi0 : 3;  // Indirect tex stage 0 ntexmap
    u32 bc0 : 3;  // Indirect tex stage 0 ntexcoord
    u32 bi1 : 3;
    u32 bc1 : 3;
    u32 bi2 : 3;
    u32 bc3 : 3;
    u32 bi4 : 3;
    u32 bc4 : 3;
    u32 rid : 8;
  };
  u32 hex;

  u32 getTexCoord(int i) const { return (hex >> (6 * i + 3)) & 7; }
  u32 getTexMap(int i) const { return (hex >> (6 * i)) & 7; }
};

// Texture structs

union TexMode0
{
  enum TextureFilter : u32
  {
    TEXF_NONE = 0,
    TEXF_POINT = 1,
    TEXF_LINEAR = 2
  };

  struct
  {
    u32 wrap_s : 2;
    u32 wrap_t : 2;
    u32 mag_filter : 1;
    u32 min_filter : 3;
    u32 diag_lod : 1;
    s32 lod_bias : 8;
    u32 pad0 : 2;
    u32 max_aniso : 2;
    u32 lod_clamp : 1;
  };
  u32 hex;
};
union TexMode1
{
  struct
  {
    u32 min_lod : 8;
    u32 max_lod : 8;
  };
  u32 hex;
};
union TexImage0
{
  struct
  {
    u32 width : 10;   // Actually w-1
    u32 height : 10;  // Actually h-1
    u32 format : 4;
  };
  u32 hex;
};
union TexImage1
{
  struct
  {
    u32 tmem_even : 15;  // TMEM line index for even LODs
    u32 cache_width : 3;
    u32 cache_height : 3;
    u32 image_type : 1;  // 1 if this texture is managed manually (0 means we'll autofetch the
                         // texture data whenever it changes)
  };
  u32 hex;
};

union TexImage2
{
  struct
  {
    u32 tmem_odd : 15;  // tmem line index for odd LODs
    u32 cache_width : 3;
    u32 cache_height : 3;
  };
  u32 hex;
};

union TexImage3
{
  struct
  {
    u32 image_base : 24;  // address in memory >> 5 (was 20 for GC)
  };
  u32 hex;
};
union TexTLUT
{
  struct
  {
    u32 tmem_offset : 10;
    u32 tlut_format : 2;
  };
  u32 hex;
};

union ZTex1
{
  BitField<0, 24, u32> bias;
  u32 hex;
};

union ZTex2
{
  BitField<0, 2, u32> type;  // TEV_Z_TYPE_X
  BitField<2, 2, u32> op;    // GXZTexOp
  u32 hex;
};

struct FourTexUnits
{
  TexMode0 texMode0[4];
  TexMode1 texMode1[4];
  TexImage0 texImage0[4];
  TexImage1 texImage1[4];
  TexImage2 texImage2[4];
  TexImage3 texImage3[4];
  TexTLUT texTlut[4];
  u32 unknown[4];
};

// Geometry/other structs

union GenMode
{
  enum CullMode : u32
  {
    CULL_NONE = 0,
    CULL_BACK = 1,   // cull back-facing primitives
    CULL_FRONT = 2,  // cull front-facing primitives
    CULL_ALL = 3,    // cull all primitives
  };

  BitField<0, 4, u32> numtexgens;
  BitField<4, 3, u32> numcolchans;
  // 1 bit unused?
  BitField<8, 1, u32> flat_shading;  // unconfirmed
  BitField<9, 1, u32> multisampling;
  BitField<10, 4, u32> numtevstages;
  BitField<14, 2, CullMode> cullmode;
  BitField<16, 3, u32> numindstages;
  BitField<19, 1, u32> zfreeze;

  u32 hex;
};

union LPSize
{
  struct
  {
    u32 linesize : 8;   // in 1/6th pixels
    u32 pointsize : 8;  // in 1/6th pixels
    u32 lineoff : 3;
    u32 pointoff : 3;
    u32 lineaspect : 1;  // interlacing: adjust for pixels having AR of 1/2
    u32 padding : 1;
  };
  u32 hex;
};

union X12Y12
{
  struct
  {
    u32 y : 12;
    u32 x : 12;
  };
  u32 hex;
};
union X10Y10
{
  struct
  {
    u32 x : 10;
    u32 y : 10;
  };
  u32 hex;
};

// Framebuffer/pixel stuff (incl fog)

union BlendMode
{
  enum BlendFactor : u32
  {
    ZERO = 0,
    ONE = 1,
    SRCCLR = 2,             // for dst factor
    INVSRCCLR = 3,          // for dst factor
    DSTCLR = SRCCLR,        // for src factor
    INVDSTCLR = INVSRCCLR,  // for src factor
    SRCALPHA = 4,
    INVSRCALPHA = 5,
    DSTALPHA = 6,
    INVDSTALPHA = 7
  };

  enum LogicOp : u32
  {
    CLEAR = 0,
    AND = 1,
    AND_REVERSE = 2,
    COPY = 3,
    AND_INVERTED = 4,
    NOOP = 5,
    XOR = 6,
    OR = 7,
    NOR = 8,
    EQUIV = 9,
    INVERT = 10,
    OR_REVERSE = 11,
    COPY_INVERTED = 12,
    OR_INVERTED = 13,
    NAND = 14,
    SET = 15
  };

  BitField<0, 1, u32> blendenable;
  BitField<1, 1, u32> logicopenable;
  BitField<2, 1, u32> dither;
  BitField<3, 1, u32> colorupdate;
  BitField<4, 1, u32> alphaupdate;
  BitField<5, 3, BlendFactor> dstfactor;
  BitField<8, 3, BlendFactor> srcfactor;
  BitField<11, 1, u32> subtract;
  BitField<12, 4, LogicOp> logicmode;

  u32 hex;

  bool UseLogicOp() const;
};

union FogParam0
{
  BitField<0, 11, u32> mant;
  BitField<11, 8, u32> exp;
  BitField<19, 1, u32> sign;

  u32 hex;
};

union FogParam3
{
  BitField<0, 11, u32> c_mant;
  BitField<11, 8, u32> c_exp;
  BitField<19, 1, u32> c_sign;
  BitField<20, 1, u32> proj;  // 0 - perspective, 1 - orthographic
  BitField<21, 3, u32> fsel;  // 0 - off, 2 - linear, 4 - exp, 5 - exp2, 6 -
                              // backward exp, 7 - backward exp2

  u32 hex;
};

union FogRangeKElement
{
  BitField<0, 12, u32> HI;
  BitField<12, 12, u32> LO;
  BitField<24, 8, u32> regid;

  // TODO: Which scaling coefficient should we use here? This is just a guess!
  float GetValue(int i) const { return (i ? HI.Value() : LO.Value()) / 256.f; }
  u32 HEX;
};

struct FogRangeParams
{
  union RangeBase
  {
    BitField<0, 10, u32> Center;  // viewport center + 342
    BitField<10, 1, u32> Enabled;
    BitField<24, 8, u32> regid;
    u32 hex;
  };
  RangeBase Base;
  FogRangeKElement K[5];
};
// final eq: ze = A/(B_MAG - (Zs>>B_SHF));
struct FogParams
{
  FogParam0 a;
  u32 b_magnitude;
  u32 b_shift;  // b's exp + 1?
  FogParam3 c_proj_fsel;

  union FogColor
  {
    BitField<0, 8, u32> b;
    BitField<8, 8, u32> g;
    BitField<16, 8, u32> r;
    u32 hex;
  };

  FogColor color;  // 0:b 8:g 16:r - nice!

  // Special case where a and c are infinite and the sign matches, resulting in a result of NaN.
  bool IsNaNCase() const;
  float GetA() const;

  // amount to subtract from eyespacez after range adjustment
  float GetC() const;
};

union ZMode
{
  enum CompareMode : u32
  {
    NEVER = 0,
    LESS = 1,
    EQUAL = 2,
    LEQUAL = 3,
    GREATER = 4,
    NEQUAL = 5,
    GEQUAL = 6,
    ALWAYS = 7
  };

  BitField<0, 1, u32> testenable;
  BitField<1, 3, CompareMode> func;
  BitField<4, 1, u32> updateenable;

  u32 hex;
};

union ConstantAlpha
{
  BitField<0, 8, u32> alpha;
  BitField<8, 1, u32> enable;
  u32 hex;
};

union FieldMode
{
  struct
  {
    u32 texLOD : 1;  // adjust vert tex LOD computation to account for interlacing
  };
  u32 hex;
};

union FieldMask
{
  struct
  {
    // If bit is not set, do not write field to EFB
    u32 odd : 1;
    u32 even : 1;
  };
  u32 hex;
};

union PEControl
{
  enum PixelFormat : u32
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

  enum DepthFormat : u32
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

  BitField<0, 3, PixelFormat> pixel_format;
  BitField<3, 3, DepthFormat> zformat;
  BitField<6, 1, u32> early_ztest;

  u32 hex;
};

// Texture coordinate stuff

union TCInfo
{
  struct
  {
    u32 scale_minus_1 : 16;
    u32 range_bias : 1;
    u32 cylindric_wrap : 1;
    // These bits only have effect in the s field of TCoordInfo
    u32 line_offset : 1;
    u32 point_offset : 1;
  };
  u32 hex;
};
struct TCoordInfo
{
  TCInfo s;
  TCInfo t;
};

union TevReg
{
  u64 hex;

  // Access to individual registers
  BitField<0, 32, u64> low;
  BitField<32, 32, u64> high;

  // TODO: Check if Konst uses all 11 bits or just 8

  // Low register
  BitField<0, 11, s64> red;

  BitField<12, 11, s64> alpha;
  BitField<23, 1, u64> type_ra;

  // High register
  BitField<32, 11, s64> blue;

  BitField<44, 11, s64> green;
  BitField<55, 1, u64> type_bg;
};

union TevKSel
{
  BitField<0, 2, u32> swap1;
  BitField<2, 2, u32> swap2;
  BitField<4, 5, u32> kcsel0;
  BitField<9, 5, u32> kasel0;
  BitField<14, 5, u32> kcsel1;
  BitField<19, 5, u32> kasel1;
  u32 hex;

  u32 getKC(int i) const { return i ? kcsel1.Value() : kcsel0.Value(); }
  u32 getKA(int i) const { return i ? kasel1.Value() : kasel0.Value(); }
};

union AlphaTest
{
  enum CompareMode : u32
  {
    NEVER = 0,
    LESS = 1,
    EQUAL = 2,
    LEQUAL = 3,
    GREATER = 4,
    NEQUAL = 5,
    GEQUAL = 6,
    ALWAYS = 7
  };

  enum Op : u32
  {
    AND = 0,
    OR = 1,
    XOR = 2,
    XNOR = 3
  };

  BitField<0, 8, u32> ref0;
  BitField<8, 8, u32> ref1;
  BitField<16, 3, CompareMode> comp0;
  BitField<19, 3, CompareMode> comp1;
  BitField<22, 2, Op> logic;

  u32 hex;

  enum TEST_RESULT
  {
    UNDETERMINED = 0,
    FAIL = 1,
    PASS = 2,
  };

  DOLPHIN_FORCE_INLINE TEST_RESULT TestResult() const
  {
    switch (logic)
    {
    case AND:
      if (comp0 == ALWAYS && comp1 == ALWAYS)
        return PASS;
      if (comp0 == NEVER || comp1 == NEVER)
        return FAIL;
      break;

    case OR:
      if (comp0 == ALWAYS || comp1 == ALWAYS)
        return PASS;
      if (comp0 == NEVER && comp1 == NEVER)
        return FAIL;
      break;

    case XOR:
      if ((comp0 == ALWAYS && comp1 == NEVER) || (comp0 == NEVER && comp1 == ALWAYS))
        return PASS;
      if ((comp0 == ALWAYS && comp1 == ALWAYS) || (comp0 == NEVER && comp1 == NEVER))
        return FAIL;
      break;

    case XNOR:
      if ((comp0 == ALWAYS && comp1 == NEVER) || (comp0 == NEVER && comp1 == ALWAYS))
        return FAIL;
      if ((comp0 == ALWAYS && comp1 == ALWAYS) || (comp0 == NEVER && comp1 == NEVER))
        return PASS;
      break;

    default:
      return UNDETERMINED;
    }
    return UNDETERMINED;
  }
};

union UPE_Copy
{
  u32 Hex;

  BitField<0, 1, u32> clamp_top;            // if set clamp top
  BitField<1, 1, u32> clamp_bottom;         // if set clamp bottom
  BitField<2, 1, u32> yuv;                  // if set, color conversion from RGB to YUV
  BitField<3, 4, u32> target_pixel_format;  // realformat is (fmt/2)+((fmt&1)*8).... for some reason
                                            // the msb is the lsb (pattern: cycling right shift)
  BitField<7, 2, u32> gamma;  // gamma correction.. 0 = 1.0 ; 1 = 1.7 ; 2 = 2.2 ; 3 is reserved
  BitField<9, 1, u32>
      half_scale;  // "mipmap" filter... 0 = no filter (scale 1:1) ; 1 = box filter (scale 2:1)
  BitField<10, 1, u32> scale_invert;  // if set vertical scaling is on
  BitField<11, 1, u32> clear;
  BitField<12, 2, u32> frame_to_field;  // 0 progressive ; 1 is reserved ; 2 = interlaced (even
                                        // lines) ; 3 = interlaced 1 (odd lines)
  BitField<14, 1, u32> copy_to_xfb;
  BitField<15, 1, u32> intensity_fmt;  // if set, is an intensity format (I4,I8,IA4,IA8)
  BitField<16, 1, u32>
      auto_conv;  // if 0 automatic color conversion by texture format and pixel type

  EFBCopyFormat tp_realFormat() const
  {
    return static_cast<EFBCopyFormat>(target_pixel_format / 2 + (target_pixel_format & 1) * 8);
  }
};

union CopyFilterCoefficients
{
  using Values = std::array<u8, 7>;

  u64 Hex;

  BitField<0, 6, u64> w0;
  BitField<6, 6, u64> w1;
  BitField<12, 6, u64> w2;
  BitField<18, 6, u64> w3;
  BitField<32, 6, u64> w4;
  BitField<38, 6, u64> w5;
  BitField<44, 6, u64> w6;

  Values GetCoefficients() const
  {
    return {{
        static_cast<u8>(w0),
        static_cast<u8>(w1),
        static_cast<u8>(w2),
        static_cast<u8>(w3),
        static_cast<u8>(w4),
        static_cast<u8>(w5),
        static_cast<u8>(w6),
    }};
  }
};

union BPU_PreloadTileInfo
{
  u32 hex;
  struct
  {
    u32 count : 15;
    u32 type : 2;
  };
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

// All of BP memory

struct BPCmd
{
  u32 address;
  u32 changes;
  u32 newvalue;
};

struct BPMemory
{
  GenMode genMode;
  u32 display_copy_filter[4];  // 01-04
  u32 unknown;                 // 05
  // indirect matrices (set by GXSetIndTexMtx, selected by TevStageIndirect::mid)
  // abc form a 2x3 offset matrix, there's 3 such matrices
  // the 3 offset matrices can either be indirect type, S-type, or T-type
  // 6bit scale factor s is distributed across IND_MTXA/B/C.
  // before using matrices scale by 2^-(s-17)
  IND_MTX indmtx[3];               // 06-0e GXSetIndTexMtx, 2x3 matrices
  IND_IMASK imask;                 // 0f
  TevStageIndirect tevind[16];     // 10 GXSetTevIndirect
  X12Y12 scissorTL;                // 20
  X12Y12 scissorBR;                // 21
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
  UPE_Copy triggerEFBCopy;            // 52
  CopyFilterCoefficients copyfilter;  // 53,54
  u32 boundbox0;                      // 55
  u32 boundbox1;                      // 56
  u32 unknown7[2];                    // 57,58
  X10Y10 scissorOffset;               // 59
  u32 unknown8[6];                    // 5a,5b,5c,5d, 5e,5f
  BPS_TmemConfig tmem_config;         // 60-66
  u32 metric;                         // 67
  FieldMode fieldmode;                // 68
  u32 unknown10[7];                   // 69-6F
  u32 unknown11[16];                  // 70-7F
  FourTexUnits tex[2];                // 80-bf
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
};

#pragma pack()

extern BPMemory bpmem;

void LoadBPReg(u32 value0);
void LoadBPRegPreprocess(u32 value0);

void GetBPRegInfo(const u8* data, std::string* name, std::string* desc);
