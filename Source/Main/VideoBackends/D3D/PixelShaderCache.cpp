// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/Host.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/PixelShaderCache.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
ID3D11PixelShader* s_ColorCopyProgram[2] = {nullptr};
ID3D11PixelShader* s_ClearProgram = nullptr;
ID3D11PixelShader* s_AnaglyphProgram = nullptr;
ID3D11PixelShader* s_DepthResolveProgram = nullptr;
ID3D11PixelShader* s_rgba6_to_rgb8[2] = {nullptr};
ID3D11PixelShader* s_rgb8_to_rgba6[2] = {nullptr};
ID3D11Buffer* pscbuf = nullptr;

const char clear_program_code[] = {"void main(\n"
                                   "out float4 ocol0 : SV_Target,\n"
                                   "in float4 pos : SV_Position,\n"
                                   "in float4 incol0 : COLOR0){\n"
                                   "ocol0 = incol0;\n"
                                   "}\n"};

// TODO: Find some way to avoid having separate shaders for non-MSAA and MSAA...
const char color_copy_program_code[] = {"sampler samp0 : register(s0);\n"
                                        "Texture2DArray Tex0 : register(t0);\n"
                                        "void main(\n"
                                        "out float4 ocol0 : SV_Target,\n"
                                        "in float4 pos : SV_Position,\n"
                                        "in float3 uv0 : TEXCOORD0){\n"
                                        "ocol0 = Tex0.Sample(samp0,uv0);\n"
                                        "}\n"};

// Anaglyph Red-Cyan shader based on Dubois algorithm
// Constants taken from the paper:
// "Conversion of a Stereo Pair to Anaglyph with
// the Least-Squares Projection Method"
// Eric Dubois, March 2009
const char anaglyph_program_code[] = {"sampler samp0 : register(s0);\n"
                                      "Texture2DArray Tex0 : register(t0);\n"
                                      "void main(\n"
                                      "out float4 ocol0 : SV_Target,\n"
                                      "in float4 pos : SV_Position,\n"
                                      "in float3 uv0 : TEXCOORD0){\n"
                                      "float4 c0 = Tex0.Sample(samp0, float3(uv0.xy, 0.0));\n"
                                      "float4 c1 = Tex0.Sample(samp0, float3(uv0.xy, 1.0));\n"
                                      "float3x3 l = float3x3( 0.437, 0.449, 0.164,\n"
                                      "                      -0.062,-0.062,-0.024,\n"
                                      "                      -0.048,-0.050,-0.017);\n"
                                      "float3x3 r = float3x3(-0.011,-0.032,-0.007,\n"
                                      "                       0.377, 0.761, 0.009,\n"
                                      "                      -0.026,-0.093, 1.234);\n"
                                      "ocol0 = float4(mul(l, c0.rgb) + mul(r, c1.rgb), c0.a);\n"
                                      "}\n"};

// TODO: Improve sampling algorithm!
const char color_copy_program_code_msaa[] = {
    "#define SAMPLES %d\n"
    "sampler samp0 : register(s0);\n"
    "Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
    "void main(\n"
    "out float4 ocol0 : SV_Target,\n"
    "in float4 pos : SV_Position,\n"
    "in float3 uv0 : TEXCOORD0){\n"
    "int width, height, slices, samples;\n"
    "Tex0.GetDimensions(width, height, slices, samples);\n"
    "ocol0 = 0;\n"
    "for(int i = 0; i < SAMPLES; ++i)\n"
    "	ocol0 += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
    "ocol0 /= SAMPLES;\n"
    "}\n"};

const char depth_resolve_program[] = {
    "#define SAMPLES %d\n"
    "Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
    "void main(\n"
    "	 out float ocol0 : SV_Target,\n"
    "    in float4 pos : SV_Position,\n"
    "    in float3 uv0 : TEXCOORD0)\n"
    "{\n"
    "	int width, height, slices, samples;\n"
    "	Tex0.GetDimensions(width, height, slices, samples);\n"
    "	ocol0 = Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), 0).x;\n"
    "	for(int i = 1; i < SAMPLES; ++i)\n"
    "		ocol0 = min(ocol0, Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i).x);\n"
    "}\n"};

