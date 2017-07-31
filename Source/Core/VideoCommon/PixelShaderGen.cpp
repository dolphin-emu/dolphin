// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "VideoCommon/PixelShaderGen.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/LightingShaderGen.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"  // for texture projection mode

// TODO: Get rid of these
enum : u32
{
  C_COLORMATRIX = 0,                //  0
  C_COLORS = 0,                     //  0
  C_KCOLORS = C_COLORS + 4,         //  4
  C_ALPHA = C_KCOLORS + 4,          //  8
  C_TEXDIMS = C_ALPHA + 1,          //  9
  C_ZBIAS = C_TEXDIMS + 8,          // 17
  C_INDTEXSCALE = C_ZBIAS + 2,      // 19
  C_INDTEXMTX = C_INDTEXSCALE + 2,  // 21
  C_FOGCOLOR = C_INDTEXMTX + 6,     // 27
  C_FOGI = C_FOGCOLOR + 1,          // 28
  C_FOGF = C_FOGI + 1,              // 29
  C_ZSLOPE = C_FOGF + 2,            // 31
  C_EFBSCALE = C_ZSLOPE + 1,        // 32
  C_PENVCONST_END = C_EFBSCALE + 1
};

static const char* tevKSelTableC[] = {
    "255,255,255",        // 1   = 0x00
    "223,223,223",        // 7_8 = 0x01
    "191,191,191",        // 3_4 = 0x02
    "159,159,159",        // 5_8 = 0x03
    "128,128,128",        // 1_2 = 0x04
    "96,96,96",           // 3_8 = 0x05
    "64,64,64",           // 1_4 = 0x06
    "32,32,32",           // 1_8 = 0x07
    "0,0,0",              // INVALID = 0x08
    "0,0,0",              // INVALID = 0x09
    "0,0,0",              // INVALID = 0x0a
    "0,0,0",              // INVALID = 0x0b
    I_KCOLORS "[0].rgb",  // K0 = 0x0C
    I_KCOLORS "[1].rgb",  // K1 = 0x0D
    I_KCOLORS "[2].rgb",  // K2 = 0x0E
    I_KCOLORS "[3].rgb",  // K3 = 0x0F
    I_KCOLORS "[0].rrr",  // K0_R = 0x10
    I_KCOLORS "[1].rrr",  // K1_R = 0x11
    I_KCOLORS "[2].rrr",  // K2_R = 0x12
    I_KCOLORS "[3].rrr",  // K3_R = 0x13
    I_KCOLORS "[0].ggg",  // K0_G = 0x14
    I_KCOLORS "[1].ggg",  // K1_G = 0x15
    I_KCOLORS "[2].ggg",  // K2_G = 0x16
    I_KCOLORS "[3].ggg",  // K3_G = 0x17
    I_KCOLORS "[0].bbb",  // K0_B = 0x18
    I_KCOLORS "[1].bbb",  // K1_B = 0x19
    I_KCOLORS "[2].bbb",  // K2_B = 0x1A
    I_KCOLORS "[3].bbb",  // K3_B = 0x1B
    I_KCOLORS "[0].aaa",  // K0_A = 0x1C
    I_KCOLORS "[1].aaa",  // K1_A = 0x1D
    I_KCOLORS "[2].aaa",  // K2_A = 0x1E
    I_KCOLORS "[3].aaa",  // K3_A = 0x1F
};

static const char* tevKSelTableA[] = {
    "255",              // 1   = 0x00
    "223",              // 7_8 = 0x01
    "191",              // 3_4 = 0x02
    "159",              // 5_8 = 0x03
    "128",              // 1_2 = 0x04
    "96",               // 3_8 = 0x05
    "64",               // 1_4 = 0x06
    "32",               // 1_8 = 0x07
    "0",                // INVALID = 0x08
    "0",                // INVALID = 0x09
    "0",                // INVALID = 0x0a
    "0",                // INVALID = 0x0b
    "0",                // INVALID = 0x0c
    "0",                // INVALID = 0x0d
    "0",                // INVALID = 0x0e
    "0",                // INVALID = 0x0f
    I_KCOLORS "[0].r",  // K0_R = 0x10
    I_KCOLORS "[1].r",  // K1_R = 0x11
    I_KCOLORS "[2].r",  // K2_R = 0x12
    I_KCOLORS "[3].r",  // K3_R = 0x13
    I_KCOLORS "[0].g",  // K0_G = 0x14
    I_KCOLORS "[1].g",  // K1_G = 0x15
    I_KCOLORS "[2].g",  // K2_G = 0x16
    I_KCOLORS "[3].g",  // K3_G = 0x17
    I_KCOLORS "[0].b",  // K0_B = 0x18
    I_KCOLORS "[1].b",  // K1_B = 0x19
    I_KCOLORS "[2].b",  // K2_B = 0x1A
    I_KCOLORS "[3].b",  // K3_B = 0x1B
    I_KCOLORS "[0].a",  // K0_A = 0x1C
    I_KCOLORS "[1].a",  // K1_A = 0x1D
    I_KCOLORS "[2].a",  // K2_A = 0x1E
    I_KCOLORS "[3].a",  // K3_A = 0x1F
};

static const char* tevCInputTable[] = {
    "prev.rgb",           // CPREV,
    "prev.aaa",           // APREV,
    "c0.rgb",             // C0,
    "c0.aaa",             // A0,
    "c1.rgb",             // C1,
    "c1.aaa",             // A1,
    "c2.rgb",             // C2,
    "c2.aaa",             // A2,
    "textemp.rgb",        // TEXC,
    "textemp.aaa",        // TEXA,
    "rastemp.rgb",        // RASC,
    "rastemp.aaa",        // RASA,
    "int3(255,255,255)",  // ONE
    "int3(128,128,128)",  // HALF
    "konsttemp.rgb",      // KONST
    "int3(0,0,0)",        // ZERO
};

static const char* tevAInputTable[] = {
    "prev.a",       // APREV,
    "c0.a",         // A0,
    "c1.a",         // A1,
    "c2.a",         // A2,
    "textemp.a",    // TEXA,
    "rastemp.a",    // RASA,
    "konsttemp.a",  // KONST,  (hw1 had quarter)
    "0",            // ZERO
};

static const char* tevRasTable[] = {
    "iround(col0 * 255.0)",
    "iround(col1 * 255.0)",
    "ERROR13",                                              // 2
    "ERROR14",                                              // 3
    "ERROR15",                                              // 4
    "(int4(1, 1, 1, 1) * alphabump)",                       // bump alpha (0..248)
    "(int4(1, 1, 1, 1) * (alphabump | (alphabump >> 5)))",  // normalized bump alpha (0..255)
    "int4(0, 0, 0, 0)",                                     // zero
};

static const char* tevCOutputTable[] = {"prev.rgb", "c0.rgb", "c1.rgb", "c2.rgb"};
static const char* tevAOutputTable[] = {"prev.a", "c0.a", "c1.a", "c2.a"};

