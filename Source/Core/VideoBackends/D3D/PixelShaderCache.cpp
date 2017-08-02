// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/Align.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"
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
PixelShaderCache::PSCache PixelShaderCache::PixelShaders;
PixelShaderCache::UberPSCache PixelShaderCache::UberPixelShaders;
const PixelShaderCache::PSCacheEntry* PixelShaderCache::last_entry;
const PixelShaderCache::PSCacheEntry* PixelShaderCache::last_uber_entry;
PixelShaderUid PixelShaderCache::last_uid;
UberShader::PixelShaderUid PixelShaderCache::last_uber_uid;

LinearDiskCache<PixelShaderUid, u8> g_ps_disk_cache;
LinearDiskCache<UberShader::PixelShaderUid, u8> g_uber_ps_disk_cache;
extern std::unique_ptr<VideoCommon::AsyncShaderCompiler> g_async_compiler;

ID3D11PixelShader* s_ColorMatrixProgram[2] = {nullptr};
ID3D11PixelShader* s_ColorCopyProgram[2] = {nullptr};
ID3D11PixelShader* s_DepthMatrixProgram[2] = {nullptr};
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

const char color_matrix_program_code[] = {"sampler samp0 : register(s0);\n"
                                          "Texture2DArray Tex0 : register(t0);\n"
                                          "uniform float4 cColMatrix[7] : register(c0);\n"
                                          "void main(\n"
                                          "out float4 ocol0 : SV_Target,\n"
                                          "in float4 pos : SV_Position,\n"
                                          "in float3 uv0 : TEXCOORD0){\n"
                                          "float4 texcol = Tex0.Sample(samp0,uv0);\n"
                                          "texcol = floor(texcol * cColMatrix[5])*cColMatrix[6];\n"
                                          "ocol0 = "
                                          "float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix["
                                          "1]),dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3]))"
                                          " + cColMatrix[4];\n"
                                          "}\n"};

const char color_matrix_program_code_msaa[] = {
    "#define SAMPLES %d\n"
    "sampler samp0 : register(s0);\n"
    "Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
    "uniform float4 cColMatrix[7] : register(c0);\n"
    "void main(\n"
    "out float4 ocol0 : SV_Target,\n"
    "in float4 pos : SV_Position,\n"
    "in float3 uv0 : TEXCOORD0){\n"
    "int width, height, slices, samples;\n"
    "Tex0.GetDimensions(width, height, slices, samples);\n"
    "float4 texcol = 0;\n"
    "for(int i = 0; i < SAMPLES; ++i)\n"
    "	texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
    "texcol /= SAMPLES;\n"
    "texcol = floor(texcol * cColMatrix[5])*cColMatrix[6];\n"
    "ocol0 = "
    "float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot("
    "texcol,cColMatrix[3])) + cColMatrix[4];\n"
    "}\n"};

const char depth_matrix_program[] = {"sampler samp0 : register(s0);\n"
                                     "Texture2DArray Tex0 : register(t0);\n"
                                     "uniform float4 cColMatrix[7] : register(c0);\n"
                                     "void main(\n"
                                     "out float4 ocol0 : SV_Target,\n"
                                     " in float4 pos : SV_Position,\n"
                                     " in float3 uv0 : TEXCOORD0){\n"
                                     "	float4 texcol = Tex0.Sample(samp0,uv0);\n"
                                     "	int depth = int((1.0 - texcol.x) * 16777216.0);\n"

                                     // Convert to Z24 format
                                     "	int4 workspace;\n"
                                     "	workspace.r = (depth >> 16) & 255;\n"
                                     "	workspace.g = (depth >> 8) & 255;\n"
                                     "	workspace.b = depth & 255;\n"

                                     // Convert to Z4 format
                                     "	workspace.a = (depth >> 16) & 0xF0;\n"

                                     // Normalize components to [0.0..1.0]
                                     "	texcol = float4(workspace) / 255.0;\n"

                                     // Apply color matrix
                                     "	ocol0 = "
                                     "float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),"
                                     "dot(texcol,cColMatrix[2]),dot(texcol,cColMatrix[3])) + "
                                     "cColMatrix[4];\n"
                                     "}\n"};

