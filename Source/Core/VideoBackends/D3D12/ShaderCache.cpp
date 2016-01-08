// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/LinearDiskCache.h"

#include "Core/ConfigManager.h"

#include "VideoBackends/D3D12/D3DCommandListManager.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoBackends/D3D12/ShaderCache.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/Statistics.h"

namespace DX12
{

// Primitive topology type is always triangle, unless the GS stage is used. This is consumed
// by the PSO created in Renderer::ApplyState.
static D3D12_PRIMITIVE_TOPOLOGY_TYPE s_current_primitive_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

using GsBytecodeCache = std::map<GeometryShaderUid, D3D12_SHADER_BYTECODE>;
using PsBytecodeCache = std::map<PixelShaderUid, D3D12_SHADER_BYTECODE>;
using VsBytecodeCache = std::map<VertexShaderUid, D3D12_SHADER_BYTECODE>;
GsBytecodeCache s_gs_bytecode_cache;
PsBytecodeCache s_ps_bytecode_cache;
VsBytecodeCache s_vs_bytecode_cache;

// Used to keep track of blobs to release at Shutdown time.
static std::vector<D3DBlob*> s_shader_blob_list;

// Only used for shader debugging..
using GsHlslCache = std::map<GeometryShaderUid, std::string>;
using PsHlslCache = std::map<PixelShaderUid, std::string>;
using VsHlslCache = std::map<VertexShaderUid, std::string>;
static GsHlslCache s_gs_hlsl_cache;
static PsHlslCache s_ps_hlsl_cache;
static VsHlslCache s_vs_hlsl_cache;

static LinearDiskCache<GeometryShaderUid, u8> s_gs_disk_cache;
static LinearDiskCache<PixelShaderUid, u8> s_ps_disk_cache;
static LinearDiskCache<VertexShaderUid, u8> s_vs_disk_cache;

static UidChecker<GeometryShaderUid, ShaderCode> s_geometry_uid_checker;
static UidChecker<PixelShaderUid, ShaderCode> s_pixel_uid_checker;
static UidChecker<VertexShaderUid, ShaderCode> s_vertex_uid_checker;

static D3D12_SHADER_BYTECODE s_last_geometry_shader_bytecode;
static D3D12_SHADER_BYTECODE s_last_pixel_shader_bytecode;
static D3D12_SHADER_BYTECODE s_last_vertex_shader_bytecode;
static GeometryShaderUid s_last_geometry_shader_uid;
static PixelShaderUid s_last_pixel_shader_uid;
static VertexShaderUid s_last_vertex_shader_uid;

template<class UidType, class ShaderCacheType, ShaderCacheType* cache>
class ShaderCacheInserter final : public LinearDiskCacheReader<UidType, u8>
{
public:
	void Read(const UidType &key, const u8* value, u32 value_size)
	{
		D3DBlob* blob = new D3DBlob(value_size, value);
		ShaderCache::InsertByteCode<UidType, ShaderCacheType>(key, cache, blob);
	}
};

void ShaderCache::Init()
{
	// This class intentionally shares its shader cache files with DX11, as the shaders are (right now) identical.
	// Reduces unnecessary compilation when switching between APIs.

	s_last_geometry_shader_bytecode = {};
	s_last_pixel_shader_bytecode = {};
	s_last_vertex_shader_bytecode = {};
	s_last_geometry_shader_uid = {};
	s_last_pixel_shader_uid = {};
	s_last_vertex_shader_uid = {};

	// Ensure shader cache directory exists..
	std::string shader_cache_path = File::GetUserPath(D_SHADERCACHE_IDX);

	if (!File::Exists(shader_cache_path))
		File::CreateDir(File::GetUserPath(D_SHADERCACHE_IDX));

	std::string title_unique_id = SConfig::GetInstance().m_strUniqueID.c_str();

	std::string gs_cache_filename = StringFromFormat("%sdx11-%s-gs.cache", shader_cache_path.c_str(), title_unique_id.c_str());
	std::string ps_cache_filename = StringFromFormat("%sdx11-%s-ps.cache", shader_cache_path.c_str(), title_unique_id.c_str());
	std::string vs_cache_filename = StringFromFormat("%sdx11-%s-vs.cache", shader_cache_path.c_str(), title_unique_id.c_str());

	ShaderCacheInserter<GeometryShaderUid, GsBytecodeCache, &s_gs_bytecode_cache> gs_inserter;
	s_gs_disk_cache.OpenAndRead(gs_cache_filename, gs_inserter);

	ShaderCacheInserter<PixelShaderUid, PsBytecodeCache, &s_ps_bytecode_cache> ps_inserter;
	s_ps_disk_cache.OpenAndRead(ps_cache_filename, ps_inserter);

	ShaderCacheInserter<VertexShaderUid, VsBytecodeCache, &s_vs_bytecode_cache> vs_inserter;
	s_vs_disk_cache.OpenAndRead(vs_cache_filename, vs_inserter);

	// Clear out cache when debugging shaders to ensure stale ones don't stick around..
	if (g_Config.bEnableShaderDebugging)
		Clear();

	SETSTAT(stats.numPixelShadersAlive, (int)s_ps_bytecode_cache.size());
	SETSTAT(stats.numPixelShadersCreated, (int)s_ps_bytecode_cache.size());
	SETSTAT(stats.numVertexShadersAlive, (int)s_vs_bytecode_cache.size());
	SETSTAT(stats.numVertexShadersCreated, (int)s_vs_bytecode_cache.size());
}

void ShaderCache::Clear()
{
	for (auto& iter : s_shader_blob_list)
		SAFE_RELEASE(iter);

	s_shader_blob_list.clear();

	s_gs_bytecode_cache.clear();
	s_ps_bytecode_cache.clear();
	s_vs_bytecode_cache.clear();

	s_last_geometry_shader_bytecode = {};
	s_last_geometry_shader_uid = {};

	s_last_pixel_shader_bytecode = {};
	s_last_pixel_shader_uid = {};

	s_last_vertex_shader_bytecode = {};
	s_last_vertex_shader_uid = {};
}

void ShaderCache::Shutdown()
{
	Clear();

	s_gs_disk_cache.Sync();
	s_gs_disk_cache.Close();
	s_ps_disk_cache.Sync();
	s_ps_disk_cache.Close();
	s_vs_disk_cache.Sync();
	s_vs_disk_cache.Close();

	if (g_Config.bEnableShaderDebugging)
	{
		s_gs_hlsl_cache.clear();
		s_ps_hlsl_cache.clear();
		s_vs_hlsl_cache.clear();
	}

	s_geometry_uid_checker.Invalidate();
	s_pixel_uid_checker.Invalidate();
	s_vertex_uid_checker.Invalidate();
}

void ShaderCache::LoadAndSetActiveShaders(DSTALPHA_MODE ps_dst_alpha_mode, u32 gs_primitive_type)
{
	SetCurrentPrimitiveTopology(gs_primitive_type);

	GeometryShaderUid gs_uid = GetGeometryShaderUid(gs_primitive_type, API_D3D);
	PixelShaderUid ps_uid = GetPixelShaderUid(ps_dst_alpha_mode, API_D3D);
	VertexShaderUid vs_uid = GetVertexShaderUid(API_D3D);

	bool gs_changed = gs_uid != s_last_geometry_shader_uid;
	bool ps_changed = ps_uid != s_last_pixel_shader_uid;
	bool vs_changed = vs_uid != s_last_vertex_shader_uid;

	if (!gs_changed && !ps_changed && !vs_changed)
	{
		return;
	}

	if (gs_changed)
	{
		HandleGSUIDChange(gs_uid, gs_primitive_type);
	}

	if (ps_changed)
	{
		HandlePSUIDChange(ps_uid, ps_dst_alpha_mode);
	}

	if (vs_changed)
	{
		HandleVSUIDChange(vs_uid);
	}

	// A Uid has changed, so the PSO will need to be reset at next ApplyState.
	D3D::command_list_mgr->m_dirty_pso = true;
}

void ShaderCache::SetCurrentPrimitiveTopology(u32 gs_primitive_type)
{
	switch (gs_primitive_type)
	{
	case PRIMITIVE_TRIANGLES:
		s_current_primitive_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		break;
	case PRIMITIVE_LINES:
		s_current_primitive_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		break;
	case PRIMITIVE_POINTS:
		s_current_primitive_topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		break;
	default:
		CHECK(0, "Invalid primitive type.");
		break;
	}
}

void ShaderCache::HandleGSUIDChange(GeometryShaderUid gs_uid, u32 gs_primitive_type)
{
	s_last_geometry_shader_uid = gs_uid;

	if (g_ActiveConfig.bEnableShaderDebugging)
	{
		ShaderCode code = GenerateGeometryShaderCode(gs_primitive_type, API_D3D);
		s_geometry_uid_checker.AddToIndexAndCheck(code, gs_uid, "Geometry", "g");
	}

	if (gs_uid.GetUidData()->IsPassthrough())
	{
		s_last_geometry_shader_bytecode = {};
		return;
	}

	auto gs_iterator = s_gs_bytecode_cache.find(gs_uid);
	if (gs_iterator != s_gs_bytecode_cache.end())
	{
		s_last_geometry_shader_bytecode = gs_iterator->second;
	}
	else
	{
		ShaderCode gs_code = GenerateGeometryShaderCode(gs_primitive_type, API_D3D);
		D3DBlob* gs_bytecode = nullptr;

		if (!D3D::CompileGeometryShader(gs_code.GetBuffer(), &gs_bytecode))
		{
			GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
			return;
		}

		s_last_geometry_shader_bytecode = InsertByteCode<GeometryShaderUid, GsBytecodeCache>(gs_uid, &s_gs_bytecode_cache, gs_bytecode);
		s_gs_disk_cache.Append(gs_uid, gs_bytecode->Data(), gs_bytecode->Size());

		if (g_ActiveConfig.bEnableShaderDebugging && gs_bytecode)
		{
			s_gs_hlsl_cache[gs_uid] = gs_code.GetBuffer();
		}
	}
}

void ShaderCache::HandlePSUIDChange(PixelShaderUid ps_uid, DSTALPHA_MODE ps_dst_alpha_mode)
{
	s_last_pixel_shader_uid = ps_uid;

	if (g_ActiveConfig.bEnableShaderDebugging)
	{
		ShaderCode code = GeneratePixelShaderCode(ps_dst_alpha_mode, API_D3D);
		s_pixel_uid_checker.AddToIndexAndCheck(code, ps_uid, "Pixel", "p");
	}

	auto ps_iterator = s_ps_bytecode_cache.find(ps_uid);
	if (ps_iterator != s_ps_bytecode_cache.end())
	{
		s_last_pixel_shader_bytecode = ps_iterator->second;
		GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
	}
	else
	{
		ShaderCode ps_code = GeneratePixelShaderCode(ps_dst_alpha_mode, API_D3D);
		D3DBlob* ps_bytecode = nullptr;

		if (!D3D::CompilePixelShader(ps_code.GetBuffer(), &ps_bytecode))
		{
			GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
			return;
		}

		s_last_pixel_shader_bytecode = InsertByteCode<PixelShaderUid, PsBytecodeCache>(ps_uid, &s_ps_bytecode_cache, ps_bytecode);
		s_ps_disk_cache.Append(ps_uid, ps_bytecode->Data(), ps_bytecode->Size());

		SETSTAT(stats.numPixelShadersAlive, (int)s_ps_bytecode_cache.size());
		INCSTAT(stats.numPixelShadersCreated);

		if (g_ActiveConfig.bEnableShaderDebugging && ps_bytecode)
		{
			s_ps_hlsl_cache[ps_uid] = ps_code.GetBuffer();
		}
	}
}

void ShaderCache::HandleVSUIDChange(VertexShaderUid vs_uid)
{
	s_last_vertex_shader_uid = vs_uid;

	if (g_ActiveConfig.bEnableShaderDebugging)
	{
		ShaderCode code = GenerateVertexShaderCode(API_D3D);
		s_vertex_uid_checker.AddToIndexAndCheck(code, vs_uid, "Vertex", "v");
	}

	auto vs_iterator = s_vs_bytecode_cache.find(vs_uid);
	if (vs_iterator != s_vs_bytecode_cache.end())
	{
		s_last_vertex_shader_bytecode = vs_iterator->second;
		GFX_DEBUGGER_PAUSE_AT(NEXT_PIXEL_SHADER_CHANGE, true);
	}
	else
	{
		ShaderCode vs_code = GenerateVertexShaderCode(API_D3D);
		D3DBlob* vs_bytecode = nullptr;

		if (!D3D::CompileVertexShader(vs_code.GetBuffer(), &vs_bytecode))
		{
			GFX_DEBUGGER_PAUSE_AT(NEXT_ERROR, true);
			return;
		}

		s_last_vertex_shader_bytecode = InsertByteCode<VertexShaderUid, VsBytecodeCache>(vs_uid, &s_vs_bytecode_cache, vs_bytecode);
		s_vs_disk_cache.Append(vs_uid, vs_bytecode->Data(), vs_bytecode->Size());

		SETSTAT(stats.numVertexShadersAlive, (int)s_vs_bytecode_cache.size());
		INCSTAT(stats.numVertexShadersCreated);

		if (g_ActiveConfig.bEnableShaderDebugging && vs_bytecode)
		{
			s_vs_hlsl_cache[vs_uid] = vs_code.GetBuffer();
		}
	}
}

template<class UidType, class ShaderCacheType>
D3D12_SHADER_BYTECODE ShaderCache::InsertByteCode(const UidType& uid, ShaderCacheType* shader_cache, D3DBlob* bytecode_blob)
{
	// Note: Don't release the incoming bytecode, we need it to stick around, since in D3D12
	// the raw bytecode itself is bound. It is released at Shutdown() time.

	s_shader_blob_list.push_back(bytecode_blob);

	D3D12_SHADER_BYTECODE shader_bytecode;
	shader_bytecode.pShaderBytecode = bytecode_blob->Data();
	shader_bytecode.BytecodeLength = bytecode_blob->Size();

	(*shader_cache)[uid] = shader_bytecode;

	return shader_bytecode;
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE ShaderCache::GetCurrentPrimitiveTopology() { return s_current_primitive_topology; }

D3D12_SHADER_BYTECODE ShaderCache::GetActiveGeometryShaderBytecode() { return s_last_geometry_shader_bytecode; }
D3D12_SHADER_BYTECODE ShaderCache::GetActivePixelShaderBytecode() { return s_last_pixel_shader_bytecode; }
D3D12_SHADER_BYTECODE ShaderCache::GetActiveVertexShaderBytecode() { return s_last_vertex_shader_bytecode; }

const GeometryShaderUid* ShaderCache::GetActiveGeometryShaderUid() { return &s_last_geometry_shader_uid; }
const PixelShaderUid* ShaderCache::GetActivePixelShaderUid() { return &s_last_pixel_shader_uid; }
const VertexShaderUid* ShaderCache::GetActiveVertexShaderUid() { return &s_last_vertex_shader_uid; }

D3D12_SHADER_BYTECODE ShaderCache::GetGeometryShaderFromUid(const GeometryShaderUid* uid) { return s_gs_bytecode_cache[*uid]; }
D3D12_SHADER_BYTECODE ShaderCache::GetPixelShaderFromUid(const PixelShaderUid* uid) { return s_ps_bytecode_cache[*uid]; }
D3D12_SHADER_BYTECODE ShaderCache::GetVertexShaderFromUid(const VertexShaderUid* uid) { return s_vs_bytecode_cache[*uid]; }

}