// FIXME: Some of the video card's capabilities (BBox support, EarlyZ support, dstAlpha support)
// leak
//        into this UID; This is really unhelpful if these UIDs ever move from one machine to
//        another.
PixelShaderUid GetPixelShaderUid()
{
  PixelShaderUid out;
  pixel_shader_uid_data* uid_data = out.GetUidData<pixel_shader_uid_data>();
  memset(uid_data, 0, sizeof(*uid_data));

  uid_data->useDstAlpha = bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate &&
                          bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;

  uid_data->genMode_numindstages = bpmem.genMode.numindstages;
  uid_data->genMode_numtevstages = bpmem.genMode.numtevstages;
  uid_data->genMode_numtexgens = bpmem.genMode.numtexgens;
  uid_data->bounding_box = g_ActiveConfig.BBoxUseFragmentShaderImplementation() &&
                           g_ActiveConfig.bBBoxEnable && BoundingBox::active;
  uid_data->rgba6_format =
      bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24 && !g_ActiveConfig.bForceTrueColor;
  uid_data->dither = bpmem.blendmode.dither && uid_data->rgba6_format;

  u32 numStages = uid_data->genMode_numtevstages + 1;

  const bool forced_early_z =
      bpmem.UseEarlyDepthTest() &&
      (g_ActiveConfig.bFastDepthCalc || bpmem.alpha_test.TestResult() == AlphaTest::UNDETERMINED)
      // We can't allow early_ztest for zfreeze because depth is overridden per-pixel.
      // This means it's impossible for zcomploc to be emulated on a zfrozen polygon.
      && !(bpmem.zmode.testenable && bpmem.genMode.zfreeze);
  const bool per_pixel_depth =
      (bpmem.ztex2.op != ZTEXTURE_DISABLE && bpmem.UseLateDepthTest()) ||
      (!g_ActiveConfig.bFastDepthCalc && bpmem.zmode.testenable && !forced_early_z) ||
      (bpmem.zmode.testenable && bpmem.genMode.zfreeze);

  uid_data->per_pixel_depth = per_pixel_depth;
  uid_data->forced_early_z = forced_early_z;

  if (g_ActiveConfig.bEnablePixelLighting)
  {
    // The lighting shader only needs the two color bits of the 23bit component bit array.
    uid_data->components =
        (VertexLoaderManager::g_current_components & (VB_HAS_COL0 | VB_HAS_COL1)) >> VB_COL_SHIFT;
    uid_data->numColorChans = xfmem.numChan.numColorChans;
    GetLightingShaderUid(uid_data->lighting);
  }

  if (uid_data->genMode_numtexgens > 0)
  {
    for (unsigned int i = 0; i < uid_data->genMode_numtexgens; ++i)
    {
      // optional perspective divides
      uid_data->texMtxInfo_n_projection |= xfmem.texMtxInfo[i].projection << i;
    }
  }

  // indirect texture map lookup
  int nIndirectStagesUsed = 0;
  if (uid_data->genMode_numindstages > 0)
  {
    for (unsigned int i = 0; i < numStages; ++i)
    {
      if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < uid_data->genMode_numindstages)
        nIndirectStagesUsed |= 1 << bpmem.tevind[i].bt;
    }
  }

  uid_data->nIndirectStagesUsed = nIndirectStagesUsed;
  for (u32 i = 0; i < uid_data->genMode_numindstages; ++i)
  {
    if (uid_data->nIndirectStagesUsed & (1 << i))
      uid_data->SetTevindrefValues(i, bpmem.tevindref.getTexCoord(i), bpmem.tevindref.getTexMap(i));
  }

  for (unsigned int n = 0; n < numStages; n++)
  {
    int texcoord = bpmem.tevorders[n / 2].getTexCoord(n & 1);
    bool bHasTexCoord = (u32)texcoord < bpmem.genMode.numtexgens;
    // HACK to handle cases where the tex gen is not enabled
    if (!bHasTexCoord)
      texcoord = bpmem.genMode.numtexgens;

    uid_data->stagehash[n].hasindstage = bpmem.tevind[n].bt < bpmem.genMode.numindstages;
    uid_data->stagehash[n].tevorders_texcoord = texcoord;
    if (uid_data->stagehash[n].hasindstage)
      uid_data->stagehash[n].tevind = bpmem.tevind[n].hex;

    TevStageCombiner::ColorCombiner& cc = bpmem.combiners[n].colorC;
    TevStageCombiner::AlphaCombiner& ac = bpmem.combiners[n].alphaC;
    uid_data->stagehash[n].cc = cc.hex & 0xFFFFFF;
    uid_data->stagehash[n].ac = ac.hex & 0xFFFFF0;  // Storing rswap and tswap later

    if (cc.a == TEVCOLORARG_RASA || cc.a == TEVCOLORARG_RASC || cc.b == TEVCOLORARG_RASA ||
        cc.b == TEVCOLORARG_RASC || cc.c == TEVCOLORARG_RASA || cc.c == TEVCOLORARG_RASC ||
        cc.d == TEVCOLORARG_RASA || cc.d == TEVCOLORARG_RASC || ac.a == TEVALPHAARG_RASA ||
        ac.b == TEVALPHAARG_RASA || ac.c == TEVALPHAARG_RASA || ac.d == TEVALPHAARG_RASA)
    {
      const int i = bpmem.combiners[n].alphaC.rswap;
      uid_data->stagehash[n].tevksel_swap1a = bpmem.tevksel[i * 2].swap1;
      uid_data->stagehash[n].tevksel_swap2a = bpmem.tevksel[i * 2].swap2;
      uid_data->stagehash[n].tevksel_swap1b = bpmem.tevksel[i * 2 + 1].swap1;
      uid_data->stagehash[n].tevksel_swap2b = bpmem.tevksel[i * 2 + 1].swap2;
      uid_data->stagehash[n].tevorders_colorchan = bpmem.tevorders[n / 2].getColorChan(n & 1);
    }

    uid_data->stagehash[n].tevorders_enable = bpmem.tevorders[n / 2].getEnable(n & 1);
    if (uid_data->stagehash[n].tevorders_enable)
    {
      const int i = bpmem.combiners[n].alphaC.tswap;
      uid_data->stagehash[n].tevksel_swap1c = bpmem.tevksel[i * 2].swap1;
      uid_data->stagehash[n].tevksel_swap2c = bpmem.tevksel[i * 2].swap2;
      uid_data->stagehash[n].tevksel_swap1d = bpmem.tevksel[i * 2 + 1].swap1;
      uid_data->stagehash[n].tevksel_swap2d = bpmem.tevksel[i * 2 + 1].swap2;
      uid_data->stagehash[n].tevorders_texmap = bpmem.tevorders[n / 2].getTexMap(n & 1);
    }

    if (cc.a == TEVCOLORARG_KONST || cc.b == TEVCOLORARG_KONST || cc.c == TEVCOLORARG_KONST ||
        cc.d == TEVCOLORARG_KONST || ac.a == TEVALPHAARG_KONST || ac.b == TEVALPHAARG_KONST ||
        ac.c == TEVALPHAARG_KONST || ac.d == TEVALPHAARG_KONST)
    {
      uid_data->stagehash[n].tevksel_kc = bpmem.tevksel[n / 2].getKC(n & 1);
      uid_data->stagehash[n].tevksel_ka = bpmem.tevksel[n / 2].getKA(n & 1);
    }
  }

#define MY_STRUCT_OFFSET(str, elem) ((u32)((u64) & (str).elem - (u64) & (str)))
  uid_data->num_values = (g_ActiveConfig.bEnablePixelLighting) ?
                             sizeof(*uid_data) :
                             MY_STRUCT_OFFSET(*uid_data, stagehash[numStages]);

  AlphaTest::TEST_RESULT Pretest = bpmem.alpha_test.TestResult();
  uid_data->Pretest = Pretest;
  uid_data->late_ztest = bpmem.UseLateDepthTest();

  // NOTE: Fragment may not be discarded if alpha test always fails and early depth test is enabled
  // (in this case we need to write a depth value if depth test passes regardless of the alpha
  // testing result)
  if (uid_data->Pretest == AlphaTest::UNDETERMINED ||
      (uid_data->Pretest == AlphaTest::FAIL && uid_data->late_ztest))
  {
    uid_data->alpha_test_comp0 = bpmem.alpha_test.comp0;
    uid_data->alpha_test_comp1 = bpmem.alpha_test.comp1;
    uid_data->alpha_test_logic = bpmem.alpha_test.logic;

    // ZCOMPLOC HACK:
    // The only way to emulate alpha test + early-z is to force early-z in the shader.
    // As this isn't available on all drivers and as we can't emulate this feature otherwise,
    // we are only able to choose which one we want to respect more.
    // Tests seem to have proven that writing depth even when the alpha test fails is more
    // important that a reliable alpha test, so we just force the alpha test to always succeed.
    // At least this seems to be less buggy.
    uid_data->alpha_test_use_zcomploc_hack =
        bpmem.UseEarlyDepthTest() && bpmem.zmode.updateenable &&
        !g_ActiveConfig.backend_info.bSupportsEarlyZ && !bpmem.genMode.zfreeze;
  }

  uid_data->zfreeze = bpmem.genMode.zfreeze;
  uid_data->ztex_op = bpmem.ztex2.op;
  uid_data->early_ztest = bpmem.UseEarlyDepthTest();
  uid_data->fog_fsel = bpmem.fog.c_proj_fsel.fsel;
  uid_data->fog_fsel = bpmem.fog.c_proj_fsel.fsel;
  uid_data->fog_proj = bpmem.fog.c_proj_fsel.proj;
  uid_data->fog_RangeBaseEnabled = bpmem.fogRange.Base.Enabled;

  return out;
}

