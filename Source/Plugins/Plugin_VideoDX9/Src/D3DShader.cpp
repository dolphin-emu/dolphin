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

#include <d3dx9.h>
#include <string>

#include "VideoConfig.h"
#include "D3DShader.h"

namespace D3D
{

// Bytecode->shader.
LPDIRECT3DVERTEXSHADER9 CreateVertexShaderFromByteCode(const u8 *bytecode, int len)
{
	LPDIRECT3DVERTEXSHADER9 v_shader;
	HRESULT hr = D3D::dev->CreateVertexShader((DWORD *)bytecode, &v_shader);
	if (FAILED(hr))
		v_shader = 0;
	return v_shader;
}

// Code->bytecode.
bool CompileVertexShader(const char *code, int len, u8 **bytecode, int *bytecodelen)
{
	//try to compile
	LPD3DXBUFFER shaderBuffer = 0;
	LPD3DXBUFFER errorBuffer = 0;
 	HRESULT hr = D3DXCompileShader(code, len, 0, 0, "main", D3D::VertexShaderVersionString(),
						           0, &shaderBuffer, &errorBuffer, 0);
	if (FAILED(hr))
	{
		//compilation error
		if (g_ActiveConfig.bShowShaderErrors) {
			std::string hello = (char*)errorBuffer->GetBufferPointer();
			hello += "\n\n";
			hello += code;
			MessageBoxA(0, hello.c_str(), "Error compiling vertex shader", MB_ICONERROR);
		}
		*bytecode = 0;
		*bytecodelen = 0;
	}
	else if (SUCCEEDED(hr))
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


// Bytecode->shader.
LPDIRECT3DPIXELSHADER9 CreatePixelShaderFromByteCode(const u8 *bytecode, int len)
{
	LPDIRECT3DPIXELSHADER9 p_shader;
	HRESULT hr = D3D::dev->CreatePixelShader((DWORD *)bytecode, &p_shader);
	if (FAILED(hr))
		p_shader = 0;
	return p_shader;
}


bool CompilePixelShader(const char *code, int len, u8 **bytecode, int *bytecodelen)
{
	LPD3DXBUFFER shaderBuffer = 0;
	LPD3DXBUFFER errorBuffer = 0;

	// Someone:
	// For some reason, I had this kind of errors : "Shader uses texture addressing operations
	// in a dependency chain that is too complex for the target shader model (ps_2_0) to handle."
	HRESULT hr = D3DXCompileShader(code, len, 0, 0, "main", D3D::PixelShaderVersionString(), 
				 		           0, &shaderBuffer, &errorBuffer, 0);

	if (FAILED(hr))
	{
		if (g_ActiveConfig.bShowShaderErrors) {
			std::string hello = (char*)errorBuffer->GetBufferPointer();
			hello += "\n\n";
			hello += code;
			MessageBoxA(0, hello.c_str(), "Error compiling pixel shader", MB_ICONERROR);
		}
		*bytecode = 0;
		*bytecodelen = 0;
	}
	else if (SUCCEEDED(hr))
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

LPDIRECT3DVERTEXSHADER9 CompileAndCreateVertexShader(const char *code, int len) {
	u8 *bytecode;
	int bytecodelen;
	if (CompileVertexShader(code, len, &bytecode, &bytecodelen)) {
		LPDIRECT3DVERTEXSHADER9 v_shader = CreateVertexShaderFromByteCode(bytecode, len);
		delete [] bytecode;
		return v_shader;
	} else {
		return 0;
	}
}

LPDIRECT3DPIXELSHADER9 CompileAndCreatePixelShader(const char *code, int len) {
	u8 *bytecode;
	int bytecodelen;
	if (CompilePixelShader(code, len, &bytecode, &bytecodelen)) {
		LPDIRECT3DPIXELSHADER9 p_shader = CreatePixelShaderFromByteCode(bytecode, len);
		delete [] bytecode;
		return p_shader;
	} else {
		return 0;
	}
}

}  // namespace
