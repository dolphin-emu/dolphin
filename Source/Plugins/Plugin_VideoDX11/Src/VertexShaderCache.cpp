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

#include "FileUtil.h"
#include "LinearDiskCache.h"

#include "Debugger.h"
#include "Statistics.h"
#include "VertexShaderGen.h"

#include "D3DShader.h"
#include "Globals.h"
#include "VertexShaderCache.h"

#include "ConfigManager.h"

// See comment near the bottom of this file
static unsigned int vs_constant_offset_table[C_VENVCONST_END];
float vsconstants[C_VENVCONST_END*4];
bool vscbufchanged = true;

namespace DX11 {

VertexShaderCache::VSCache VertexShaderCache::vshaders;
const VertexShaderCache::VSCacheEntry* VertexShaderCache::last_entry;

static SharedPtr<ID3D11VertexShader> SimpleVertexShader;
static SharedPtr<ID3D11VertexShader> ClearVertexShader;

static SharedPtr<ID3D11InputLayout> SimpleLayout;
static SharedPtr<ID3D11InputLayout> ClearLayout;

LinearDiskCache<VERTEXSHADERUID, u8> g_vs_disk_cache;

ID3D11VertexShader* VertexShaderCache::GetSimpleVertexShader() { return SimpleVertexShader; }
ID3D11VertexShader* VertexShaderCache::GetClearVertexShader() { return ClearVertexShader; }
ID3D11InputLayout* VertexShaderCache::GetSimpleInputLayout() { return SimpleLayout; }
ID3D11InputLayout* VertexShaderCache::GetClearInputLayout() { return ClearLayout; }

SharedPtr<ID3D11Buffer> vscbuf;

ID3D11Buffer*const& VertexShaderCache::GetConstantBuffer()
{
	// TODO: divide the global variables of the generated shaders into about 5 constant buffers to speed this up
	if (vscbufchanged)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		D3D::g_context->Map(vscbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, vsconstants, sizeof(vsconstants));
		D3D::g_context->Unmap(vscbuf, 0);
		vscbufchanged = false;
	}
	return vscbuf;
}

// this class will load the precompiled shaders into our cache
class VertexShaderCacheInserter
{
public:
	template <typename F>
	void operator()(const VERTEXSHADERUID& key, u32 value_size, F get_data) const
	{
		ID3D10Blob* blob = nullptr;
		PD3D10CreateBlob(value_size, &blob);
		get_data((u8*)blob->GetBufferPointer());	// read data from file into blob

		VertexShaderCache::InsertByteCode(key, SharedPtr<ID3D10Blob>::FromPtr(blob));
	}
};