void WritePixelShaderCommonHeader(ShaderCode& out, APIType ApiType, u32 num_texgens,
                                  bool per_pixel_lighting, bool bounding_box)
{
  // dot product for integer vectors
  out.Write("int idot(int3 x, int3 y)\n"
            "{\n"
            "\tint3 tmp = x * y;\n"
            "\treturn tmp.x + tmp.y + tmp.z;\n"
            "}\n");

  out.Write("int idot(int4 x, int4 y)\n"
            "{\n"
            "\tint4 tmp = x * y;\n"
            "\treturn tmp.x + tmp.y + tmp.z + tmp.w;\n"
            "}\n\n");

  // rounding + casting to integer at once in a single function
  out.Write("int  iround(float  x) { return int (round(x)); }\n"
            "int2 iround(float2 x) { return int2(round(x)); }\n"
            "int3 iround(float3 x) { return int3(round(x)); }\n"
            "int4 iround(float4 x) { return int4(round(x)); }\n\n");

  if (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan)
  {
    out.Write("SAMPLER_BINDING(0) uniform sampler2DArray samp[8];\n");
  }
  else  // D3D
  {
    // Declare samplers
    out.Write("SamplerState samp[8] : register(s0);\n");
    out.Write("\n");
    out.Write("Texture2DArray Tex[8] : register(t0);\n");
  }
  out.Write("\n");

  if (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan)
    out.Write("UBO_BINDING(std140, 1) uniform PSBlock {\n");
  else
    out.Write("cbuffer PSBlock : register(b0) {\n");

  out.Write("\tint4 " I_COLORS "[4];\n"
            "\tint4 " I_KCOLORS "[4];\n"
            "\tint4 " I_ALPHA ";\n"
            "\tfloat4 " I_TEXDIMS "[8];\n"
            "\tint4 " I_ZBIAS "[2];\n"
            "\tint4 " I_INDTEXSCALE "[2];\n"
            "\tint4 " I_INDTEXMTX "[6];\n"
            "\tint4 " I_FOGCOLOR ";\n"
            "\tint4 " I_FOGI ";\n"
            "\tfloat4 " I_FOGF "[2];\n"
            "\tfloat4 " I_ZSLOPE ";\n"
            "\tfloat2 " I_EFBSCALE ";\n"
            "\tuint  bpmem_genmode;\n"
            "\tuint  bpmem_alphaTest;\n"
            "\tuint  bpmem_fogParam3;\n"
            "\tuint  bpmem_fogRangeBase;\n"
            "\tuint  bpmem_dstalpha;\n"
            "\tuint  bpmem_ztex_op;\n"
            "\tbool  bpmem_late_ztest;\n"
            "\tbool  bpmem_rgba6_format;\n"
            "\tbool  bpmem_dither;\n"
            "\tbool  bpmem_bounding_box;\n"
            "\tuint4 bpmem_pack1[16];\n"  // .xy - combiners, .z - tevind
            "\tuint4 bpmem_pack2[8];\n"   // .x - tevorder, .y - tevksel
            "\tint4  konstLookup[32];\n"
            "};\n\n");
  out.Write("#define bpmem_combiners(i) (bpmem_pack1[(i)].xy)\n"
            "#define bpmem_tevind(i) (bpmem_pack1[(i)].z)\n"
            "#define bpmem_iref(i) (bpmem_pack1[(i)].w)\n"
            "#define bpmem_tevorder(i) (bpmem_pack2[(i)].x)\n"
            "#define bpmem_tevksel(i) (bpmem_pack2[(i)].y)\n\n");

  if (per_pixel_lighting)
  {
    out.Write("%s", s_lighting_struct);

    if (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan)
      out.Write("UBO_BINDING(std140, 2) uniform VSBlock {\n");
    else
      out.Write("cbuffer VSBlock : register(b1) {\n");

    out.Write(s_shader_uniforms);
    out.Write("};\n");
  }

  if (bounding_box)
  {
    if (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan)
    {
      out.Write("SSBO_BINDING(0) buffer BBox {\n"
                "\tint4 bbox_data;\n"
                "};\n");
    }
    else
    {
      out.Write("globallycoherent RWBuffer<int> bbox_data : register(u2);\n");
    }
  }

  out.Write("struct VS_OUTPUT {\n");
  GenerateVSOutputMembers(out, ApiType, num_texgens, per_pixel_lighting, "");
  out.Write("};\n");
}

static void WriteStage(ShaderCode& out, const pixel_shader_uid_data* uid_data, int n,
                       APIType ApiType, bool stereo);
static void WriteTevRegular(ShaderCode& out, const char* components, int bias, int op, int clamp,
                            int shift, bool alpha);
static void SampleTexture(ShaderCode& out, const char* texcoords, const char* texswap, int texmap,
                          bool stereo, APIType ApiType);
static void WriteAlphaTest(ShaderCode& out, const pixel_shader_uid_data* uid_data, APIType ApiType,
                           bool per_pixel_depth, bool use_dual_source);
static void WriteFog(ShaderCode& out, const pixel_shader_uid_data* uid_data);
static void WriteColor(ShaderCode& out, const pixel_shader_uid_data* uid_data,
                       bool use_dual_source);