const char depth_matrix_program_msaa[] = {
    "#define SAMPLES %d\n"
    "sampler samp0 : register(s0);\n"
    "Texture2DMSArray<float4, SAMPLES> Tex0 : register(t0);\n"
    "uniform float4 cColMatrix[7] : register(c0);\n"
    "void main(\n"
    "out float4 ocol0 : SV_Target,\n"
    " in float4 pos : SV_Position,\n"
    " in float3 uv0 : TEXCOORD0){\n"
    "	int width, height, slices, samples;\n"
    "	Tex0.GetDimensions(width, height, slices, samples);\n"
    "	float4 texcol = 0;\n"
    "	for(int i = 0; i < SAMPLES; ++i)\n"
    "		texcol += Tex0.Load(int3(uv0.x*(width), uv0.y*(height), uv0.z), i);\n"
    "	texcol /= SAMPLES;\n"
    "	int depth = int((1.0 - texcol.x) * 16777216.0);\n"

    // Convert to Z24 format
    "	int4 workspace;\n"
    "	workspace.r = (depth >> 16) & 255;\n"
    "	workspace.g = (depth >> 8) & 255;\n"
    "	workspace.b = depth & 255;\n"

    // Convert to Z4 format
    "	workspace.a = (depth >> 16) & 0xF0;\n"

    // Normalize components to [0.0..1.0]
    "	texcol = float4(workspace) / 255.0;\n"

    // Apply color matrix
    "	ocol0 = "
    "float4(dot(texcol,cColMatrix[0]),dot(texcol,cColMatrix[1]),dot(texcol,cColMatrix[2]),dot("
    "texcol,cColMatrix[3])) + cColMatrix[4];\n"
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
    D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ColorCopyProgram[1],
                            "color copy MSAA pixel shader");
    return s_ColorCopyProgram[1];
  }
}

ID3D11PixelShader* PixelShaderCache::GetColorMatrixProgram(bool multisampled)
{
  if (!multisampled || g_ActiveConfig.iMultisamples <= 1)
  {
    return s_ColorMatrixProgram[0];
  }
  else if (s_ColorMatrixProgram[1])
  {
    return s_ColorMatrixProgram[1];
  }
  else
  {
    // create MSAA shader for current AA mode
    std::string buf =
        StringFromFormat(color_matrix_program_code_msaa, g_ActiveConfig.iMultisamples);
    s_ColorMatrixProgram[1] = D3D::CompileAndCreatePixelShader(buf);
    CHECK(s_ColorMatrixProgram[1] != nullptr, "Create color matrix MSAA pixel shader");
    D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ColorMatrixProgram[1],
                            "color matrix MSAA pixel shader");
    return s_ColorMatrixProgram[1];
  }
}

