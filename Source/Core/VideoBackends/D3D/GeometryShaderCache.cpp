// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/FileUtil.h"
#include "Common/LinearDiskCache.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DShader.h"
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

ID3D11GeometryShader* ClearGeometryShader = nullptr;
ID3D11GeometryShader* CopyGeometryShader = nullptr;

LinearDiskCache<GeometryShaderUid, u8> g_gs_disk_cache;

ID3D11GeometryShader* GeometryShaderCache::GetClearGeometryShader() { return (g_ActiveConfig.iStereoMode > 0) ? ClearGeometryShader: nullptr; }
ID3D11GeometryShader* GeometryShaderCache::GetCopyGeometryShader() { return (g_ActiveConfig.iStereoMode > 0) ? CopyGeometryShader : nullptr; }

ID3D11Buffer* gscbuf = nullptr;

ID3D11Buffer* &GeometryShaderCache::GetConstantBuffer()
{
	// TODO: divide the global variables of the generated shaders into about 5 constant buffers to speed this up
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

// this class will load the precompiled shaders into our cache
class GeometryShaderCacheInserter : public LinearDiskCacheReader<GeometryShaderUid, u8>
{
public:
	void Read(const GeometryShaderUid &key, const u8* value, u32 value_size)
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
	"}\n"
};

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
	"}\n"
};

void GeometryShaderCache::Init()
{
	unsigned int gbsize = ROUND_UP(sizeof(GeometryShaderConstants), 16); // must be a multiple of 16
	D3D11_BUFFER_DESC gbdesc = CD3D11_BUFFER_DESC(gbsize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	HRESULT hr = D3D::device->CreateBuffer(&gbdesc, nullptr, &gscbuf);
	CHECK(hr == S_OK, "Create geometry shader constant buffer (size=%u)", gbsize);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)gscbuf, "geometry shader constant buffer used to emulate the GX pipeline");

	// used when drawing clear quads
	ClearGeometryShader = D3D::CompileAndCreateGeometryShader(clear_shader_code);
	CHECK(ClearGeometryShader != nullptr, "Create clear geometry shader");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)ClearGeometryShader, "clear geometry shader");

	// used for buffer copy
	CopyGeometryShader = D3D::CompileAndCreateGeometryShader(copy_shader_code);
	CHECK(CopyGeometryShader != nullptr, "Create copy geometry shader");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)CopyGeometryShader, "copy geometry shader");

	Clear();

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

	std::string cache_filename = StringFromFormat("%sdx11-%s-gs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
			SConfig::GetInstance().m_strUniqueID.c_str());
	GeometryShaderCacheInserter inserter;
	g_gs_disk_cache.OpenAndRead(cache_filename, inserter);

	if (g_Config.bEnableShaderDebugging)
		Clear();

	last_entry = nullptr;
}

// ONLY to be used during shutdown.
void GeometryShaderCache::Clear()
{
	for (auto& iter : GeometryShaders)
		iter.second.Destroy();
	GeometryShaders.clear();

	last_entry = nullptr;
}

void GeometryShaderCache::Shutdown()
{
	SAFE_RELEASE(gscbuf);

	SAFE_RELEASE(ClearGeometryShader);
	SAFE_RELEASE(CopyGeometryShader);

	Clear();
	g_gs_disk_cache.Sync();
	g_gs_disk_cache.Close();
}

bool GeometryShaderCache::SetShader(u32 primitive_type)
{
	GeometryShaderUid uid = GetGeometryShaderUid(primitive_type);

	// Check if the shader is already set
	if (last_entry)
	{
		if (uid == last_uid)
		{
			GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE,true);
			return true;
		}
	}

	last_uid = uid;

	// Check if the shader is a pass-through shader
	if (uid.GetUidData()->IsPassthrough())
	{
		// Return the default pass-through shader
		last_entry = &pass_entry;
		return true;
	}

	// Check if the shader is already in the cache
	GSCache::iterator iter;
	iter = GeometryShaders.find(uid);
	if (iter != GeometryShaders.end())
	{
		const GSCacheEntry &entry = iter->second;
		last_entry = &entry;

		return (entry.shader != nullptr);
	}

	// Need to compile a new shader
	ShaderCode code = GenerateGeometryShaderCode(primitive_type, API_D3D, uid.GetUidData());

	D3DBlob* pbytecode;
	if (!D3D::CompileGeometryShader(code.GetBuffer(), &pbytecode))
	{
		GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
		return false;
	}

	// Insert the bytecode into the caches
	g_gs_disk_cache.Append(uid, pbytecode->Data(), pbytecode->Size());

	bool success = InsertByteCode(uid, pbytecode->Data(), pbytecode->Size());
	pbytecode->Release();

	if (g_ActiveConfig.bEnableShaderDebugging && success)
	{
		GeometryShaders[uid].code = code.GetBuffer();
	}

	return success;
}

bool GeometryShaderCache::InsertByteCode(const GeometryShaderUid &uid, const void* bytecode, unsigned int bytecodelen)
{
	ID3D11GeometryShader* shader = D3D::CreateGeometryShaderFromByteCode(bytecode, bytecodelen);
	if (shader == nullptr)
		return false;

	// TODO: Somehow make the debug name a bit more specific
	D3D::SetDebugObjectName((ID3D11DeviceChild*)shader, "a pixel shader of GeometryShaderCache");

	// Make an entry in the table
	GSCacheEntry newentry;
	newentry.shader = shader;
	GeometryShaders[uid] = newentry;
	last_entry = &GeometryShaders[uid];

	if (!shader)
		return false;

	return true;
}

}  // DX11
