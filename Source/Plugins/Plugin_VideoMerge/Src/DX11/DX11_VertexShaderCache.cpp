// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <map>

// Common
#include "Common.h"
#include "FileUtil.h"
#include "LinearDiskCache.h"

// VideoCommon
#include "VideoConfig.h"
#include "Statistics.h"
#include "Profiler.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "XFMemory.h"

// DX11
#include "DX11_D3DBase.h"
#include "DX11_D3DShader.h"
#include "DX11_VertexShaderCache.h"
#include "DX11_Render.h"

#include "../Main.h"

namespace DX11
{

VertexShaderCache::VSCache VertexShaderCache::vshaders;
const VertexShaderCache::VSCacheEntry* VertexShaderCache::last_entry;

static ID3D11VertexShader* SimpleVertexShader = NULL;
static ID3D11VertexShader* ClearVertexShader = NULL;
static ID3D11InputLayout* SimpleLayout = NULL;
static ID3D11InputLayout* ClearLayout = NULL;

LinearDiskCache g_vs_disk_cache;

ID3D11VertexShader* VertexShaderCache::GetSimpleVertexShader() { return SimpleVertexShader; }
ID3D11VertexShader* VertexShaderCache::GetClearVertexShader() { return ClearVertexShader; }
ID3D11InputLayout* VertexShaderCache::GetSimpleInputLayout() { return SimpleLayout; }
ID3D11InputLayout* VertexShaderCache::GetClearInputLayout() { return ClearLayout; }

// maps the constant numbers to float indices in the constant buffer
unsigned int vs_constant_offset_table[C_VENVCONST_END];
void VertexShaderCache::SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	D3D::gfxstate->vsconstants[vs_constant_offset_table[const_number]  ] = f1;
	D3D::gfxstate->vsconstants[vs_constant_offset_table[const_number]+1] = f2;
	D3D::gfxstate->vsconstants[vs_constant_offset_table[const_number]+2] = f3;
	D3D::gfxstate->vsconstants[vs_constant_offset_table[const_number]+3] = f4;
	D3D::gfxstate->vscbufchanged = true;
}

void VertexShaderCache::SetVSConstant4fv(unsigned int const_number, const float* f)
{
	memcpy(&D3D::gfxstate->vsconstants[vs_constant_offset_table[const_number]], f, sizeof(float)*4);
	D3D::gfxstate->vscbufchanged = true;
}

void VertexShaderCache::SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float* f)
{
	for (unsigned int i = 0; i < count; i++)
	{
		memcpy(&D3D::gfxstate->vsconstants[vs_constant_offset_table[const_number+i]], f+3*i, sizeof(float)*3);
		D3D::gfxstate->vsconstants[vs_constant_offset_table[const_number+i]+3] = 0.f;		
	}
	D3D::gfxstate->vscbufchanged = true;
}

void VertexShaderCache::SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float* f)
{
	memcpy(&D3D::gfxstate->vsconstants[vs_constant_offset_table[const_number]], f, sizeof(float)*4*count);
	D3D::gfxstate->vscbufchanged = true;
}

// this class will load the precompiled shaders into our cache
class VertexShaderCacheInserter : public LinearDiskCacheReader {
public:
	void Read(const u8* key, int key_size, const u8* value, int value_size)
	{
		VERTEXSHADERUID uid;
		if (key_size != sizeof(uid))
		{
			ERROR_LOG(VIDEO, "Wrong key size in vertex shader cache");
			return;
		}
		memcpy(&uid, key, key_size);

		D3DBlob* blob = new D3DBlob(value_size, value);
		VertexShaderCache::InsertByteCode(uid, blob);
		blob->Release();
	}
};

const char simple_shader_code[] = {
	"struct VSOUTPUT\n"
	"{\n"
	"float4 vPosition : POSITION;\n"
	"float2 vTexCoord : TEXCOORD0;\n"
	"};\n"
	"VSOUTPUT main(float4 inPosition : POSITION,float2 inTEX0 : TEXCOORD0)\n"
	"{\n"
	"VSOUTPUT OUT;\n"
	"OUT.vPosition = inPosition;\n"
	"OUT.vTexCoord = inTEX0;\n"
	"return OUT;\n"
	"}\n"
};

const char clear_shader_code[] = {
	"struct VSOUTPUT\n"
	"{\n"
	"float4 vPosition   : POSITION;\n"
	"float4 vColor0   : COLOR0;\n"						   
	"};\n"
	"VSOUTPUT main(float4 inPosition : POSITION,float4 inColor0: COLOR0)\n"
	"{\n"
	"VSOUTPUT OUT;\n"
	"OUT.vPosition = inPosition;\n"
	"OUT.vColor0 = inColor0;\n"
	"return OUT;\n"
	"}\n"
};