ID3D11PixelShader* PixelShaderCache::GetDepthMatrixProgram(bool multisampled)
{
  if (!multisampled || g_ActiveConfig.iMultisamples <= 1)
  {
    return s_DepthMatrixProgram[0];
  }
  else if (s_DepthMatrixProgram[1])
  {
    return s_DepthMatrixProgram[1];
  }
  else
  {
    // create MSAA shader for current AA mode
    std::string buf = StringFromFormat(depth_matrix_program_msaa, g_ActiveConfig.iMultisamples);
    s_DepthMatrixProgram[1] = D3D::CompileAndCreatePixelShader(buf);
    CHECK(s_DepthMatrixProgram[1] != nullptr, "Create depth matrix MSAA pixel shader");
    D3D::SetDebugObjectName((ID3D11DeviceChild*)s_DepthMatrixProgram[1],
                            "depth matrix MSAA pixel shader");
    return s_DepthMatrixProgram[1];
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
  D3D::SetDebugObjectName((ID3D11DeviceChild*)s_DepthResolveProgram, "depth resolve pixel shader");
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

// this class will load the precompiled shaders into our cache
template <typename UidType>
class PixelShaderCacheInserter : public LinearDiskCacheReader<UidType, u8>
{
public:
  void Read(const UidType& key, const u8* value, u32 value_size)
  {
    PixelShaderCache::InsertByteCode(key, value, value_size);
  }
};

void PixelShaderCache::Init()
{
  unsigned int cbsize = Common::AlignUp(static_cast<unsigned int>(sizeof(PixelShaderConstants)),
                                        16);  // must be a multiple of 16
  D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(cbsize, D3D11_BIND_CONSTANT_BUFFER,
                                                D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
  D3D::device->CreateBuffer(&cbdesc, nullptr, &pscbuf);
  CHECK(pscbuf != nullptr, "Create pixel shader constant buffer");
  D3D::SetDebugObjectName((ID3D11DeviceChild*)pscbuf,
                          "pixel shader constant buffer used to emulate the GX pipeline");

  // used when drawing clear quads
  s_ClearProgram = D3D::CompileAndCreatePixelShader(clear_program_code);
  CHECK(s_ClearProgram != nullptr, "Create clear pixel shader");
  D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ClearProgram, "clear pixel shader");

  // used for anaglyph stereoscopy
  s_AnaglyphProgram = D3D::CompileAndCreatePixelShader(anaglyph_program_code);
  CHECK(s_AnaglyphProgram != nullptr, "Create anaglyph pixel shader");
  D3D::SetDebugObjectName((ID3D11DeviceChild*)s_AnaglyphProgram, "anaglyph pixel shader");

  // used when copying/resolving the color buffer
  s_ColorCopyProgram[0] = D3D::CompileAndCreatePixelShader(color_copy_program_code);
  CHECK(s_ColorCopyProgram[0] != nullptr, "Create color copy pixel shader");
  D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ColorCopyProgram[0], "color copy pixel shader");

  // used for color conversion
  s_ColorMatrixProgram[0] = D3D::CompileAndCreatePixelShader(color_matrix_program_code);
  CHECK(s_ColorMatrixProgram[0] != nullptr, "Create color matrix pixel shader");
  D3D::SetDebugObjectName((ID3D11DeviceChild*)s_ColorMatrixProgram[0], "color matrix pixel shader");

  // used for depth copy
  s_DepthMatrixProgram[0] = D3D::CompileAndCreatePixelShader(depth_matrix_program);
  CHECK(s_DepthMatrixProgram[0] != nullptr, "Create depth matrix pixel shader");
  D3D::SetDebugObjectName((ID3D11DeviceChild*)s_DepthMatrixProgram[0], "depth matrix pixel shader");

  Clear();

  SETSTAT(stats.numPixelShadersCreated, 0);
  SETSTAT(stats.numPixelShadersAlive, 0);

  if (g_ActiveConfig.bShaderCache)
    LoadShaderCache();

  if (g_ActiveConfig.CanPrecompileUberShaders())
    QueueUberShaderCompiles();
}

void PixelShaderCache::LoadShaderCache()
{
  PixelShaderCacheInserter<PixelShaderUid> inserter;
  g_ps_disk_cache.OpenAndRead(GetDiskShaderCacheFileName(APIType::D3D, "PS", true, true), inserter);

  PixelShaderCacheInserter<UberShader::PixelShaderUid> uber_inserter;
  g_uber_ps_disk_cache.OpenAndRead(GetDiskShaderCacheFileName(APIType::D3D, "UberPS", false, true),
                                   uber_inserter);
}

void PixelShaderCache::Reload()
{
  g_ps_disk_cache.Sync();
  g_ps_disk_cache.Close();
  g_uber_ps_disk_cache.Sync();
  g_uber_ps_disk_cache.Close();
  Clear();

  if (g_ActiveConfig.bShaderCache)
    LoadShaderCache();

  if (g_ActiveConfig.CanPrecompileUberShaders())
    QueueUberShaderCompiles();
}

// ONLY to be used during shutdown.
void PixelShaderCache::Clear()
{
  for (auto& iter : PixelShaders)
    iter.second.Destroy();
  for (auto& iter : UberPixelShaders)
    iter.second.Destroy();
  PixelShaders.clear();
  UberPixelShaders.clear();

  last_entry = nullptr;
  last_uber_entry = nullptr;
  last_uid = {};
  last_uber_uid = {};
}

// Used in Swap() when AA mode has changed
void PixelShaderCache::InvalidateMSAAShaders()
{
  SAFE_RELEASE(s_ColorCopyProgram[1]);
  SAFE_RELEASE(s_ColorMatrixProgram[1]);
  SAFE_RELEASE(s_DepthMatrixProgram[1]);
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
    SAFE_RELEASE(s_ColorMatrixProgram[i]);
    SAFE_RELEASE(s_DepthMatrixProgram[i]);
    SAFE_RELEASE(s_rgba6_to_rgb8[i]);
    SAFE_RELEASE(s_rgb8_to_rgba6[i]);
  }

  Clear();
  g_ps_disk_cache.Sync();
  g_ps_disk_cache.Close();
  g_uber_ps_disk_cache.Sync();
  g_uber_ps_disk_cache.Close();
}