const char simple_shader_code[] = {
	"struct VSOUTPUT\n"
	"{\n"
	"float4 vPosition : POSITION;\n"
	"float2 vTexCoord : TEXCOORD0;\n"
	"float  vTexCoord1 : TEXCOORD1;\n"
	"};\n"
	"VSOUTPUT main(float4 inPosition : POSITION,float3 inTEX0 : TEXCOORD0)\n"
	"{\n"
	"VSOUTPUT OUT;\n"
	"OUT.vPosition = inPosition;\n"
	"OUT.vTexCoord = inTEX0.xy;\n"
	"OUT.vTexCoord1 = inTEX0.z;\n"
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

void VertexShaderCache::Init()
{
	const D3D11_INPUT_ELEMENT_DESC simpleelems[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		
	};
	const D3D11_INPUT_ELEMENT_DESC clearelems[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	unsigned int cbsize = ((sizeof(vsconstants))&(~0xf))+0x10; // must be a multiple of 16
	D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(cbsize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	vscbuf = CreateBufferShared(&cbdesc, NULL);
	CHECK(vscbuf, "Create vertex shader constant buffer (size=%u)", cbsize);
	D3D::SetDebugObjectName(vscbuf, "vertex shader constant buffer used to emulate the GX pipeline");

	{
	auto const blob = D3D::CompileVertexShader(simple_shader_code, sizeof(simple_shader_code));
	SimpleLayout = CreateInputLayoutShared(simpleelems, 2, blob->GetBufferPointer(), blob->GetBufferSize());
	SimpleVertexShader = D3D::CreateVertexShaderFromByteCode(blob);
	if (SimpleLayout == NULL || !SimpleVertexShader)
		PanicAlert("Failed to create simple vertex shader or input layout at %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName(SimpleVertexShader, "simple vertex shader");
	D3D::SetDebugObjectName(SimpleLayout, "simple input layout");
	}

	{
	auto const blob = D3D::CompileVertexShader(clear_shader_code, sizeof(clear_shader_code));
	ClearLayout = CreateInputLayoutShared(clearelems, 2, blob->GetBufferPointer(), blob->GetBufferSize());
	ClearVertexShader = D3D::CreateVertexShaderFromByteCode(blob);
	if (ClearLayout == NULL || !ClearVertexShader)
		PanicAlert("Failed to create clear vertex shader or input layout at %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName(ClearVertexShader, "clear vertex shader");
	D3D::SetDebugObjectName(ClearLayout, "clear input layout");
	}

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
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX).c_str());

	SETSTAT(stats.numVertexShadersCreated, 0);
	SETSTAT(stats.numVertexShadersAlive, 0);

	char cache_filename[MAX_PATH];
	sprintf(cache_filename, "%sdx11-%s-vs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str());

	g_vs_disk_cache.OpenAndRead(cache_filename, VertexShaderCacheInserter());
}

void VertexShaderCache::Clear()
{
	vshaders.clear();
}

void VertexShaderCache::Shutdown()
{
	vscbuf.reset();

	SimpleVertexShader.reset();
	ClearVertexShader.reset();

	SimpleLayout.reset();
	ClearLayout.reset();

	Clear();
	g_vs_disk_cache.Sync();
	g_vs_disk_cache.Close();
}

bool VertexShaderCache::LoadShader(u32 components)
{
	VERTEXSHADERUID uid;
	GetVertexShaderId(&uid, components);
	if (uid == last_vertex_shader_uid && vshaders[uid].frameCount == frameCount)
	{
		GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
		return (!!vshaders[uid].shader);
	}

	memcpy(&last_vertex_shader_uid, &uid, sizeof(VERTEXSHADERUID));

	VSCache::iterator iter = vshaders.find(uid);
	if (iter != vshaders.end())
	{
		iter->second.frameCount = frameCount;
		const VSCacheEntry &entry = iter->second;
		last_entry = &entry;

		GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
		return (!!entry.shader);
	}

	const char *code = GenerateVertexShaderCode(components, API_D3D11);

	auto const pbytecode = D3D::CompileVertexShader(code, (int)strlen(code));
	if (!pbytecode)
	{
		PanicAlert("Failed to compile Vertex Shader %s %d:\n\n%s", __FILE__, __LINE__, code);
		GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
		return false;
	}
	g_vs_disk_cache.Append(uid, (u8*)pbytecode->GetBufferPointer(), (u32)pbytecode->GetBufferSize());
	g_vs_disk_cache.Sync();

	bool result = InsertByteCode(uid, pbytecode);
	GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
	return result;
}

bool VertexShaderCache::InsertByteCode(const VERTEXSHADERUID &uid, SharedPtr<ID3D10Blob> bcodeblob)
{
	auto const shader = D3D::CreateVertexShaderFromByteCode(bcodeblob);
	if (!shader)
	{
		PanicAlert("Failed to create vertex shader from %p size %d at %s %d\n",
			bcodeblob->GetBufferPointer(), bcodeblob->GetBufferSize(), __FILE__, __LINE__);
		return false;
	}
	// TODO: Somehow make the debug name a bit more specific
	D3D::SetDebugObjectName(shader, "a vertex shader of VertexShaderCache");

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

// These are "callbacks" from VideoCommon and thus must be outside namespace DX11.
// This will have to be changed when we merge.

// maps the constant numbers to float indices in the constant buffer
void Renderer::SetVSConstant4f(unsigned int const_number, float f1, float f2, float f3, float f4)
{
	vsconstants[vs_constant_offset_table[const_number]  ] = f1;
	vsconstants[vs_constant_offset_table[const_number]+1] = f2;
	vsconstants[vs_constant_offset_table[const_number]+2] = f3;
	vsconstants[vs_constant_offset_table[const_number]+3] = f4;
	vscbufchanged = true;
}

void Renderer::SetVSConstant4fv(unsigned int const_number, const float* f)
{
	memcpy(&vsconstants[vs_constant_offset_table[const_number]], f, sizeof(float)*4);
	vscbufchanged = true;
}

void Renderer::SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float* f)
{
	for (unsigned int i = 0; i < count; i++)
	{
		memcpy(&vsconstants[vs_constant_offset_table[const_number+i]], f+3*i, sizeof(float)*3);
		vsconstants[vs_constant_offset_table[const_number+i]+3] = 0.f;		
	}
	vscbufchanged = true;
}

void Renderer::SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float* f)
{
	memcpy(&vsconstants[vs_constant_offset_table[const_number]], f, sizeof(float)*4*count);
	vscbufchanged = true;
}

}  // namespace DX11
