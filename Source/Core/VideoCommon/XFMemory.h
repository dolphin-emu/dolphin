// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "VideoCommon/CPMemory.h"

class DataReader;

// Lighting

// Projection
enum : u32
{
  XF_TEXPROJ_ST = 0,
  XF_TEXPROJ_STQ = 1
};

// Input form
enum : u32
{
  XF_TEXINPUT_AB11 = 0,
  XF_TEXINPUT_ABC1 = 1
};

// Texture generation type
enum : u32
{
  XF_TEXGEN_REGULAR = 0,
  XF_TEXGEN_EMBOSS_MAP = 1,  // Used when bump mapping
  XF_TEXGEN_COLOR_STRGBC0 = 2,
  XF_TEXGEN_COLOR_STRGBC1 = 3
};

// Source row
enum : u32
{
  XF_SRCGEOM_INROW = 0,    // Input is abc
  XF_SRCNORMAL_INROW = 1,  // Input is abc
  XF_SRCCOLORS_INROW = 2,
  XF_SRCBINORMAL_T_INROW = 3,  // Input is abc
  XF_SRCBINORMAL_B_INROW = 4,  // Input is abc
  XF_SRCTEX0_INROW = 5,
  XF_SRCTEX1_INROW = 6,
  XF_SRCTEX2_INROW = 7,
  XF_SRCTEX3_INROW = 8,
  XF_SRCTEX4_INROW = 9,
  XF_SRCTEX5_INROW = 10,
  XF_SRCTEX6_INROW = 11,
  XF_SRCTEX7_INROW = 12
};

// Control source
enum : u32
{
  GX_SRC_REG = 0,
  GX_SRC_VTX = 1
};

// Light diffuse attenuation function
enum : u32
{
  LIGHTDIF_NONE = 0,
  LIGHTDIF_SIGN = 1,
  LIGHTDIF_CLAMP = 2
};

// Light attenuation function
enum : u32
{
  LIGHTATTN_NONE = 0,  // No attenuation
  LIGHTATTN_SPEC = 1,  // Point light attenuation
  LIGHTATTN_DIR = 2,   // Directional light attenuation
  LIGHTATTN_SPOT = 3   // Spot light attenuation
};

// Projection type
enum : u32
{
  GX_PERSPECTIVE = 0,
  GX_ORTHOGRAPHIC = 1
};

// Registers and register ranges
enum
{
  XFMEM_POSMATRICES = 0x000,
  XFMEM_POSMATRICES_END = 0x100,
  XFMEM_NORMALMATRICES = 0x400,
  XFMEM_NORMALMATRICES_END = 0x460,
  XFMEM_POSTMATRICES = 0x500,
  XFMEM_POSTMATRICES_END = 0x600,
  XFMEM_LIGHTS = 0x600,
  XFMEM_LIGHTS_END = 0x680,
  XFMEM_ERROR = 0x1000,
  XFMEM_DIAG = 0x1001,
  XFMEM_STATE0 = 0x1002,
  XFMEM_STATE1 = 0x1003,
  XFMEM_CLOCK = 0x1004,
  XFMEM_CLIPDISABLE = 0x1005,
  XFMEM_SETGPMETRIC = 0x1006,
  XFMEM_VTXSPECS = 0x1008,
  XFMEM_SETNUMCHAN = 0x1009,
  XFMEM_SETCHAN0_AMBCOLOR = 0x100a,
  XFMEM_SETCHAN1_AMBCOLOR = 0x100b,
  XFMEM_SETCHAN0_MATCOLOR = 0x100c,
  XFMEM_SETCHAN1_MATCOLOR = 0x100d,
  XFMEM_SETCHAN0_COLOR = 0x100e,
  XFMEM_SETCHAN1_COLOR = 0x100f,
  XFMEM_SETCHAN0_ALPHA = 0x1010,
  XFMEM_SETCHAN1_ALPHA = 0x1011,
  XFMEM_DUALTEX = 0x1012,
  XFMEM_SETMATRIXINDA = 0x1018,
  XFMEM_SETMATRIXINDB = 0x1019,
  XFMEM_SETVIEWPORT = 0x101a,
  XFMEM_SETZSCALE = 0x101c,
  XFMEM_SETZOFFSET = 0x101f,
  XFMEM_SETPROJECTION = 0x1020,
  // XFMEM_SETPROJECTIONB      = 0x1021,
  // XFMEM_SETPROJECTIONC      = 0x1022,
  // XFMEM_SETPROJECTIOND      = 0x1023,
  // XFMEM_SETPROJECTIONE      = 0x1024,
  // XFMEM_SETPROJECTIONF      = 0x1025,
  // XFMEM_SETPROJECTIONORTHO1 = 0x1026,
  // XFMEM_SETPROJECTIONORTHO2 = 0x1027,
  XFMEM_SETNUMTEXGENS = 0x103f,
  XFMEM_SETTEXMTXINFO = 0x1040,
  XFMEM_SETPOSMTXINFO = 0x1050,
};