ShaderCode GeneratePixelShaderCode(APIType ApiType, const ShaderHostConfig& host_config,
                                   const pixel_shader_uid_data* uid_data)
{
  ShaderCode out;

  const bool per_pixel_lighting = g_ActiveConfig.bEnablePixelLighting;
  const bool msaa = host_config.msaa;
  const bool ssaa = host_config.ssaa;
  const bool stereo = host_config.stereo;
  const u32 numStages = uid_data->genMode_numtevstages + 1;

  out.Write("//Pixel Shader for TEV stages\n");
  out.Write("//%i TEV stages, %i texgens, %i IND stages\n", numStages, uid_data->genMode_numtexgens,
            uid_data->genMode_numindstages);

  // Stuff that is shared between ubershaders and pixelgen.
  WritePixelShaderCommonHeader(out, ApiType, uid_data->genMode_numtexgens, per_pixel_lighting,
                               uid_data->bounding_box);

  if (uid_data->forced_early_z && g_ActiveConfig.backend_info.bSupportsEarlyZ)
  {
    // Zcomploc (aka early_ztest) is a way to control whether depth test is done before
    // or after texturing and alpha test. PC graphics APIs used to provide no way to emulate
    // this feature properly until 2012: Depth tests were always done after alpha testing.
    // Most importantly, it was not possible to write to the depth buffer without also writing
    // a color value (unless color writing was disabled altogether).

    // OpenGL 4.2 actually provides two extensions which can force an early z test:
    //  * ARB_image_load_store has 'layout(early_fragment_tests)' which forces the driver to do z
    //  and stencil tests early.
    //  * ARB_conservative_depth has 'layout(depth_unchanged) which signals to the driver that it
    //  can make optimisations
    //    which assume the pixel shader won't update the depth buffer.

    // early_fragment_tests is the best option, as it requires the driver to do early-z and defines
    // early-z exactly as
    // we expect, with discard causing the shader to exit with only the depth buffer updated.

    // Conservative depth's 'depth_unchanged' only hints to the driver that an early-z optimisation
    // can be made and
    // doesn't define what will happen if we discard the fragment. But the way modern graphics
    // hardware is implemented
    // means it is not unreasonable to expect the same behaviour as early_fragment_tests.
    // We can also assume that if a driver has gone out of its way to support conservative depth and
    // not image_load_store
    // as required by OpenGL 4.2 that it will be doing the optimisation.
    // If the driver doesn't actually do an early z optimisation, ZCompLoc will be broken and depth
    // will only be written
    // if the alpha test passes.

    // We support Conservative as a fallback, because many drivers based on Mesa haven't implemented
    // all of the
    // ARB_image_load_store extension yet.

    // D3D11 also has a way to force the driver to enable early-z, so we're fine here.
    if (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan)
    {
      // This is a #define which signals whatever early-z method the driver supports.
      out.Write("FORCE_EARLY_Z; \n");
    }
    else
    {
      out.Write("[earlydepthstencil]\n");
    }
  }

  // Only use dual-source blending when required on drivers that don't support it very well.
  const bool use_dual_source =
      host_config.backend_dual_source_blend &&
      (!DriverDetails::HasBug(DriverDetails::BUG_BROKEN_DUAL_SOURCE_BLENDING) ||
       uid_data->useDstAlpha);

  if (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan)
  {
    if (use_dual_source)
    {
      if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_FRAGMENT_SHADER_INDEX_DECORATION))
      {
        out.Write("FRAGMENT_OUTPUT_LOCATION(0) out vec4 ocol0;\n");
        out.Write("FRAGMENT_OUTPUT_LOCATION(1) out vec4 ocol1;\n");
      }
      else
      {
        out.Write("FRAGMENT_OUTPUT_LOCATION_INDEXED(0, 0) out vec4 ocol0;\n");
        out.Write("FRAGMENT_OUTPUT_LOCATION_INDEXED(0, 1) out vec4 ocol1;\n");
      }
    }
    else
    {
      out.Write("FRAGMENT_OUTPUT_LOCATION(0) out vec4 ocol0;\n");
    }

    if (uid_data->per_pixel_depth)
      out.Write("#define depth gl_FragDepth\n");

    // We need to always use output blocks for Vulkan, but geometry shaders are also optional.
    if (host_config.backend_geometry_shaders || ApiType == APIType::Vulkan)
    {
      out.Write("VARYING_LOCATION(0) in VertexData {\n");
      GenerateVSOutputMembers(out, ApiType, uid_data->genMode_numtexgens, per_pixel_lighting,
                              GetInterpolationQualifier(msaa, ssaa, true, true));

      if (stereo)
        out.Write("\tflat int layer;\n");

      out.Write("};\n");
    }
    else
    {
      out.Write("%s in float4 colors_0;\n", GetInterpolationQualifier(msaa, ssaa));
      out.Write("%s in float4 colors_1;\n", GetInterpolationQualifier(msaa, ssaa));
      // compute window position if needed because binding semantic WPOS is not widely supported
      // Let's set up attributes
      for (unsigned int i = 0; i < uid_data->genMode_numtexgens; ++i)
      {
        out.Write("%s in float3 tex%d;\n", GetInterpolationQualifier(msaa, ssaa), i);
      }
      out.Write("%s in float4 clipPos;\n", GetInterpolationQualifier(msaa, ssaa));
      if (per_pixel_lighting)
      {
        out.Write("%s in float3 Normal;\n", GetInterpolationQualifier(msaa, ssaa));
        out.Write("%s in float3 WorldPos;\n", GetInterpolationQualifier(msaa, ssaa));
      }
    }

    out.Write("void main()\n{\n");
    out.Write("\tfloat4 rawpos = gl_FragCoord;\n");
  }
  else  // D3D
  {
    out.Write("void main(\n");
    out.Write("  out float4 ocol0 : SV_Target0,\n"
              "  out float4 ocol1 : SV_Target1,\n%s"
              "  in float4 rawpos : SV_Position,\n",
              uid_data->per_pixel_depth ? "  out float depth : SV_Depth,\n" : "");

    out.Write("  in %s float4 colors_0 : COLOR0,\n", GetInterpolationQualifier(msaa, ssaa));
    out.Write("  in %s float4 colors_1 : COLOR1\n", GetInterpolationQualifier(msaa, ssaa));

    // compute window position if needed because binding semantic WPOS is not widely supported
    for (unsigned int i = 0; i < uid_data->genMode_numtexgens; ++i)
      out.Write(",\n  in %s float3 tex%d : TEXCOORD%d", GetInterpolationQualifier(msaa, ssaa), i,
                i);
    out.Write(",\n  in %s float4 clipPos : TEXCOORD%d", GetInterpolationQualifier(msaa, ssaa),
              uid_data->genMode_numtexgens);
    if (per_pixel_lighting)
    {
      out.Write(",\n  in %s float3 Normal : TEXCOORD%d", GetInterpolationQualifier(msaa, ssaa),
                uid_data->genMode_numtexgens + 1);
      out.Write(",\n  in %s float3 WorldPos : TEXCOORD%d", GetInterpolationQualifier(msaa, ssaa),
                uid_data->genMode_numtexgens + 2);
    }
    out.Write(",\n  in float clipDist0 : SV_ClipDistance0\n");
    out.Write(",\n  in float clipDist1 : SV_ClipDistance1\n");
    if (stereo)
      out.Write(",\n  in uint layer : SV_RenderTargetArrayIndex\n");
    out.Write("        ) {\n");
  }

  out.Write("\tint4 c0 = " I_COLORS "[1], c1 = " I_COLORS "[2], c2 = " I_COLORS
            "[3], prev = " I_COLORS "[0];\n"
            "\tint4 rastemp = int4(0, 0, 0, 0), textemp = int4(0, 0, 0, 0), konsttemp = int4(0, 0, "
            "0, 0);\n"
            "\tint3 comp16 = int3(1, 256, 0), comp24 = int3(1, 256, 256*256);\n"
            "\tint alphabump=0;\n"
            "\tint3 tevcoord=int3(0, 0, 0);\n"
            "\tint2 wrappedcoord=int2(0,0), tempcoord=int2(0,0);\n"
            "\tint4 "
            "tevin_a=int4(0,0,0,0),tevin_b=int4(0,0,0,0),tevin_c=int4(0,0,0,0),tevin_d=int4(0,0,0,"
            "0);\n\n");  // tev combiner inputs

  // On GLSL, input variables must not be assigned to.
  // This is why we declare these variables locally instead.
  out.Write("\tfloat4 col0 = colors_0;\n");
  out.Write("\tfloat4 col1 = colors_1;\n");

  if (per_pixel_lighting)
  {
    out.Write("\tfloat3 _norm0 = normalize(Normal.xyz);\n\n");
    out.Write("\tfloat3 pos = WorldPos;\n");

    out.Write("\tint4 lacc;\n"
              "\tfloat3 ldir, h, cosAttn, distAttn;\n"
              "\tfloat dist, dist2, attn;\n");

    // TODO: Our current constant usage code isn't able to handle more than one buffer.
    //       So we can't mark the VS constant as used here. But keep them here as reference.
    // out.SetConstantsUsed(C_PLIGHT_COLORS, C_PLIGHT_COLORS+7); // TODO: Can be optimized further
    // out.SetConstantsUsed(C_PLIGHTS, C_PLIGHTS+31); // TODO: Can be optimized further
    // out.SetConstantsUsed(C_PMATERIALS, C_PMATERIALS+3);
    GenerateLightingShaderCode(out, uid_data->lighting, uid_data->components << VB_COL_SHIFT,
                               uid_data->numColorChans, "colors_", "col");
  }

  // HACK to handle cases where the tex gen is not enabled
  if (uid_data->genMode_numtexgens == 0)
  {
    out.Write("\tint2 fixpoint_uv0 = int2(0, 0);\n\n");
  }
  else
  {
    out.SetConstantsUsed(C_TEXDIMS, C_TEXDIMS + uid_data->genMode_numtexgens - 1);
    for (unsigned int i = 0; i < uid_data->genMode_numtexgens; ++i)
    {
      out.Write("\tint2 fixpoint_uv%d = int2(", i);
      out.Write("(tex%d.z == 0.0 ? tex%d.xy : tex%d.xy / tex%d.z)", i, i, i, i);
      out.Write(" * " I_TEXDIMS "[%d].zw);\n", i);
      // TODO: S24 overflows here?
    }
  }

  for (u32 i = 0; i < uid_data->genMode_numindstages; ++i)
  {
    if (uid_data->nIndirectStagesUsed & (1 << i))
    {
      unsigned int texcoord = uid_data->GetTevindirefCoord(i);
      unsigned int texmap = uid_data->GetTevindirefMap(i);

      if (texcoord < uid_data->genMode_numtexgens)
      {
        out.SetConstantsUsed(C_INDTEXSCALE + i / 2, C_INDTEXSCALE + i / 2);
        out.Write("\ttempcoord = fixpoint_uv%d >> " I_INDTEXSCALE "[%d].%s;\n", texcoord, i / 2,
                  (i & 1) ? "zw" : "xy");
      }
      else
        out.Write("\ttempcoord = int2(0, 0);\n");

      out.Write("\tint3 iindtex%d = ", i);
      SampleTexture(out, "float2(tempcoord)", "abg", texmap, stereo, ApiType);
    }
  }

  for (unsigned int i = 0; i < numStages; i++)
    WriteStage(out, uid_data, i, ApiType, stereo);  // build the equation for this stage

  {
    // The results of the last texenv stage are put onto the screen,
    // regardless of the used destination register
    TevStageCombiner::ColorCombiner last_cc;
    TevStageCombiner::AlphaCombiner last_ac;
    last_cc.hex = uid_data->stagehash[uid_data->genMode_numtevstages].cc;
    last_ac.hex = uid_data->stagehash[uid_data->genMode_numtevstages].ac;
    if (last_cc.dest != 0)
    {
      out.Write("\tprev.rgb = %s;\n", tevCOutputTable[last_cc.dest]);
    }
    if (last_ac.dest != 0)
    {
      out.Write("\tprev.a = %s;\n", tevAOutputTable[last_ac.dest]);
    }
  }
  out.Write("\tprev = prev & 255;\n");

  // NOTE: Fragment may not be discarded if alpha test always fails and early depth test is enabled
  // (in this case we need to write a depth value if depth test passes regardless of the alpha
  // testing result)
  if (uid_data->Pretest == AlphaTest::UNDETERMINED ||
      (uid_data->Pretest == AlphaTest::FAIL && uid_data->late_ztest))
    WriteAlphaTest(out, uid_data, ApiType, uid_data->per_pixel_depth, use_dual_source);

  if (uid_data->zfreeze)
  {
    out.SetConstantsUsed(C_ZSLOPE, C_ZSLOPE);
    out.SetConstantsUsed(C_EFBSCALE, C_EFBSCALE);

    out.Write("\tfloat2 screenpos = rawpos.xy * " I_EFBSCALE ".xy;\n");

    // Opengl has reversed vertical screenspace coordinates
    if (ApiType == APIType::OpenGL)
      out.Write("\tscreenpos.y = %i.0 - screenpos.y;\n", EFB_HEIGHT);

    out.Write("\tint zCoord = int(" I_ZSLOPE ".z + " I_ZSLOPE ".x * screenpos.x + " I_ZSLOPE
              ".y * screenpos.y);\n");
  }
  else if (!host_config.fast_depth_calc)
  {
    // FastDepth means to trust the depth generated in perspective division.
    // It should be correct, but it seems not to be as accurate as required. TODO: Find out why!
    // For disabled FastDepth we just calculate the depth value again.
    // The performance impact of this additional calculation doesn't matter, but it prevents
    // the host GPU driver from performing any early depth test optimizations.
    out.SetConstantsUsed(C_ZBIAS + 1, C_ZBIAS + 1);
    // the screen space depth value = far z + (clip z / clip w) * z range
    out.Write("\tint zCoord = " I_ZBIAS "[1].x + int((clipPos.z / clipPos.w) * float(" I_ZBIAS
              "[1].y));\n");
  }
  else
  {
    if (ApiType == APIType::D3D || ApiType == APIType::Vulkan)
      out.Write("\tint zCoord = int((1.0 - rawpos.z) * 16777216.0);\n");
    else
      out.Write("\tint zCoord = int(rawpos.z * 16777216.0);\n");
  }
  out.Write("\tzCoord = clamp(zCoord, 0, 0xFFFFFF);\n");

  // depth texture can safely be ignored if the result won't be written to the depth buffer
  // (early_ztest) and isn't used for fog either
  const bool skip_ztexture = !uid_data->per_pixel_depth && !uid_data->fog_fsel;

  // Note: z-textures are not written to depth buffer if early depth test is used
  if (uid_data->per_pixel_depth && uid_data->early_ztest)
  {
    if (ApiType == APIType::D3D || ApiType == APIType::Vulkan)
      out.Write("\tdepth = 1.0 - float(zCoord) / 16777216.0;\n");
    else
      out.Write("\tdepth = float(zCoord) / 16777216.0;\n");
  }

  // Note: depth texture output is only written to depth buffer if late depth test is used
  // theoretical final depth value is used for fog calculation, though, so we have to emulate
  // ztextures anyway
  if (uid_data->ztex_op != ZTEXTURE_DISABLE && !skip_ztexture)
  {
    // use the texture input of the last texture stage (textemp), hopefully this has been read and
    // is in correct format...
    out.SetConstantsUsed(C_ZBIAS, C_ZBIAS + 1);
    out.Write("\tzCoord = idot(" I_ZBIAS "[0].xyzw, textemp.xyzw) + " I_ZBIAS "[1].w %s;\n",
              (uid_data->ztex_op == ZTEXTURE_ADD) ? "+ zCoord" : "");
    out.Write("\tzCoord = zCoord & 0xFFFFFF;\n");
  }

  if (uid_data->per_pixel_depth && uid_data->late_ztest)
  {
    if (ApiType == APIType::D3D || ApiType == APIType::Vulkan)
      out.Write("\tdepth = 1.0 - float(zCoord) / 16777216.0;\n");
    else
      out.Write("\tdepth = float(zCoord) / 16777216.0;\n");
  }

  // No dithering for RGB8 mode
  if (uid_data->dither)
  {
    // Flipper uses a standard 2x2 Bayer Matrix for 6 bit dithering
    // Here the matrix is encoded into the two factor constants
    out.Write("\tint2 dither = int2(rawpos.xy) & 1;\n");
    out.Write("\tprev.rgb = (prev.rgb - (prev.rgb >> 6)) + abs(dither.y * 3 - dither.x * 2);\n");
  }

  WriteFog(out, uid_data);

  // Write the color and alpha values to the framebuffer
  WriteColor(out, uid_data, use_dual_source);

  if (uid_data->bounding_box)
  {
    const char* atomic_op =
        (ApiType == APIType::OpenGL || ApiType == APIType::Vulkan) ? "atomic" : "Interlocked";
    out.Write("\tif(bbox_data[0] > int(rawpos.x)) %sMin(bbox_data[0], int(rawpos.x));\n"
              "\tif(bbox_data[1] < int(rawpos.x)) %sMax(bbox_data[1], int(rawpos.x));\n"
              "\tif(bbox_data[2] > int(rawpos.y)) %sMin(bbox_data[2], int(rawpos.y));\n"
              "\tif(bbox_data[3] < int(rawpos.y)) %sMax(bbox_data[3], int(rawpos.y));\n",
              atomic_op, atomic_op, atomic_op, atomic_op);
  }

  out.Write("}\n");

  return out;
}

