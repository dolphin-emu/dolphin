// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/GeometryShaderGen.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/VertexShaderGen.h"

namespace DX12
{

class D3DBlob;

class ShaderCache final
{
public:
	static void Init();
	static void Clear();
	static void Shutdown();

	static void LoadAndSetActiveShaders(DSTALPHA_MODE ps_dst_alpha_mode, u32 gs_primitive_type);

	template<class UidType, class ShaderCacheType>
	static D3D12_SHADER_BYTECODE InsertByteCode(const UidType& uid, ShaderCacheType* shader_cache, ID3DBlob* bytecode_blob);

	static D3D12_SHADER_BYTECODE GetActiveGeometryShaderBytecode();
	static D3D12_SHADER_BYTECODE GetActivePixelShaderBytecode();
	static D3D12_SHADER_BYTECODE GetActiveVertexShaderBytecode();

	static const GeometryShaderUid* GetActiveGeometryShaderUid();
	static const PixelShaderUid*    GetActivePixelShaderUid();
	static const VertexShaderUid*   GetActiveVertexShaderUid();

	static D3D12_SHADER_BYTECODE GetGeometryShaderFromUid(const GeometryShaderUid* uid);
	static D3D12_SHADER_BYTECODE GetPixelShaderFromUid(const PixelShaderUid* uid);
	static D3D12_SHADER_BYTECODE GetVertexShaderFromUid(const VertexShaderUid* uid);

	static D3D12_PRIMITIVE_TOPOLOGY_TYPE GetCurrentPrimitiveTopology();

private:
	static void SetCurrentPrimitiveTopology(u32 gs_primitive_type);

	static void HandleGSUIDChange(GeometryShaderUid gs_uid, u32 gs_primitive_type);
	static void HandlePSUIDChange(PixelShaderUid ps_uid, DSTALPHA_MODE ps_dst_alpha_mode);
	static void HandleVSUIDChange(VertexShaderUid vs_uid);
};

}