union LitChannel
{
  BitField<0, 1, u32> matsource;
  BitField<1, 1, u32> enablelighting;
  BitField<2, 4, u32> lightMask0_3;
  BitField<6, 1, u32> ambsource;
  BitField<7, 2, u32> diffusefunc;  // LIGHTDIF_X
  BitField<9, 2, u32> attnfunc;     // LIGHTATTN_X
  BitField<11, 4, u32> lightMask4_7;
  u32 hex;

  unsigned int GetFullLightMask() const
  {
    return enablelighting ? (lightMask0_3 | (lightMask4_7 << 4)) : 0;
  }
};

union INVTXSPEC
{
  struct
  {
    u32 numcolors : 2;
    u32 numnormals : 2;  // 0 - nothing, 1 - just normal, 2 - normals and binormals
    u32 numtextures : 4;
    u32 unused : 24;
  };
  u32 hex;
};

union TexMtxInfo
{
  BitField<0, 1, u32> unknown;             //
  BitField<1, 1, u32> projection;          // XF_TEXPROJ_X
  BitField<2, 1, u32> inputform;           // XF_TEXINPUT_X
  BitField<3, 1, u32> unknown2;            //
  BitField<4, 3, u32> texgentype;          // XF_TEXGEN_X
  BitField<7, 5, u32> sourcerow;           // XF_SRCGEOM_X
  BitField<12, 3, u32> embosssourceshift;  // what generated texcoord to use
  BitField<15, 3, u32> embosslightshift;   // light index that is used
  u32 hex;
};

union PostMtxInfo
{
  BitField<0, 6, u32> index;      // base row of dual transform matrix
  BitField<6, 2, u32> unused;     //
  BitField<8, 1, u32> normalize;  // normalize before send operation
  u32 hex;
};

union NumColorChannel
{
  struct
  {
    u32 numColorChans : 2;
  };
  u32 hex;
};

union NumTexGen
{
  struct
  {
    u32 numTexGens : 4;
  };
  u32 hex;
};

union DualTexInfo
{
  struct
  {
    u32 enabled : 1;
  };
  u32 hex;
};

struct Light
{
  u32 useless[3];
  u8 color[4];
  float cosatt[3];   // cos attenuation
  float distatt[3];  // dist attenuation

  union
  {
    struct
    {
      float dpos[3];
      float ddir[3];  // specular lights only
    };

    struct
    {
      float sdir[3];
      float shalfangle[3];  // specular lights only
    };
  };
};

struct Viewport
{
  float wd;
  float ht;
  float zRange;
  float xOrig;
  float yOrig;
  float farZ;
};

struct Projection
{
  float rawProjection[6];
  u32 type;  // only GX_PERSPECTIVE or GX_ORTHOGRAPHIC are allowed
};

struct XFMemory
{
  float posMatrices[256];      // 0x0000 - 0x00ff
  u32 unk0[768];               // 0x0100 - 0x03ff
  float normalMatrices[96];    // 0x0400 - 0x045f
  u32 unk1[160];               // 0x0460 - 0x04ff
  float postMatrices[256];     // 0x0500 - 0x05ff
  Light lights[8];             // 0x0600 - 0x067f
  u32 unk2[2432];              // 0x0680 - 0x0fff
  u32 error;                   // 0x1000
  u32 diag;                    // 0x1001
  u32 state0;                  // 0x1002
  u32 state1;                  // 0x1003
  u32 xfClock;                 // 0x1004
  u32 clipDisable;             // 0x1005
  u32 perf0;                   // 0x1006
  u32 perf1;                   // 0x1007
  INVTXSPEC hostinfo;          // 0x1008 number of textures,colors,normals from vertex input
  NumColorChannel numChan;     // 0x1009
  u32 ambColor[2];             // 0x100a, 0x100b
  u32 matColor[2];             // 0x100c, 0x100d
  LitChannel color[2];         // 0x100e, 0x100f
  LitChannel alpha[2];         // 0x1010, 0x1011
  DualTexInfo dualTexTrans;    // 0x1012
  u32 unk3;                    // 0x1013
  u32 unk4;                    // 0x1014
  u32 unk5;                    // 0x1015
  u32 unk6;                    // 0x1016
  u32 unk7;                    // 0x1017
  TMatrixIndexA MatrixIndexA;  // 0x1018
  TMatrixIndexB MatrixIndexB;  // 0x1019
  Viewport viewport;           // 0x101a - 0x101f
  Projection projection;       // 0x1020 - 0x1026
  u32 unk8[24];                // 0x1027 - 0x103e
  NumTexGen numTexGen;         // 0x103f
  TexMtxInfo texMtxInfo[8];    // 0x1040 - 0x1047
  u32 unk9[8];                 // 0x1048 - 0x104f
  PostMtxInfo postMtxInfo[8];  // 0x1050 - 0x1057
};

extern XFMemory xfmem;

void LoadXFReg(u32 transferSize, u32 address, DataReader src);
void LoadIndexedXF(u32 val, int array);
void PreprocessIndexedXF(u32 val, int refarray);