static void WriteStage(ShaderCode& out, const pixel_shader_uid_data* uid_data, int n,
                       APIType ApiType, bool stereo)
{
  auto& stage = uid_data->stagehash[n];
  out.Write("\n\t// TEV stage %d\n", n);

  // HACK to handle cases where the tex gen is not enabled
  u32 texcoord = stage.tevorders_texcoord;
  bool bHasTexCoord = texcoord < uid_data->genMode_numtexgens;
  if (!bHasTexCoord)
    texcoord = 0;

  if (stage.hasindstage)
  {
    TevStageIndirect tevind;
    tevind.hex = stage.tevind;

    out.Write("\t// indirect op\n");
    // perform the indirect op on the incoming regular coordinates using iindtex%d as the offset
    // coords
    if (tevind.bs != ITBA_OFF)
    {
      const char* tevIndAlphaSel[] = {"", "x", "y", "z"};
      const char* tevIndAlphaMask[] = {"248", "224", "240",
                                       "248"};  // 0b11111000, 0b11100000, 0b11110000, 0b11111000
      out.Write("alphabump = iindtex%d.%s & %s;\n", tevind.bt.Value(), tevIndAlphaSel[tevind.bs],
                tevIndAlphaMask[tevind.fmt]);
    }
    else
    {
      // TODO: Should we reset alphabump to 0 here?
    }

    if (tevind.mid != 0)
    {
      // format
      const char* tevIndFmtMask[] = {"255", "31", "15", "7"};
      out.Write("\tint3 iindtevcrd%d = iindtex%d & %s;\n", n, tevind.bt.Value(),
                tevIndFmtMask[tevind.fmt]);

      // bias - TODO: Check if this needs to be this complicated..
      const char* tevIndBiasField[] = {"",  "x",  "y",  "xy",
                                       "z", "xz", "yz", "xyz"};  // indexed by bias
      const char* tevIndBiasAdd[] = {"-128", "1", "1", "1"};     // indexed by fmt
      if (tevind.bias == ITB_S || tevind.bias == ITB_T || tevind.bias == ITB_U)
        out.Write("\tiindtevcrd%d.%s += int(%s);\n", n, tevIndBiasField[tevind.bias],
                  tevIndBiasAdd[tevind.fmt]);
      else if (tevind.bias == ITB_ST || tevind.bias == ITB_SU || tevind.bias == ITB_TU)
        out.Write("\tiindtevcrd%d.%s += int2(%s, %s);\n", n, tevIndBiasField[tevind.bias],
                  tevIndBiasAdd[tevind.fmt], tevIndBiasAdd[tevind.fmt]);
      else if (tevind.bias == ITB_STU)
        out.Write("\tiindtevcrd%d.%s += int3(%s, %s, %s);\n", n, tevIndBiasField[tevind.bias],
                  tevIndBiasAdd[tevind.fmt], tevIndBiasAdd[tevind.fmt], tevIndBiasAdd[tevind.fmt]);

      // multiply by offset matrix and scale - calculations are likely to overflow badly,
      // yet it works out since we only care about the lower 23 bits (+1 sign bit) of the result
      if (tevind.mid <= 3)
      {
        int mtxidx = 2 * (tevind.mid - 1);
        out.SetConstantsUsed(C_INDTEXMTX + mtxidx, C_INDTEXMTX + mtxidx);

        out.Write("\tint2 indtevtrans%d = int2(idot(" I_INDTEXMTX
                  "[%d].xyz, iindtevcrd%d), idot(" I_INDTEXMTX "[%d].xyz, iindtevcrd%d)) >> 3;\n",
                  n, mtxidx, n, mtxidx + 1, n);

        // TODO: should use a shader uid branch for this for better performance
        if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_BITWISE_OP_NEGATION))
        {
          out.Write("\tint indtexmtx_w_inverse_%d = -" I_INDTEXMTX "[%d].w;\n", n, mtxidx);
          out.Write("\tif (" I_INDTEXMTX "[%d].w >= 0) indtevtrans%d >>= " I_INDTEXMTX "[%d].w;\n",
                    mtxidx, n, mtxidx);
          out.Write("\telse indtevtrans%d <<= indtexmtx_w_inverse_%d;\n", n, n);
        }
        else
        {
          out.Write("\tif (" I_INDTEXMTX "[%d].w >= 0) indtevtrans%d >>= " I_INDTEXMTX "[%d].w;\n",
                    mtxidx, n, mtxidx);
          out.Write("\telse indtevtrans%d <<= (-" I_INDTEXMTX "[%d].w);\n", n, mtxidx);
        }
      }
      else if (tevind.mid <= 7 && bHasTexCoord)
      {  // s matrix
        _assert_(tevind.mid >= 5);
        int mtxidx = 2 * (tevind.mid - 5);
        out.SetConstantsUsed(C_INDTEXMTX + mtxidx, C_INDTEXMTX + mtxidx);

        out.Write("\tint2 indtevtrans%d = int2(fixpoint_uv%d * iindtevcrd%d.xx) >> 8;\n", n,
                  texcoord, n);
        if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_BITWISE_OP_NEGATION))
        {
          out.Write("\tint  indtexmtx_w_inverse_%d = -" I_INDTEXMTX "[%d].w;\n", n, mtxidx);
          out.Write("\tif (" I_INDTEXMTX "[%d].w >= 0) indtevtrans%d >>= " I_INDTEXMTX "[%d].w;\n",
                    mtxidx, n, mtxidx);
          out.Write("\telse indtevtrans%d <<= (indtexmtx_w_inverse_%d);\n", n, n);
        }
        else
        {
          out.Write("\tif (" I_INDTEXMTX "[%d].w >= 0) indtevtrans%d >>= " I_INDTEXMTX "[%d].w;\n",
                    mtxidx, n, mtxidx);
          out.Write("\telse indtevtrans%d <<= (-" I_INDTEXMTX "[%d].w);\n", n, mtxidx);
        }
      }
      else if (tevind.mid <= 11 && bHasTexCoord)
      {  // t matrix
        _assert_(tevind.mid >= 9);
        int mtxidx = 2 * (tevind.mid - 9);
        out.SetConstantsUsed(C_INDTEXMTX + mtxidx, C_INDTEXMTX + mtxidx);

        out.Write("\tint2 indtevtrans%d = int2(fixpoint_uv%d * iindtevcrd%d.yy) >> 8;\n", n,
                  texcoord, n);

        if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_BITWISE_OP_NEGATION))
        {
          out.Write("\tint  indtexmtx_w_inverse_%d = -" I_INDTEXMTX "[%d].w;\n", n, mtxidx);
          out.Write("\tif (" I_INDTEXMTX "[%d].w >= 0) indtevtrans%d >>= " I_INDTEXMTX "[%d].w;\n",
                    mtxidx, n, mtxidx);
          out.Write("\telse indtevtrans%d <<= (indtexmtx_w_inverse_%d);\n", n, n);
        }
        else
        {
          out.Write("\tif (" I_INDTEXMTX "[%d].w >= 0) indtevtrans%d >>= " I_INDTEXMTX "[%d].w;\n",
                    mtxidx, n, mtxidx);
          out.Write("\telse indtevtrans%d <<= (-" I_INDTEXMTX "[%d].w);\n", n, mtxidx);
        }
      }
      else
      {
        out.Write("\tint2 indtevtrans%d = int2(0, 0);\n", n);
      }
    }
    else
    {
      out.Write("\tint2 indtevtrans%d = int2(0, 0);\n", n);
    }

    // ---------
    // Wrapping
    // ---------
    const char* tevIndWrapStart[] = {
        "0",       "(256<<7)", "(128<<7)", "(64<<7)",
        "(32<<7)", "(16<<7)",  "1"};  // TODO: Should the last one be 1 or (1<<7)?

    // wrap S
    if (tevind.sw == ITW_OFF)
      out.Write("\twrappedcoord.x = fixpoint_uv%d.x;\n", texcoord);
    else if (tevind.sw == ITW_0)
      out.Write("\twrappedcoord.x = 0;\n");
    else
      out.Write("\twrappedcoord.x = fixpoint_uv%d.x & (%s - 1);\n", texcoord,
                tevIndWrapStart[tevind.sw]);

    // wrap T
    if (tevind.tw == ITW_OFF)
      out.Write("\twrappedcoord.y = fixpoint_uv%d.y;\n", texcoord);
    else if (tevind.tw == ITW_0)
      out.Write("\twrappedcoord.y = 0;\n");
    else
      out.Write("\twrappedcoord.y = fixpoint_uv%d.y & (%s - 1);\n", texcoord,
                tevIndWrapStart[tevind.tw]);

    if (tevind.fb_addprev)  // add previous tevcoord
      out.Write("\ttevcoord.xy += wrappedcoord + indtevtrans%d;\n", n);
    else
      out.Write("\ttevcoord.xy = wrappedcoord + indtevtrans%d;\n", n);

    // Emulate s24 overflows
    out.Write("\ttevcoord.xy = (tevcoord.xy << 8) >> 8;\n");
  }

  TevStageCombiner::ColorCombiner cc;
  TevStageCombiner::AlphaCombiner ac;
  cc.hex = stage.cc;
  ac.hex = stage.ac;

  if (cc.a == TEVCOLORARG_RASA || cc.a == TEVCOLORARG_RASC || cc.b == TEVCOLORARG_RASA ||
      cc.b == TEVCOLORARG_RASC || cc.c == TEVCOLORARG_RASA || cc.c == TEVCOLORARG_RASC ||
      cc.d == TEVCOLORARG_RASA || cc.d == TEVCOLORARG_RASC || ac.a == TEVALPHAARG_RASA ||
      ac.b == TEVALPHAARG_RASA || ac.c == TEVALPHAARG_RASA || ac.d == TEVALPHAARG_RASA)
  {
    // Generate swizzle string to represent the Ras color channel swapping
    char rasswap[5] = {"rgba"[stage.tevksel_swap1a], "rgba"[stage.tevksel_swap2a],
                       "rgba"[stage.tevksel_swap1b], "rgba"[stage.tevksel_swap2b], '\0'};

    out.Write("\trastemp = %s.%s;\n", tevRasTable[stage.tevorders_colorchan], rasswap);
  }

  if (stage.tevorders_enable)
  {
    // Generate swizzle string to represent the texture color channel swapping
    char texswap[5] = {"rgba"[stage.tevksel_swap1c], "rgba"[stage.tevksel_swap2c],
                       "rgba"[stage.tevksel_swap1d], "rgba"[stage.tevksel_swap2d], '\0'};

    if (!stage.hasindstage)
    {
      // calc tevcord
      if (bHasTexCoord)
        out.Write("\ttevcoord.xy = fixpoint_uv%d;\n", texcoord);
      else
        out.Write("\ttevcoord.xy = int2(0, 0);\n");
    }
    out.Write("\ttextemp = ");
    SampleTexture(out, "float2(tevcoord.xy)", texswap, stage.tevorders_texmap, stereo, ApiType);
  }
  else
  {
    out.Write("\ttextemp = int4(255, 255, 255, 255);\n");
  }

  if (cc.a == TEVCOLORARG_KONST || cc.b == TEVCOLORARG_KONST || cc.c == TEVCOLORARG_KONST ||
      cc.d == TEVCOLORARG_KONST || ac.a == TEVALPHAARG_KONST || ac.b == TEVALPHAARG_KONST ||
      ac.c == TEVALPHAARG_KONST || ac.d == TEVALPHAARG_KONST)
  {
    out.Write("\tkonsttemp = int4(%s, %s);\n", tevKSelTableC[stage.tevksel_kc],
              tevKSelTableA[stage.tevksel_ka]);

    if (stage.tevksel_kc > 7)
      out.SetConstantsUsed(C_KCOLORS + ((stage.tevksel_kc - 0xc) % 4),
                           C_KCOLORS + ((stage.tevksel_kc - 0xc) % 4));
    if (stage.tevksel_ka > 7)
      out.SetConstantsUsed(C_KCOLORS + ((stage.tevksel_ka - 0xc) % 4),
                           C_KCOLORS + ((stage.tevksel_ka - 0xc) % 4));
  }

  if (cc.d == TEVCOLORARG_C0 || cc.d == TEVCOLORARG_A0 || ac.d == TEVALPHAARG_A0)
    out.SetConstantsUsed(C_COLORS + 1, C_COLORS + 1);

  if (cc.d == TEVCOLORARG_C1 || cc.d == TEVCOLORARG_A1 || ac.d == TEVALPHAARG_A1)
    out.SetConstantsUsed(C_COLORS + 2, C_COLORS + 2);

  if (cc.d == TEVCOLORARG_C2 || cc.d == TEVCOLORARG_A2 || ac.d == TEVALPHAARG_A2)
    out.SetConstantsUsed(C_COLORS + 3, C_COLORS + 3);

  if (cc.dest >= GX_TEVREG0)
    out.SetConstantsUsed(C_COLORS + cc.dest, C_COLORS + cc.dest);

  if (ac.dest >= GX_TEVREG0)
    out.SetConstantsUsed(C_COLORS + ac.dest, C_COLORS + ac.dest);

  out.Write("\ttevin_a = int4(%s, %s)&int4(255, 255, 255, 255);\n", tevCInputTable[cc.a],
            tevAInputTable[ac.a]);
  out.Write("\ttevin_b = int4(%s, %s)&int4(255, 255, 255, 255);\n", tevCInputTable[cc.b],
            tevAInputTable[ac.b]);
  out.Write("\ttevin_c = int4(%s, %s)&int4(255, 255, 255, 255);\n", tevCInputTable[cc.c],
            tevAInputTable[ac.c]);
  out.Write("\ttevin_d = int4(%s, %s);\n", tevCInputTable[cc.d], tevAInputTable[ac.d]);

  out.Write("\t// color combine\n");
  out.Write("\t%s = clamp(", tevCOutputTable[cc.dest]);
  if (cc.bias != TEVBIAS_COMPARE)
  {
    WriteTevRegular(out, "rgb", cc.bias, cc.op, cc.clamp, cc.shift, false);
  }
  else
  {
    const char* function_table[] = {
        "((tevin_a.r > tevin_b.r) ? tevin_c.rgb : int3(0,0,0))",   // TEVCMP_R8_GT
        "((tevin_a.r == tevin_b.r) ? tevin_c.rgb : int3(0,0,0))",  // TEVCMP_R8_EQ
        "((idot(tevin_a.rgb, comp16) >  idot(tevin_b.rgb, comp16)) ? tevin_c.rgb : "
        "int3(0,0,0))",  // TEVCMP_GR16_GT
        "((idot(tevin_a.rgb, comp16) == idot(tevin_b.rgb, comp16)) ? tevin_c.rgb : "
        "int3(0,0,0))",  // TEVCMP_GR16_EQ
        "((idot(tevin_a.rgb, comp24) >  idot(tevin_b.rgb, comp24)) ? tevin_c.rgb : "
        "int3(0,0,0))",  // TEVCMP_BGR24_GT
        "((idot(tevin_a.rgb, comp24) == idot(tevin_b.rgb, comp24)) ? tevin_c.rgb : "
        "int3(0,0,0))",                                                         // TEVCMP_BGR24_EQ
        "(max(sign(tevin_a.rgb - tevin_b.rgb), int3(0,0,0)) * tevin_c.rgb)",    // TEVCMP_RGB8_GT
        "((int3(1,1,1) - sign(abs(tevin_a.rgb - tevin_b.rgb))) * tevin_c.rgb)"  // TEVCMP_RGB8_EQ
    };

    int mode = (cc.shift << 1) | cc.op;
    out.Write("   tevin_d.rgb + ");
    out.Write("%s", function_table[mode]);
  }
  if (cc.clamp)
    out.Write(", int3(0,0,0), int3(255,255,255))");
  else
    out.Write(", int3(-1024,-1024,-1024), int3(1023,1023,1023))");
  out.Write(";\n");

  out.Write("\t// alpha combine\n");
  out.Write("\t%s = clamp(", tevAOutputTable[ac.dest]);
  if (ac.bias != TEVBIAS_COMPARE)
  {
    WriteTevRegular(out, "a", ac.bias, ac.op, ac.clamp, ac.shift, true);
  }
  else
  {
    const char* function_table[] = {
        "((tevin_a.r > tevin_b.r) ? tevin_c.a : 0)",   // TEVCMP_R8_GT
        "((tevin_a.r == tevin_b.r) ? tevin_c.a : 0)",  // TEVCMP_R8_EQ
        "((idot(tevin_a.rgb, comp16) >  idot(tevin_b.rgb, comp16)) ? tevin_c.a : 0)",  // TEVCMP_GR16_GT
        "((idot(tevin_a.rgb, comp16) == idot(tevin_b.rgb, comp16)) ? tevin_c.a : 0)",  // TEVCMP_GR16_EQ
        "((idot(tevin_a.rgb, comp24) >  idot(tevin_b.rgb, comp24)) ? tevin_c.a : 0)",  // TEVCMP_BGR24_GT
        "((idot(tevin_a.rgb, comp24) == idot(tevin_b.rgb, comp24)) ? tevin_c.a : 0)",  // TEVCMP_BGR24_EQ
        "((tevin_a.a >  tevin_b.a) ? tevin_c.a : 0)",  // TEVCMP_A8_GT
        "((tevin_a.a == tevin_b.a) ? tevin_c.a : 0)"   // TEVCMP_A8_EQ
    };

    int mode = (ac.shift << 1) | ac.op;
    out.Write("   tevin_d.a + ");
    out.Write("%s", function_table[mode]);
  }
  if (ac.clamp)
    out.Write(", 0, 255)");
  else
    out.Write(", -1024, 1023)");

  out.Write(";\n");
}