const char reint_rgba6_to_rgb8[] = {"sampler samp0 : register(s0);\n"
                                    "Texture2DArray Tex0 : register(t0);\n"
                                    "void main(\n"
                                    "	out float4 ocol0 : SV_Target,\n"
                                    "	in float4 pos : SV_Position,\n"
                                    "	in float3 uv0 : TEXCOORD0)\n"
                                    "{\n"
                                    "	int4 src6 = round(Tex0.Sample(samp0,uv0) * 63.f);\n"
                                    "	int4 dst8;\n"
                                    "	dst8.r = (src6.r << 2) | (src6.g >> 4);\n"
                                    "	dst8.g = ((src6.g & 0xF) << 4) | (src6.b >> 2);\n"
                                    "	dst8.b = ((src6.b & 0x3) << 6) | src6.a;\n"
                                    "	dst8.a = 255;\n"
                                    "	ocol0 = (float4)dst8 / 255.f;\n"
                                    "}"};

const char reint_rgba6_to_rgb8_msaa[] = {
    "#define SAMPLES %d\n"
    "sampler samp0 : register(s0);\n"
    "Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
    "void main(\n"
    "	out float4 ocol0 : SV_Target,\n"
    "	in float4 pos : SV_Position,\n"
    "	in float3 uv0 : TEXCOORD0)\n"
    "{\n"
    "	int width, height, slices, samples;\n"
    "	Tex0.GetDimensions(width, height, slices, samples);\n"
    "	float4 texcol = 0;\n"
    "	for (int i = 0; i < SAMPLES; ++i)\n"
    "		texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
    "	texcol /= SAMPLES;\n"
    "	int4 src6 = round(texcol * 63.f);\n"
    "	int4 dst8;\n"
    "	dst8.r = (src6.r << 2) | (src6.g >> 4);\n"
    "	dst8.g = ((src6.g & 0xF) << 4) | (src6.b >> 2);\n"
    "	dst8.b = ((src6.b & 0x3) << 6) | src6.a;\n"
    "	dst8.a = 255;\n"
    "	ocol0 = (float4)dst8 / 255.f;\n"
    "}"};

const char reint_rgb8_to_rgba6[] = {"sampler samp0 : register(s0);\n"
                                    "Texture2DArray Tex0 : register(t0);\n"
                                    "void main(\n"
                                    "	out float4 ocol0 : SV_Target,\n"
                                    "	in float4 pos : SV_Position,\n"
                                    "	in float3 uv0 : TEXCOORD0)\n"
                                    "{\n"
                                    "	int4 src8 = round(Tex0.Sample(samp0,uv0) * 255.f);\n"
                                    "	int4 dst6;\n"
                                    "	dst6.r = src8.r >> 2;\n"
                                    "	dst6.g = ((src8.r & 0x3) << 4) | (src8.g >> 4);\n"
                                    "	dst6.b = ((src8.g & 0xF) << 2) | (src8.b >> 6);\n"
                                    "	dst6.a = src8.b & 0x3F;\n"
                                    "	ocol0 = (float4)dst6 / 63.f;\n"
                                    "}\n"};

const char reint_rgb8_to_rgba6_msaa[] = {
    "#define SAMPLES %d\n"
    "sampler samp0 : register(s0);\n"
    "Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
    "void main(\n"
    "	out float4 ocol0 : SV_Target,\n"
    "	in float4 pos : SV_Position,\n"
    "	in float3 uv0 : TEXCOORD0)\n"
    "{\n"
    "	int width, height, slices, samples;\n"
    "	Tex0.GetDimensions(width, height, slices, samples);\n"
    "	float4 texcol = 0;\n"
    "	for (int i = 0; i < SAMPLES; ++i)\n"
    "		texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
    "	texcol /= SAMPLES;\n"
    "	int4 src8 = round(texcol * 255.f);\n"
    "	int4 dst6;\n"
    "	dst6.r = src8.r >> 2;\n"
    "	dst6.g = ((src8.r & 0x3) << 4) | (src8.g >> 4);\n"
    "	dst6.b = ((src8.g & 0xF) << 2) | (src8.b >> 6);\n"
    "	dst6.a = src8.b & 0x3F;\n"
    "	ocol0 = (float4)dst6 / 63.f;\n"
    "}\n"};

