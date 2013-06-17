// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
const VertexShaderCache::VSCacheEntry *VertexShaderCache::last_entry;
VertexShaderUid VertexShaderCache::last_uid;
UidChecker<VertexShaderUid,VertexShaderCode> VertexShaderCache::vertex_uid_checker;

static ID3D11VertexShader* SimpleVertexShader = NULL;
static ID3D11VertexShader* ClearVertexShader = NULL;
static ID3D11InputLayout* SimpleLayout = NULL;
static ID3D11InputLayout* ClearLayout = NULL;

LinearDiskCache<VertexShaderUid, u8> g_vs_disk_cache;

ID3D11VertexShader* VertexShaderCache::GetSimpleVertexShader() { return SimpleVertexShader; }
ID3D11VertexShader* VertexShaderCache::GetClearVertexShader() { return ClearVertexShader; }
ID3D11InputLayout* VertexShaderCache::GetSimpleInputLayout() { return SimpleLayout; }
ID3D11InputLayout* VertexShaderCache::GetClearInputLayout() { return ClearLayout; }

ID3D11Buffer* vscbuf = NULL;

ID3D11Buffer* &VertexShaderCache::GetConstantBuffer()
{
	// TODO: divide the global variables of the generated shaders into about 5 constant buffers to speed this up
	if (vscbufchanged)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		D3D::context->Map(vscbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, vsconstants, sizeof(vsconstants));
		D3D::context->Unmap(vscbuf, 0);
		vscbufchanged = false;
		
		ADDSTAT(stats.thisFrame.bytesUniformStreamed, sizeof(vsconstants));
	}
	return vscbuf;
}

// this class will load the precompiled shaders into our cache
class VertexShaderCacheInserter : public LinearDiskCacheReader<VertexShaderUid, u8>
{
public:
	void Read(const VertexShaderUid &key, const u8 *value, u32 value_size)
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

	unsigned int cbsize = ((sizeof(vsconstants))&(~0xf))+0x10; // must be a multiple of 16
	D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(cbsize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
	HRESULT hr = D3D::device->CreateBuffer(&cbdesc, NULL, &vscbuf);
	CHECK(hr==S_OK, "Create vertex shader constant buffer (size=%u)", cbsize);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)vscbuf, "vertex shader constant buffer used to emulate the GX pipeline");

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
	vs_constant_offset_table[C_DEPTHPARAMS] = 952;

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
	vertex_uid_checker.Invalidate();

	last_entry = NULL;
}

void VertexShaderCache::Shutdown()
{
	SAFE_RELEASE(vscbuf);

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
	VertexShaderUid uid;
	GetVertexShaderUid(uid, components, API_D3D11);
	if (g_ActiveConfig.bEnableShaderDebugging)
	{
		VertexShaderCode code;
		GenerateVertexShaderCode(code, components, API_D3D11);
		vertex_uid_checker.AddToIndexAndCheck(code, uid, "Vertex", "v");
	}

	if (last_entry)
	{
		if (uid == last_uid)
		{
			GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
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
		return (entry.shader != NULL);
	}

	VertexShaderCode code;
	GenerateVertexShaderCode(code, components, API_D3D11);

	D3DBlob* pbytecode = NULL;
	D3D::CompileVertexShader(code.GetBuffer(), (int)strlen(code.GetBuffer()), &pbytecode);

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
		vshaders[uid].code = code.GetBuffer();
	}

	GFX_DEBUGGER_PAUSE_AT(NEXT_VERTEX_SHADER_CHANGE, true);
	return success;
}

bool VertexShaderCache::InsertByteCode(const VertexShaderUid &uid, D3DBlob* bcodeblob)
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