bool PixelShaderCache::SetShader()
{
  if (g_ActiveConfig.bDisableSpecializedShaders)
    return SetUberShader();

  PixelShaderUid uid = GetPixelShaderUid();
  if (last_entry && uid == last_uid)
  {
    if (last_entry->pending)
      return SetUberShader();

    if (!last_entry->shader)
      return false;

    D3D::stateman->SetPixelShader(last_entry->shader);
    return true;
  }

  // Check if the shader is already in the cache
  auto iter = PixelShaders.find(uid);
  if (iter != PixelShaders.end())
  {
    const PSCacheEntry& entry = iter->second;
    if (entry.pending)
      return SetUberShader();

    last_uid = uid;
    last_entry = &entry;

    GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
    if (!last_entry->shader)
      return false;

    D3D::stateman->SetPixelShader(last_entry->shader);
    return true;
  }

  // Background compiling?
  if (g_ActiveConfig.CanBackgroundCompileShaders())
  {
    // Create a pending entry
    PSCacheEntry entry;
    entry.pending = true;
    PixelShaders[uid] = entry;

    // Queue normal shader compiling and use ubershader
    g_async_compiler->QueueWorkItem(
        g_async_compiler->CreateWorkItem<PixelShaderCompilerWorkItem>(uid));
    return SetUberShader();
  }

  // Need to compile a new shader
  D3DBlob* bytecode = nullptr;
  ShaderCode code =
      GeneratePixelShaderCode(APIType::D3D, ShaderHostConfig::GetCurrent(), uid.GetUidData());
  D3D::CompilePixelShader(code.GetBuffer(), &bytecode);
  if (!InsertByteCode(uid, bytecode->Data(), bytecode->Size()))
  {
    SAFE_RELEASE(bytecode);
    return false;
  }

  g_ps_disk_cache.Append(uid, bytecode->Data(), bytecode->Size());
  return SetShader();
}

bool PixelShaderCache::SetUberShader()
{
  UberShader::PixelShaderUid uid = UberShader::GetPixelShaderUid();

  if (last_uber_entry && last_uber_uid == uid)
  {
    if (!last_uber_entry->shader)
      return false;

    D3D::stateman->SetPixelShader(last_uber_entry->shader);
    return true;
  }

  auto iter = UberPixelShaders.find(uid);
  if (iter != UberPixelShaders.end())
  {
    const PSCacheEntry& entry = iter->second;
    last_uber_uid = uid;
    last_uber_entry = &entry;

    GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
    if (!last_uber_entry->shader)
      return false;

    D3D::stateman->SetPixelShader(last_uber_entry->shader);
    return true;
  }

  D3DBlob* bytecode = nullptr;
  ShaderCode code =
      UberShader::GenPixelShader(APIType::D3D, ShaderHostConfig::GetCurrent(), uid.GetUidData());
  D3D::CompilePixelShader(code.GetBuffer(), &bytecode);
  if (!InsertByteCode(uid, bytecode->Data(), bytecode->Size()))
  {
    SAFE_RELEASE(bytecode);
    return false;
  }

  // Lookup map again.
  g_uber_ps_disk_cache.Append(uid, bytecode->Data(), bytecode->Size());
  bytecode->Release();
  return SetUberShader();
}

bool PixelShaderCache::InsertByteCode(const PixelShaderUid& uid, const u8* data, size_t len)
{
  ID3D11PixelShader* shader = data ? D3D::CreatePixelShaderFromByteCode(data, len) : nullptr;
  if (!InsertShader(uid, shader))
  {
    SAFE_RELEASE(shader);
    return false;
  }

  return true;
}

bool PixelShaderCache::InsertByteCode(const UberShader::PixelShaderUid& uid, const u8* data,
                                      size_t len)
{
  ID3D11PixelShader* shader = data ? D3D::CreatePixelShaderFromByteCode(data, len) : nullptr;
  if (!InsertShader(uid, shader))
  {
    SAFE_RELEASE(shader);
    return false;
  }

  return true;
}

