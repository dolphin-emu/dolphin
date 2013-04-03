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

float vsconstants[C_VENVCONST_END*4];
bool vscbufchanged[NUM_VS_CONSTANT_BUFFERS];

namespace DX11 {

VertexShaderCache::VSCache VertexShaderCache::vshaders;
const VertexShaderCache::VSCacheEntry *VertexShaderCache::last_entry;
VERTEXSHADERUID VertexShaderCache::last_uid;

static ID3D11VertexShader* SimpleVertexShader = NULL;
static ID3D11VertexShader* ClearVertexShader = NULL;
static ID3D11InputLayout* SimpleLayout = NULL;
static ID3D11InputLayout* ClearLayout = NULL;

LinearDiskCache<VERTEXSHADERUID, u8> g_vs_disk_cache;

ID3D11VertexShader* VertexShaderCache::GetSimpleVertexShader() { return SimpleVertexShader; }
ID3D11VertexShader* VertexShaderCache::GetClearVertexShader() { return ClearVertexShader; }
ID3D11InputLayout* VertexShaderCache::GetSimpleInputLayout() { return SimpleLayout; }
ID3D11InputLayout* VertexShaderCache::GetClearInputLayout() { return ClearLayout; }

ID3D11Buffer* vscbuf[NUM_VS_CONSTANT_BUFFERS] = {NULL};

ID3D11Buffer** VertexShaderCache::GetConstantBuffers()
{
	for (unsigned int i = 0; i < NUM_VS_CONSTANT_BUFFERS; ++i)
	{
		if (vscbufchanged[i])
		{
			D3D11_MAPPED_SUBRESOURCE map;
			D3D::context->Map(vscbuf[i], 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
			memcpy(map.pData, &vsconstants[cb_offsets[i]*4], sizeof(float)*4 * (cb_offsets[i+1] - cb_offsets[i]));
			D3D::context->Unmap(vscbuf[i], 0);
			vscbufchanged[i] = false;
		}
	}
	return vscbuf;
}

// this class will load the precompiled shaders into our cache
class VertexShaderCacheInserter : public LinearDiskCacheReader<VERTEXSHADERUID, u8>
{
public:
	void Read(const VERTEXSHADERUID &key, const u8 *value, u32 value_size)
	{
		D3DBlob* blob = new D3DBlob(value_size, value);
		VertexShaderCache::InsertByteCode(key, blob);
		blob->Release();

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

	for (unsigned int i = 0; i < NUM_VS_CONSTANT_BUFFERS; ++i)
	{
		unsigned int cbsize = ((sizeof(float)*4* (cb_offsets[i+1]-cb_offsets[i]))&(~0xf))+0x10; // must be a multiple of 16
		D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(cbsize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		HRESULT hr = D3D::device->CreateBuffer(&cbdesc, NULL, &vscbuf[i]);
		CHECK(hr==S_OK, "Create vertex shader constant buffer (size=%u)", cbsize);
		D3D::SetDebugObjectName((ID3D11DeviceChild*)vscbuf[i], "vertex shader constant buffer used to emulate the GX pipeline");

		vscbufchanged[i] = true;
	}

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

	if (!File::Exists(File::GetUserPath(D_SHADERCACHE_IDX)))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX).c_str());

	SETSTAT(stats.numVertexShadersCreated, 0);
	SETSTAT(stats.numVertexShadersAlive, 0);

	char cache_filename[MAX_PATH];
	sprintf(cache_filename, "%sdx11-%s-vs.cache", File::GetUserPath(D_SHADERCACHE_IDX).c_str(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str());
	VertexShaderCacheInserter inserter;
	g_vs_disk_cache.OpenAndRead(cache_filename, inserter);

	if (g_Config.bEnableShaderDebugging)
		Clear();

	last_entry = NULL;
}

void VertexShaderCache::Clear()
{
	for (VSCache::iterator iter = vshaders.begin(); iter != vshaders.end(); ++iter)
		iter->second.Destroy();
	vshaders.clear();

	last_entry = NULL;
}

void VertexShaderCache::Shutdown()
{
	for (unsigned int i = 0; i < NUM_VS_CONSTANT_BUFFERS; ++i)
		SAFE_RELEASE(vscbuf[i]);

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
	VERTEXSHADERUID uid;
	GetVertexShaderId(&uid, components);
	if (last_entry)
	{
		if (uid == last_uid)
		{
			GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
			ValidateVertexShaderIDs(API_D3D11, last_entry->safe_uid, last_entry->code, components);
			return (last_entry->shader != NULL);
		}
	}

	last_uid = uid;

	VSCache::iterator iter = vshaders.find(uid);
	if (iter != vshaders.end())
	{
		const VSCacheEntry &entry = iter->second;
		last_entry = &entry;

		GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
		ValidateVertexShaderIDs(API_D3D11, entry.safe_uid, entry.code, components);
		return (entry.shader != NULL);
	}

	const char *code = GenerateVertexShaderCode(components, API_D3D11);

	D3DBlob* pbytecode = NULL;
	D3D::CompileVertexShader(code, (int)strlen(code), &pbytecode);

	if (pbytecode == NULL)
	{
		GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
		return false;
	}
	g_vs_disk_cache.Append(uid, pbytecode->Data(), pbytecode->Size());

	bool success = InsertByteCode(uid, pbytecode);
	pbytecode->Release();

	if (g_ActiveConfig.bEnableShaderDebugging && success)
	{
		vshaders[uid].code = code;
		GetSafeVertexShaderId(&vshaders[uid].safe_uid, components);
	}

	GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
	return success;
}

bool VertexShaderCache::InsertByteCode(const VERTEXSHADERUID &uid, D3DBlob* bcodeblob)
{
	ID3D11VertexShader* shader = D3D::CreateVertexShaderFromByteCode(bcodeblob);
	if (shader == NULL)
		return false;

	// TODO: Somehow make the debug name a bit more specific
	D3D::SetDebugObjectName((ID3D11DeviceChild*)shader, "a vertex shader of VertexShaderCache");

	// Make an entry in the table
	VSCacheEntry entry;
	entry.shader = shader;
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
	for (unsigned int i = 0; i < NUM_VS_CONSTANT_BUFFERS; ++i)
		if (const_number >= cb_offsets[i] && const_number < cb_offsets[i+1])
			vscbufchanged[i] = true;

	vsconstants[4*const_number  ] = f1;
	vsconstants[4*const_number+1] = f2;
	vsconstants[4*const_number+2] = f3;
	vsconstants[4*const_number+3] = f4;
}

void Renderer::SetVSConstant4fv(unsigned int const_number, const float* f)
{
	for (unsigned int i = 0; i < NUM_VS_CONSTANT_BUFFERS; ++i)
		if (const_number >= cb_offsets[i] && const_number < cb_offsets[i+1])
			vscbufchanged[i] = true;

	memcpy(&vsconstants[4*const_number], f, sizeof(float)*4);
}

void Renderer::SetMultiVSConstant3fv(unsigned int const_number, unsigned int count, const float* f)
{
	for (unsigned int i = 0; i < NUM_VS_CONSTANT_BUFFERS; ++i)
		if (const_number+count >= cb_offsets[i] && const_number < cb_offsets[i+1])
			vscbufchanged[i] = true;

	for (unsigned int i = 0; i < count; i++)
	{
		memcpy(&vsconstants[4*(const_number+i)], f+3*i, sizeof(float)*3);
		vsconstants[4*(const_number+i)+3] = 0.f;		
	}
}

void Renderer::SetMultiVSConstant4fv(unsigned int const_number, unsigned int count, const float* f)
{
	for (unsigned int i = 0; i < NUM_VS_CONSTANT_BUFFERS; ++i)
		if (const_number+count >= cb_offsets[i] && const_number < cb_offsets[i+1])
			vscbufchanged[i] = true;

	memcpy(&vsconstants[4*const_number], f, sizeof(float)*4*count);
}

}  // namespace DX11
