// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/Align.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/FramebufferManager.h"
#include "VideoBackends/D3D/GeometryShaderCache.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/GeometryShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VideoConfig.h"

namespace DX11
{
ID3D11GeometryShader* ClearGeometryShader = nullptr;
ID3D11GeometryShader* CopyGeometryShader = nullptr;

ID3D11GeometryShader* GeometryShaderCache::GetClearGeometryShader()
{
  return (g_ActiveConfig.stereo_mode != StereoMode::Off) ? ClearGeometryShader : nullptr;
}
ID3D11GeometryShader* GeometryShaderCache::GetCopyGeometryShader()
{
  return (g_ActiveConfig.stereo_mode != StereoMode::Off) ? CopyGeometryShader : nullptr;
}

ID3D11Buffer* gscbuf = nullptr;

ID3D11Buffer*& GeometryShaderCache::GetConstantBuffer()
{
  // TODO: divide the global variables of the generated shaders into about 5 constant buffers to
  // speed this up
  if (GeometryShaderManager::dirty)
  {
    D3D11_MAPPED_SUBRESOURCE map;
    D3D::context->Map(gscbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy(map.pData, &GeometryShaderManager::constants, sizeof(GeometryShaderConstants));
    D3D::context->Unmap(gscbuf, 0);
    GeometryShaderManager::dirty = false;

    ADDSTAT(stats.thisFrame.bytesUniformStreamed, sizeof(GeometryShaderConstants));
  }
  return gscbuf;
}

const char clear_shader_code[] = {
    "struct VSOUTPUT\n"
    "{\n"
    "	float4 vPosition   : POSITION;\n"
    "	float4 vColor0   : COLOR0;\n"
    "};\n"
    "struct GSOUTPUT\n"
    "{\n"
    "	float4 vPosition   : POSITION;\n"
    "	float4 vColor0   : COLOR0;\n"
    "	uint slice    : SV_RenderTargetArrayIndex;\n"
    "};\n"
    "[maxvertexcount(6)]\n"
    "void main(triangle VSOUTPUT o[3], inout TriangleStream<GSOUTPUT> Output)\n"
    "{\n"
    "for(int slice = 0; slice < 2; slice++)\n"
    "{\n"
    "	for(int i = 0; i < 3; i++)\n"
    "	{\n"
    "		GSOUTPUT OUT;\n"
    "		OUT.vPosition = o[i].vPosition;\n"
    "		OUT.vColor0 = o[i].vColor0;\n"
    "		OUT.slice = slice;\n"
    "		Output.Append(OUT);\n"
    "	}\n"
    "	Output.RestartStrip();\n"
    "}\n"
    "}\n"};

const char copy_shader_code[] = {
    "struct VSOUTPUT\n"
    "{\n"
    "	float4 vPosition : POSITION;\n"
    "	float3 vTexCoord : TEXCOORD0;\n"
    "};\n"
    "struct GSOUTPUT\n"
    "{\n"
    "	float4 vPosition : POSITION;\n"
    "	float3 vTexCoord : TEXCOORD0;\n"
    "	uint slice    : SV_RenderTargetArrayIndex;\n"
    "};\n"
    "[maxvertexcount(6)]\n"
    "void main(triangle VSOUTPUT o[3], inout TriangleStream<GSOUTPUT> Output)\n"
    "{\n"
    "for(int slice = 0; slice < 2; slice++)\n"
    "{\n"
    "	for(int i = 0; i < 3; i++)\n"
    "	{\n"
    "		GSOUTPUT OUT;\n"
    "		OUT.vPosition = o[i].vPosition;\n"
    "		OUT.vTexCoord = o[i].vTexCoord;\n"
    "		OUT.vTexCoord.z = float(slice);\n"
    "		OUT.slice = slice;\n"
    "		Output.Append(OUT);\n"
    "	}\n"
    "	Output.RestartStrip();\n"
    "}\n"
    "}\n"};

void GeometryShaderCache::Init()
{
  unsigned int gbsize = Common::AlignUp(static_cast<unsigned int>(sizeof(GeometryShaderConstants)),
                                        16);  // must be a multiple of 16
  D3D11_BUFFER_DESC gbdesc = CD3D11_BUFFER_DESC(gbsize, D3D11_BIND_CONSTANT_BUFFER,
                                                D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
  HRESULT hr = D3D::device->CreateBuffer(&gbdesc, nullptr, &gscbuf);
  CHECK(hr == S_OK, "Create geometry shader constant buffer (size=%u)", gbsize);
  D3D::SetDebugObjectName(gscbuf,
                          "geometry shader constant buffer used to emulate the GX pipeline");

  // used when drawing clear quads
  ClearGeometryShader = D3D::CompileAndCreateGeometryShader(clear_shader_code);
  CHECK(ClearGeometryShader != nullptr, "Create clear geometry shader");
  D3D::SetDebugObjectName(ClearGeometryShader, "clear geometry shader");

  // used for buffer copy
  CopyGeometryShader = D3D::CompileAndCreateGeometryShader(copy_shader_code);
  CHECK(CopyGeometryShader != nullptr, "Create copy geometry shader");
  D3D::SetDebugObjectName(CopyGeometryShader, "copy geometry shader");
}

void GeometryShaderCache::Shutdown()
{
  SAFE_RELEASE(gscbuf);

  SAFE_RELEASE(ClearGeometryShader);
  SAFE_RELEASE(CopyGeometryShader);
}
}  // DX11