bool PixelShaderCache::InsertShader(const PixelShaderUid& uid, ID3D11PixelShader* shader)
{
  auto iter = PixelShaders.find(uid);
  if (iter != PixelShaders.end() && !iter->second.pending)
    return false;

  PSCacheEntry& newentry = PixelShaders[uid];
  newentry.pending = false;
  newentry.shader = shader;

  INCSTAT(stats.numPixelShadersCreated);
  SETSTAT(stats.numPixelShadersAlive, PixelShaders.size());
  return (shader != nullptr);
}

bool PixelShaderCache::InsertShader(const UberShader::PixelShaderUid& uid,
                                    ID3D11PixelShader* shader)
{
  auto iter = UberPixelShaders.find(uid);
  if (iter != UberPixelShaders.end() && !iter->second.pending)
    return false;

  PSCacheEntry& newentry = UberPixelShaders[uid];
  newentry.pending = false;
  newentry.shader = shader;
  return (shader != nullptr);
}

void PixelShaderCache::QueueUberShaderCompiles()
{
  UberShader::EnumeratePixelShaderUids([&](const UberShader::PixelShaderUid& uid) {
    if (UberPixelShaders.find(uid) != UberPixelShaders.end())
      return;

    g_async_compiler->QueueWorkItem(
        g_async_compiler->CreateWorkItem<UberPixelShaderCompilerWorkItem>(uid));
  });

  g_async_compiler->WaitUntilCompletion([](size_t completed, size_t total) {
    Host_UpdateProgressDialog(GetStringT("Compiling shaders...").c_str(),
                              static_cast<int>(completed), static_cast<int>(total));
  });
  g_async_compiler->RetrieveWorkItems();
  Host_UpdateProgressDialog("", -1, -1);
}

PixelShaderCache::PixelShaderCompilerWorkItem::PixelShaderCompilerWorkItem(
    const PixelShaderUid& uid)
{
  std::memcpy(&m_uid, &uid, sizeof(uid));
}

PixelShaderCache::PixelShaderCompilerWorkItem::~PixelShaderCompilerWorkItem()
{
  SAFE_RELEASE(m_bytecode);
}

bool PixelShaderCache::PixelShaderCompilerWorkItem::Compile()
{
  ShaderCode code =
      GeneratePixelShaderCode(APIType::D3D, ShaderHostConfig::GetCurrent(), m_uid.GetUidData());

  if (D3D::CompilePixelShader(code.GetBuffer(), &m_bytecode))
    m_shader = D3D::CreatePixelShaderFromByteCode(m_bytecode);

  return true;
}

void PixelShaderCache::PixelShaderCompilerWorkItem::Retrieve()
{
  if (InsertShader(m_uid, m_shader))
    g_ps_disk_cache.Append(m_uid, m_bytecode->Data(), m_bytecode->Size());
  else
    SAFE_RELEASE(m_shader);
}

PixelShaderCache::UberPixelShaderCompilerWorkItem::UberPixelShaderCompilerWorkItem(
    const UberShader::PixelShaderUid& uid)
{
  std::memcpy(&m_uid, &uid, sizeof(uid));
}

PixelShaderCache::UberPixelShaderCompilerWorkItem::~UberPixelShaderCompilerWorkItem()
{
  SAFE_RELEASE(m_bytecode);
}

bool PixelShaderCache::UberPixelShaderCompilerWorkItem::Compile()
{
  ShaderCode code =
      UberShader::GenPixelShader(APIType::D3D, ShaderHostConfig::GetCurrent(), m_uid.GetUidData());

  if (D3D::CompilePixelShader(code.GetBuffer(), &m_bytecode))
    m_shader = D3D::CreatePixelShaderFromByteCode(m_bytecode);

  return true;
}

void PixelShaderCache::UberPixelShaderCompilerWorkItem::Retrieve()
{
  if (InsertShader(m_uid, m_shader))
    g_uber_ps_disk_cache.Append(m_uid, m_bytecode->Data(), m_bytecode->Size());
  else
    SAFE_RELEASE(m_shader);
}

}  // DX11