ID3D11PixelShader* PixelShaderCache::ReinterpRGBA6ToRGB8(bool multisampled)
{
  if (!multisampled || g_ActiveConfig.iMultisamples <= 1)
  {
    if (!s_rgba6_to_rgb8[0])
    {
      s_rgba6_to_rgb8[0] = D3D::CompileAndCreatePixelShader(reint_rgba6_to_rgb8);
      CHECK(s_rgba6_to_rgb8[0], "Create RGBA6 to RGB8 pixel shader");
      D3D::SetDebugObjectName(s_rgba6_to_rgb8[0], "RGBA6 to RGB8 pixel shader");
    }
    return s_rgba6_to_rgb8[0];
  }
  else if (!s_rgba6_to_rgb8[1])
  {
    // create MSAA shader for current AA mode
    std::string buf = StringFromFormat(reint_rgba6_to_rgb8_msaa, g_ActiveConfig.iMultisamples);
    s_rgba6_to_rgb8[1] = D3D::CompileAndCreatePixelShader(buf);

    CHECK(s_rgba6_to_rgb8[1], "Create RGBA6 to RGB8 MSAA pixel shader");
    D3D::SetDebugObjectName(s_rgba6_to_rgb8[1], "RGBA6 to RGB8 MSAA pixel shader");
  }
  return s_rgba6_to_rgb8[1];
}

ID3D11PixelShader* PixelShaderCache::ReinterpRGB8ToRGBA6(bool multisampled)
{
  if (!multisampled || g_ActiveConfig.iMultisamples <= 1)
  {
    if (!s_rgb8_to_rgba6[0])
    {
      s_rgb8_to_rgba6[0] = D3D::CompileAndCreatePixelShader(reint_rgb8_to_rgba6);
      CHECK(s_rgb8_to_rgba6[0], "Create RGB8 to RGBA6 pixel shader");
      D3D::SetDebugObjectName(s_rgb8_to_rgba6[0], "RGB8 to RGBA6 pixel shader");
    }
    return s_rgb8_to_rgba6[0];
  }
  else if (!s_rgb8_to_rgba6[1])
  {
    // create MSAA shader for current AA mode
    std::string buf = StringFromFormat(reint_rgb8_to_rgba6_msaa, g_ActiveConfig.iMultisamples);
    s_rgb8_to_rgba6[1] = D3D::CompileAndCreatePixelShader(buf);

    CHECK(s_rgb8_to_rgba6[1], "Create RGB8 to RGBA6 MSAA pixel shader");
    D3D::SetDebugObjectName(s_rgb8_to_rgba6[1], "RGB8 to RGBA6 MSAA pixel shader");
  }
  return s_rgb8_to_rgba6[1];
}

ID3D11PixelShader* PixelShaderCache::GetColorCopyProgram(bool multisampled)
{
  if (!multisampled || g_ActiveConfig.iMultisamples <= 1)
  {
    return s_ColorCopyProgram[0];
  }
  else if (s_ColorCopyProgram[1])
  {
    return s_ColorCopyProgram[1];
  }
  else
  {
    // create MSAA shader for current AA mode
    std::string buf = StringFromFormat(color_copy_program_code_msaa, g_ActiveConfig.iMultisamples);
    s_ColorCopyProgram[1] = D3D::CompileAndCreatePixelShader(buf);
    CHECK(s_ColorCopyProgram[1] != nullptr, "Create color copy MSAA pixel shader");
    D3D::SetDebugObjectName(s_ColorCopyProgram[1], "color copy MSAA pixel shader");
    return s_ColorCopyProgram[1];
  }
}

ID3D11PixelShader* PixelShaderCache::GetClearProgram()
{
  return s_ClearProgram;
}