static void WriteTevRegular(ShaderCode& out, const char* components, int bias, int op, int clamp,
                            int shift, bool alpha)
{
  const char* tevScaleTableLeft[] = {
      "",       // SCALE_1
      " << 1",  // SCALE_2
      " << 2",  // SCALE_4
      "",       // DIVIDE_2
  };

  const char* tevScaleTableRight[] = {
      "",       // SCALE_1
      "",       // SCALE_2
      "",       // SCALE_4
      " >> 1",  // DIVIDE_2
  };

  const char* tevLerpBias[] =  // indexed by 2*op+(shift==3)
      {
          "", " + 128", "", " + 127",
      };

  const char* tevBiasTable[] = {
      "",        // ZERO,
      " + 128",  // ADDHALF,
      " - 128",  // SUBHALF,
      "",
  };

  const char* tevOpTable[] = {
      "+",  // TEVOP_ADD = 0,
      "-",  // TEVOP_SUB = 1,
  };

  // Regular TEV stage: (d + bias + lerp(a,b,c)) * scale
  // The GameCube/Wii GPU uses a very sophisticated algorithm for scale-lerping:
  // - c is scaled from 0..255 to 0..256, which allows dividing the result by 256 instead of 255
  // - if scale is bigger than one, it is moved inside the lerp calculation for increased accuracy
  // - a rounding bias is added before dividing by 256
  out.Write("(((tevin_d.%s%s)%s)", components, tevBiasTable[bias], tevScaleTableLeft[shift]);
  out.Write(" %s ", tevOpTable[op]);
  out.Write("(((((tevin_a.%s<<8) + (tevin_b.%s-tevin_a.%s)*(tevin_c.%s+(tevin_c.%s>>7)))%s)%s)>>8)",
            components, components, components, components, components, tevScaleTableLeft[shift],
            tevLerpBias[2 * op + ((shift == 3) == alpha)]);
  out.Write(")%s", tevScaleTableRight[shift]);
}