VertexShaderCache::VertexShaderCache()
{
	const D3D11_INPUT_ELEMENT_DESC simpleelems[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	const D3D11_INPUT_ELEMENT_DESC clearelems[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3DBlob* blob;
	D3D::CompileVertexShader(simple_shader_code, sizeof(simple_shader_code), &blob);
	D3D::device->CreateInputLayout(simpleelems, 2, blob->Data(), blob->Size(), &SimpleLayout);
	SimpleVertexShader = D3D::CreateVertexShaderFromByteCode(blob);
	if (SimpleLayout == NULL || SimpleVertexShader == NULL) PanicAlert("Failed to create simple vertex shader or input layout at %s %d\n", __FILE__, __LINE__);
	blob->Release();
	D3D::SetDebugObjectName((ID3D11DeviceChild*)SimpleVertexShader, "simple vertex shader");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)SimpleLayout, "simple input layout");

	D3D::CompileVertexShader(clear_shader_code, sizeof(clear_shader_code), &blob);
	D3D::device->CreateInputLayout(clearelems, 2, blob->Data(), blob->Size(), &ClearLayout);
	ClearVertexShader = D3D::CreateVertexShaderFromByteCode(blob);
	if (ClearLayout == NULL || ClearVertexShader == NULL) PanicAlert("Failed to create clear vertex shader or input layout at %s %d\n", __FILE__, __LINE__);
	blob->Release();
	D3D::SetDebugObjectName((ID3D11DeviceChild*)ClearVertexShader, "clear vertex shader");
	D3D::SetDebugObjectName((ID3D11DeviceChild*)ClearLayout, "clear input layout");

	Clear();

	// these values are hardcoded, they depend on internal D3DCompile behavior
	// TODO: Do this with D3DReflect or something instead
	unsigned int k;
	for (k = 0;k <  6;k++) vs_constant_offset_table[C_POSNORMALMATRIX+k]       =   0+4*k;
	for (k = 0;k <  4;k++) vs_constant_offset_table[C_PROJECTION+k]            =  24+4*k;
	for (k = 0;k <  4;k++) vs_constant_offset_table[C_MATERIALS+k]             =  40+4*k;
	for (k = 0;k < 40;k++) vs_constant_offset_table[C_LIGHTS+k]                =  56+4*k;
	for (k = 0;k < 24;k++) vs_constant_offset_table[C_TEXMATRICES+k]           = 216+4*k;
	for (k = 0;k < 64;k++) vs_constant_offset_table[C_TRANSFORMMATRICES+k]     = 312+4*k;	
	for (k = 0;k < 32;k++) vs_constant_offset_table[C_NORMALMATRICES+k]        = 568+4*k;	
	for (k = 0;k < 64;k++) vs_constant_offset_table[C_POSTTRANSFORMMATRICES+k] = 696+4*k;
	for (k = 0;k <  4;k++) vs_constant_offset_table[C_DEPTHPARAMS+k]		   = 952+4*k;	

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

	char cache_filename[MAX_PATH];
	sprintf(cache_filename, "%sdx11-%s-vs.cache", File::GetUserPath(D_SHADERCACHE_IDX), g_globals->unique_id);
	VertexShaderCacheInserter inserter;
	g_vs_disk_cache.OpenAndRead(cache_filename, &inserter);
}

void VertexShaderCache::Clear()
{
	for (VSCache::iterator iter = vshaders.begin(); iter != vshaders.end(); ++iter)
		iter->second.Destroy();
	vshaders.clear();
}

VertexShaderCache::~VertexShaderCache()
{
	SAFE_RELEASE(SimpleVertexShader);
	SAFE_RELEASE(ClearVertexShader);

	SAFE_RELEASE(SimpleLayout);
	SAFE_RELEASE(ClearLayout);

	Clear();
	g_vs_disk_cache.Sync();
	g_vs_disk_cache.Close();
}

bool VertexShaderCache::SetShader(u32 components)
{
	DVSTARTPROFILE();

	VERTEXSHADERUID uid;
	GetVertexShaderId(&uid, components);
	if (uid == last_vertex_shader_uid && vshaders[uid].frameCount == frameCount)
		return (vshaders[uid].shader != NULL);

	memcpy(&last_vertex_shader_uid, &uid, sizeof(VERTEXSHADERUID));

	VSCache::iterator iter;
	iter = vshaders.find(uid);
	if (iter != vshaders.end())
	{
		iter->second.frameCount = frameCount;
		const VSCacheEntry &entry = iter->second;
		last_entry = &entry;

		if (entry.shader) D3D::gfxstate->SetVShader(entry.shader, iter->second.bytecode);
		return (entry.shader != NULL);
	}

	const char* code = GenerateVertexShaderCode(components, API_D3D11);

	D3DBlob* pbytecode = NULL;
	D3D::CompileVertexShader(code, (int)strlen(code), &pbytecode);

	if (pbytecode == NULL)
	{
		PanicAlert("Failed to compile Vertex Shader %s %d:\n\n%s", __FILE__, __LINE__, code);
		return false;
	}
	g_vs_disk_cache.Append((u8*)&uid, sizeof(uid), (const u8*)pbytecode->Data(), pbytecode->Size());
	g_vs_disk_cache.Sync();

	bool result = InsertByteCode(uid, pbytecode);
	D3D::gfxstate->SetVShader(last_entry->shader, last_entry->bytecode);
	pbytecode->Release();
	return result;
}

bool VertexShaderCache::InsertByteCode(const VERTEXSHADERUID &uid, D3DBlob* bcodeblob)
{
	ID3D11VertexShader* shader = D3D::CreateVertexShaderFromByteCode(bcodeblob);
	if (shader == NULL)
	{
		PanicAlert("Failed to create vertex shader from %p size %d at %s %d\n", bcodeblob->Data(), bcodeblob->Size(), __FILE__, __LINE__);
		return false;
	}
	// TODO: Somehow make the debug name a bit more specific
	D3D::SetDebugObjectName((ID3D11DeviceChild*)shader, "a vertex shader of VertexShaderCache");

	// Make an entry in the table
	VSCacheEntry entry;
	entry.shader = shader;
	entry.frameCount = frameCount;
	entry.SetByteCode(bcodeblob);

	vshaders[uid] = entry;
	last_entry = &vshaders[uid];

	INCSTAT(stats.numVertexShadersCreated);
	SETSTAT(stats.numVertexShadersAlive, (int)vshaders.size());

	return true;
}

}