ID3D11PixelShader* PixelShaderCache::GetAnaglyphProgram()
{
  return s_AnaglyphProgram;
}

ID3D11PixelShader* PixelShaderCache::GetDepthResolveProgram()
{
  if (s_DepthResolveProgram != nullptr)
    return s_DepthResolveProgram;

  // create MSAA shader for current AA mode
  std::string buf = StringFromFormat(depth_resolve_program, g_ActiveConfig.iMultisamples);
  s_DepthResolveProgram = D3D::CompileAndCreatePixelShader(buf);
  CHECK(s_DepthResolveProgram != nullptr, "Create depth matrix MSAA pixel shader");
  D3D::SetDebugObjectName(s_DepthResolveProgram, "depth resolve pixel shader");
  return s_DepthResolveProgram;
}

static void UpdateConstantBuffers()
{
  if (PixelShaderManager::dirty)
  {
    D3D11_MAPPED_SUBRESOURCE map;
    D3D::context->Map(pscbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy(map.pData, &PixelShaderManager::constants, sizeof(PixelShaderConstants));
    D3D::context->Unmap(pscbuf, 0);
    PixelShaderManager::dirty = false;

    ADDSTAT(stats.thisFrame.bytesUniformStreamed, sizeof(PixelShaderConstants));
  }
}

ID3D11Buffer* PixelShaderCache::GetConstantBuffer()
{
  UpdateConstantBuffers();
  return pscbuf;
}

void PixelShaderCache::Init()
{
  unsigned int cbsize = Common::AlignUp(static_cast<unsigned int>(sizeof(PixelShaderConstants)),
                                        16);  // must be a multiple of 16
  D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(cbsize, D3D11_BIND_CONSTANT_BUFFER,
                                                D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
  D3D::device->CreateBuffer(&cbdesc, nullptr, &pscbuf);
  CHECK(pscbuf != nullptr, "Create pixel shader constant buffer");
  D3D::SetDebugObjectName(pscbuf, "pixel shader constant buffer used to emulate the GX pipeline");

  // used when drawing clear quads
  s_ClearProgram = D3D::CompileAndCreatePixelShader(clear_program_code);
  CHECK(s_ClearProgram != nullptr, "Create clear pixel shader");
  D3D::SetDebugObjectName(s_ClearProgram, "clear pixel shader");

  // used for anaglyph stereoscopy
  s_AnaglyphProgram = D3D::CompileAndCreatePixelShader(anaglyph_program_code);
  CHECK(s_AnaglyphProgram != nullptr, "Create anaglyph pixel shader");
  D3D::SetDebugObjectName(s_AnaglyphProgram, "anaglyph pixel shader");

  // used when copying/resolving the color buffer
  s_ColorCopyProgram[0] = D3D::CompileAndCreatePixelShader(color_copy_program_code);
  CHECK(s_ColorCopyProgram[0] != nullptr, "Create color copy pixel shader");
  D3D::SetDebugObjectName(s_ColorCopyProgram[0], "color copy pixel shader");
}

// Used in Swap() when AA mode has changed
void PixelShaderCache::InvalidateMSAAShaders()
{
  SAFE_RELEASE(s_ColorCopyProgram[1]);
  SAFE_RELEASE(s_rgb8_to_rgba6[1]);
  SAFE_RELEASE(s_rgba6_to_rgb8[1]);
  SAFE_RELEASE(s_DepthResolveProgram);
}

void PixelShaderCache::Shutdown()
{
  SAFE_RELEASE(pscbuf);

  SAFE_RELEASE(s_ClearProgram);
  SAFE_RELEASE(s_AnaglyphProgram);
  SAFE_RELEASE(s_DepthResolveProgram);
  for (int i = 0; i < 2; ++i)
  {
    SAFE_RELEASE(s_ColorCopyProgram[i]);
    SAFE_RELEASE(s_rgba6_to_rgb8[i]);
    SAFE_RELEASE(s_rgb8_to_rgba6[i]);
  }
}
}  // DX11
