// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/Align.h"
#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"
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
GeometryShaderCache::GSCache GeometryShaderCache::GeometryShaders;
const GeometryShaderCache::GSCacheEntry* GeometryShaderCache::last_entry;
GeometryShaderUid GeometryShaderCache::last_uid;
const GeometryShaderCache::GSCacheEntry GeometryShaderCache::pass_entry;

ComPtr<ID3D11GeometryShader> ClearGeometryShader;
ComPtr<ID3D11GeometryShader> CopyGeometryShader;

LinearDiskCache<GeometryShaderUid, u8> g_gs_disk_cache;

ID3D11GeometryShader* GeometryShaderCache::GetClearGeometryShader()
{
  return (g_ActiveConfig.iStereoMode > 0) ? ClearGeometryShader.Get() : nullptr;
}
ID3D11GeometryShader* GeometryShaderCache::GetCopyGeometryShader()
{
  return (g_ActiveConfig.iStereoMode > 0) ? CopyGeometryShader.Get() : nullptr;
}

ComPtr<ID3D11Buffer> gscbuf;

ID3D11Buffer* GeometryShaderCache::GetConstantBuffer()
{
  // TODO: divide the global variables of the generated shaders into about 5 constant buffers to
  // speed this up
  if (GeometryShaderManager::dirty)
  {
    D3D11_MAPPED_SUBRESOURCE map;
    D3D::context->Map(gscbuf.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy(map.pData, &GeometryShaderManager::constants, sizeof(GeometryShaderConstants));
    D3D::context->Unmap(gscbuf.Get(), 0);
    GeometryShaderManager::dirty = false;

    ADDSTAT(stats.thisFrame.bytesUniformStreamed, sizeof(GeometryShaderConstants));
  }
  return gscbuf.Get();
}

// this class will load the precompiled shaders into our cache
class GeometryShaderCacheInserter : public LinearDiskCacheReader<GeometryShaderUid, u8>
{
public:
  void Read(const GeometryShaderUid& key, const u8* value, u32 value_size)
  {
    GeometryShaderCache::InsertByteCode(key, value, value_size);
  }
};

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
    "	float  vTexCoord1 : TEXCOORD1;\n"
    "};\n"
    "struct GSOUTPUT\n"
    "{\n"
    "	float4 vPosition : POSITION;\n"
    "	float3 vTexCoord : TEXCOORD0;\n"
    "	float  vTexCoord1 : TEXCOORD1;\n"
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
    "		OUT.vTexCoord.z = slice;\n"
    "		OUT.vTexCoord1 = o[i].vTexCoord1;\n"
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
  D3D::SetDebugObjectName(gscbuf.Get(),
                          "geometry shader constant buffer used to emulate the GX pipeline");

  // used when drawing clear quads
  ClearGeometryShader = D3D::CompileAndCreateGeometryShader(clear_shader_code);
  CHECK(ClearGeometryShader != nullptr, "Create clear geometry shader");
  D3D::SetDebugObjectName(ClearGeometryShader.Get(), "clear geometry shader");

  // used for buffer copy
  CopyGeometryShader = D3D::CompileAndCreateGeometryShader(copy_shader_code);
  CHECK(CopyGeometryShader != nullptr, "Create copy geometry shader");
  D3D::SetDebugObjectName(CopyGeometryShader.Get(), "copy geometry shader");

  Clear();

  if (g_ActiveConfig.bShaderCache)
    LoadShaderCache();

  if (g_ActiveConfig.CanPrecompileUberShaders())
    PrecompileShaders();
}

void GeometryShaderCache::LoadShaderCache()
{
  GeometryShaderCacheInserter inserter;
  g_gs_disk_cache.OpenAndRead(GetDiskShaderCacheFileName(APIType::D3D, "GS", true, true), inserter);
}

void GeometryShaderCache::Reload()
{
  g_gs_disk_cache.Sync();
  g_gs_disk_cache.Close();
  Clear();

  if (g_ActiveConfig.bShaderCache)
    LoadShaderCache();

  if (g_ActiveConfig.CanPrecompileUberShaders())
    PrecompileShaders();
}

// ONLY to be used during shutdown.
void GeometryShaderCache::Clear()
{
  GeometryShaders.clear();

  last_entry = nullptr;
  last_uid = {};
}

void GeometryShaderCache::Shutdown()
{
  gscbuf.Reset();

  ClearGeometryShader.Reset();
  CopyGeometryShader.Reset();

  Clear();
  g_gs_disk_cache.Sync();
  g_gs_disk_cache.Close();
}

bool GeometryShaderCache::SetShader(u32 primitive_type)
{
  GeometryShaderUid uid = GetGeometryShaderUid(primitive_type);
  if (last_entry && uid == last_uid)
  {
    GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
    D3D::stateman->SetGeometryShader(last_entry->shader.Get());
    return true;
  }

  // Check if the shader is a pass-through shader
  if (uid.GetUidData()->IsPassthrough())
  {
    // Return the default pass-through shader
    last_uid = uid;
    last_entry = &pass_entry;
    D3D::stateman->SetGeometryShader(last_entry->shader.Get());
    return true;
  }

  // Check if the shader is already in the cache
  auto iter = GeometryShaders.find(uid);
  if (iter != GeometryShaders.end())
  {
    const GSCacheEntry& entry = iter->second;
    last_uid = uid;
    last_entry = &entry;
    D3D::stateman->SetGeometryShader(last_entry->shader.Get());
    return (entry.shader != nullptr);
  }

  // Need to compile a new shader
  if (CompileShader(uid))
    return SetShader(primitive_type);
  else
    return false;
}

bool GeometryShaderCache::CompileShader(const GeometryShaderUid& uid)
{
  ComPtr<D3DBlob> bytecode;
  ShaderCode code =
      GenerateGeometryShaderCode(APIType::D3D, ShaderHostConfig::GetCurrent(), uid.GetUidData());
  if (!D3D::CompileGeometryShader(code.GetBuffer(), &bytecode) ||
      !InsertByteCode(uid, bytecode->Data(), bytecode->Size()))
  {
    return false;
  }

  // Insert the bytecode into the caches
  g_gs_disk_cache.Append(uid, bytecode->Data(), bytecode->Size());
  return true;
}

bool GeometryShaderCache::InsertByteCode(const GeometryShaderUid& uid, const u8* bytecode,
                                         size_t len)
{
  GSCacheEntry& newentry = GeometryShaders[uid];
  newentry.shader = bytecode ? D3D::CreateGeometryShaderFromByteCode(bytecode, len) : nullptr;
  return newentry.shader != nullptr;
}

void GeometryShaderCache::PrecompileShaders()
{
  EnumerateGeometryShaderUids([](const GeometryShaderUid& uid) {
    if (GeometryShaders.find(uid) != GeometryShaders.end())
      return;

    CompileShader(uid);
  });
}

}  // DX11