static void SampleTexture(ShaderCode& out, const char* texcoords, const char* texswap, int texmap,
                          bool stereo, APIType ApiType)
{
  out.SetConstantsUsed(C_TEXDIMS + texmap, C_TEXDIMS + texmap);

  if (ApiType == APIType::D3D)
  {
    out.Write("iround(255.0 * Tex[%d].Sample(samp[%d], float3(%s.xy * " I_TEXDIMS
              "[%d].xy, %s))).%s;\n",
              texmap, texmap, texcoords, texmap, stereo ? "layer" : "0.0", texswap);
  }
  else
  {
    out.Write("iround(255.0 * texture(samp[%d], float3(%s.xy * " I_TEXDIMS "[%d].xy, %s))).%s;\n",
              texmap, texcoords, texmap, stereo ? "layer" : "0.0", texswap);
  }
}

static const char* tevAlphaFuncsTable[] = {
    "(false)",         // NEVER
    "(prev.a <  %s)",  // LESS
    "(prev.a == %s)",  // EQUAL
    "(prev.a <= %s)",  // LEQUAL
    "(prev.a >  %s)",  // GREATER
    "(prev.a != %s)",  // NEQUAL
    "(prev.a >= %s)",  // GEQUAL
    "(true)"           // ALWAYS
};

