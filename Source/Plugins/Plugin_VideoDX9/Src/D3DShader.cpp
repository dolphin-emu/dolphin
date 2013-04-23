// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <d3dx9.h>
#include <string>

#include "VideoConfig.h"
#include "D3DShader.h"

namespace DX9
{

namespace D3D
{

// bytecode->shader.
LPDIRECT3DVERTEXSHADER9 CreateVertexShaderFromByteCode(const u8 *bytecode, int len)
{
	LPDIRECT3DVERTEXSHADER9 v_shader;
	HRESULT hr = D3D::dev->CreateVertexShader((DWORD *)bytecode, &v_shader);
	if (FAILED(hr))
		return NULL;

	return v_shader;
}

// code->bytecode.
bool CompileVertexShader(const char *code, int len, u8 **bytecode, int *bytecodelen)
{
	LPD3DXBUFFER shaderBuffer = NULL;
	LPD3DXBUFFER errorBuffer = NULL;
	HRESULT hr = PD3DXCompileShader(code, len, 0, 0, "main", D3D::VertexShaderVersionString(),
						           0, &shaderBuffer, &errorBuffer, 0);
	if (FAILED(hr))
	{
		static int num_failures = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%sbad_vs_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
		std::ofstream file;
		OpenFStream(file, szTemp, std::ios_base::out);
		file << code;
		file.close();

		PanicAlert("Failed to compile vertex shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
						szTemp,
						D3D::VertexShaderVersionString(),
						(char*)errorBuffer->GetBufferPointer());

		*bytecode = NULL;
		*bytecodelen = 0;
	}
	else
	{
		*bytecodelen = shaderBuffer->GetBufferSize();
		*bytecode = new u8[*bytecodelen];
		memcpy(*bytecode, shaderBuffer->GetBufferPointer(), *bytecodelen);
	}

	//cleanup
	if (shaderBuffer)
		shaderBuffer->Release();
	if (errorBuffer)
		errorBuffer->Release();
	return SUCCEEDED(hr) ? true : false;
}

// bytecode->shader
LPDIRECT3DPIXELSHADER9 CreatePixelShaderFromByteCode(const u8 *bytecode, int len)
{
	LPDIRECT3DPIXELSHADER9 p_shader;
	HRESULT hr = D3D::dev->CreatePixelShader((DWORD *)bytecode, &p_shader);
	if (FAILED(hr))
		return NULL;

	return p_shader;
}

// code->bytecode
bool CompilePixelShader(const char *code, int len, u8 **bytecode, int *bytecodelen)
{
	LPD3DXBUFFER shaderBuffer = 0;
	LPD3DXBUFFER errorBuffer = 0;

	// Someone:
	// For some reason, I had this kind of errors : "Shader uses texture addressing operations
	// in a dependency chain that is too complex for the target shader model (ps_2_0) to handle."
	HRESULT hr = PD3DXCompileShader(code, len, 0, 0, "main", D3D::PixelShaderVersionString(), 
				 		           0, &shaderBuffer, &errorBuffer, 0);

	if (FAILED(hr))
	{
		static int num_failures = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%sbad_ps_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), num_failures++);
		std::ofstream file;
		OpenFStream(file, szTemp, std::ios_base::out);
		file << code;
		file.close();

		PanicAlert("Failed to compile pixel shader!\nThis usually happens when trying to use Dolphin with an outdated GPU or integrated GPU like the Intel GMA series.\n\nIf you're sure this is Dolphin's error anyway, post the contents of %s along with this error message at the forums.\n\nDebug info (%s):\n%s",
						szTemp,
						D3D::PixelShaderVersionString(),
						(char*)errorBuffer->GetBufferPointer());

		*bytecode = NULL;
		*bytecodelen = 0;
	}
	else
	{
		*bytecodelen = shaderBuffer->GetBufferSize();
		*bytecode = new u8[*bytecodelen];
		memcpy(*bytecode, shaderBuffer->GetBufferPointer(), *bytecodelen);
	}

	//cleanup
	if (shaderBuffer)
		shaderBuffer->Release();
	if (errorBuffer)
		errorBuffer->Release();
	return SUCCEEDED(hr) ? true : false;
}

LPDIRECT3DVERTEXSHADER9 CompileAndCreateVertexShader(const char *code, int len)
{
	u8 *bytecode;
	int bytecodelen;
	if (CompileVertexShader(code, len, &bytecode, &bytecodelen))
	{
		LPDIRECT3DVERTEXSHADER9 v_shader = CreateVertexShaderFromByteCode(bytecode, len);
		delete [] bytecode;
		return v_shader;
	}
	return NULL;
}

LPDIRECT3DPIXELSHADER9 CompileAndCreatePixelShader(const char* code, unsigned int len)
{
	u8 *bytecode;
	int bytecodelen;
	if (CompilePixelShader(code, len, &bytecode, &bytecodelen))
	{
		LPDIRECT3DPIXELSHADER9 p_shader = CreatePixelShaderFromByteCode(bytecode, len);
		delete [] bytecode;
		return p_shader;
	}
	return NULL;
}

}  // namespace

}  // namespace DX9