static const char* tevAlphaFunclogicTable[] = {
    " && ",  // and
    " || ",  // or
    " != ",  // xor
    " == "   // xnor
};

static void WriteAlphaTest(ShaderCode& out, const pixel_shader_uid_data* uid_data, APIType ApiType,
                           bool per_pixel_depth, bool use_dual_source)
{
  static const char* alphaRef[2] = {I_ALPHA ".r", I_ALPHA ".g"};

  out.SetConstantsUsed(C_ALPHA, C_ALPHA);

  if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_NEGATED_BOOLEAN))
    out.Write("\tif(( ");
  else
    out.Write("\tif(!( ");

  // Lookup the first component from the alpha function table
  int compindex = uid_data->alpha_test_comp0;
  out.Write(tevAlphaFuncsTable[compindex], alphaRef[0]);

  out.Write("%s", tevAlphaFunclogicTable[uid_data->alpha_test_logic]);  // lookup the logic op

  // Lookup the second component from the alpha function table
  compindex = uid_data->alpha_test_comp1;
  out.Write(tevAlphaFuncsTable[compindex], alphaRef[1]);

  if (DriverDetails::HasBug(DriverDetails::BUG_BROKEN_NEGATED_BOOLEAN))
    out.Write(") == false) {\n");
  else
    out.Write(")) {\n");

  out.Write("\t\tocol0 = float4(0.0, 0.0, 0.0, 0.0);\n");
  if (use_dual_source)
    out.Write("\t\tocol1 = float4(0.0, 0.0, 0.0, 0.0);\n");
  if (per_pixel_depth)
  {
    out.Write("\t\tdepth = %s;\n",
              (ApiType == APIType::D3D || ApiType == APIType::Vulkan) ? "0.0" : "1.0");
  }

  // ZCOMPLOC HACK:
  if (!uid_data->alpha_test_use_zcomploc_hack)
  {
    out.Write("\t\tdiscard;\n");
    if (ApiType != APIType::D3D)
      out.Write("\t\treturn;\n");
  }

  out.Write("\t}\n");
}

static const char* tevFogFuncsTable[] = {
    "",                                                       // No Fog
    "",                                                       // ?
    "",                                                       // Linear
    "",                                                       // ?
    "\tfog = 1.0 - exp2(-8.0 * fog);\n",                      // exp
    "\tfog = 1.0 - exp2(-8.0 * fog * fog);\n",                // exp2
    "\tfog = exp2(-8.0 * (1.0 - fog));\n",                    // backward exp
    "\tfog = 1.0 - fog;\n   fog = exp2(-8.0 * fog * fog);\n"  // backward exp2
};

static void WriteFog(ShaderCode& out, const pixel_shader_uid_data* uid_data)
{
  if (uid_data->fog_fsel == 0)
    return;  // no Fog

  out.SetConstantsUsed(C_FOGCOLOR, C_FOGCOLOR);
  out.SetConstantsUsed(C_FOGI, C_FOGI);
  out.SetConstantsUsed(C_FOGF, C_FOGF + 1);
  if (uid_data->fog_proj == 0)
  {
    // perspective
    // ze = A/(B - (Zs >> B_SHF)
    // TODO: Verify that we want to drop lower bits here! (currently taken over from software
    // renderer)
    //       Maybe we want to use "ze = (A << B_SHF)/((B << B_SHF) - Zs)" instead?
    //       That's equivalent, but keeps the lower bits of Zs.
    out.Write("\tfloat ze = (" I_FOGF "[1].x * 16777216.0) / float(" I_FOGI
              ".y - (zCoord >> " I_FOGI ".w));\n");
  }
  else
  {
    // orthographic
    // ze = a*Zs    (here, no B_SHF)
    out.Write("\tfloat ze = " I_FOGF "[1].x * float(zCoord) / 16777216.0;\n");
  }

  // x_adjust = sqrt((x-center)^2 + k^2)/k
  // ze *= x_adjust
  // TODO Instead of this theoretical calculation, we should use the
  //      coefficient table given in the fog range BP registers!
  if (uid_data->fog_RangeBaseEnabled)
  {
    out.SetConstantsUsed(C_FOGF, C_FOGF);
    out.Write("\tfloat x_adjust = (2.0 * (rawpos.x / " I_FOGF "[0].y)) - 1.0 - " I_FOGF "[0].x;\n");
    out.Write("\tx_adjust = sqrt(x_adjust * x_adjust + " I_FOGF "[0].z * " I_FOGF "[0].z) / " I_FOGF
              "[0].z;\n");
    out.Write("\tze *= x_adjust;\n");
  }

  out.Write("\tfloat fog = clamp(ze - " I_FOGF "[1].z, 0.0, 1.0);\n");

  if (uid_data->fog_fsel > 3)
  {
    out.Write("%s", tevFogFuncsTable[uid_data->fog_fsel]);
  }
  else
  {
    if (uid_data->fog_fsel != 2)
      WARN_LOG(VIDEO, "Unknown Fog Type! %08x", uid_data->fog_fsel);
  }

  out.Write("\tint ifog = iround(fog * 256.0);\n");
  out.Write("\tprev.rgb = (prev.rgb * (256 - ifog) + " I_FOGCOLOR ".rgb * ifog) >> 8;\n");
}

static void WriteColor(ShaderCode& out, const pixel_shader_uid_data* uid_data, bool use_dual_source)
{
  if (uid_data->rgba6_format)
    out.Write("\tocol0.rgb = float3(prev.rgb >> 2) / 63.0;\n");
  else
    out.Write("\tocol0.rgb = float3(prev.rgb) / 255.0;\n");

  // Colors will be blended against the 8-bit alpha from ocol1 and
  // the 6-bit alpha from ocol0 will be written to the framebuffer
  if (!uid_data->useDstAlpha)
  {
    out.Write("\tocol0.a = float(prev.a >> 2) / 63.0;\n");
    if (use_dual_source)
      out.Write("\tocol1.a = float(prev.a) / 255.0;\n");
  }
  else
  {
    out.SetConstantsUsed(C_ALPHA, C_ALPHA);
    out.Write("\tocol0.a = float(" I_ALPHA ".a >> 2) / 63.0;\n");

    // Use dual-source color blending to perform dst alpha in a single pass
    if (use_dual_source)
    {
      if (uid_data->useDstAlpha)
        out.Write("\tocol1.a = float(prev.a) / 255.0;\n");
      else
        out.Write("\tocol1.a = float(" I_ALPHA ".a) / 255.0;\n");
    }
  }
}
