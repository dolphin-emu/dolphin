// Copyright (C) 2003-2008 Dolphin Project.

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

#include "Config.h"
#include "D3DShader.h"

namespace D3D
{

LPDIRECT3DVERTEXSHADER9 CompileVertexShader(const char *code, int len)
{
	//try to compile
	LPD3DXBUFFER shaderBuffer = 0;
	LPD3DXBUFFER errorBuffer = 0;
	LPDIRECT3DVERTEXSHADER9 vShader = 0;
 	HRESULT hr = D3DXAssembleShader(code, len, 0, 0, 0, &shaderBuffer, &errorBuffer);
	/*if (FAILED(hr))
	{
		//let's try 2.0
		hr = D3DXCompileShader(code, len, 0, 0, "main", "vs_2_0", 0, &shaderBuffer, &errorBuffer, 0);
	}*/

	if (FAILED(hr))
	{
		//compilation error
		std::string hello = (char*)errorBuffer->GetBufferPointer();
		hello += "\n\n";
		hello += code;
		MessageBox(0, hello.c_str(), "Error assembling vertex shader", MB_ICONERROR);
		vShader = 0;
	}
	else if (SUCCEEDED(hr))
	{
		//create it
		HRESULT hr = E_FAIL;
		if (shaderBuffer)
			hr = D3D::dev->CreateVertexShader((DWORD *)shaderBuffer->GetBufferPointer(), &vShader);
		if (FAILED(hr) || vShader == 0)
		{
			MessageBox(0, code, (char*)errorBuffer->GetBufferPointer(),MB_ICONERROR);
		}
	}

	//cleanup
	if (shaderBuffer)
		shaderBuffer->Release();
	if (errorBuffer)
		errorBuffer->Release();

	return vShader;
}

LPDIRECT3DPIXELSHADER9 CompilePixelShader(const char *code, int len)
{
	LPD3DXBUFFER shaderBuffer = 0;
	LPD3DXBUFFER errorBuffer = 0;
	LPDIRECT3DPIXELSHADER9 pShader = 0;
	static char *versions[6] = {"ERROR", "ps_1_1", "ps_1_4", "ps_2_0", "ps_3_0", "ps_4_0"};

	HRESULT hr = D3DXAssembleShader(code, len, 0, 0, 0, &shaderBuffer, &errorBuffer);

	if (FAILED(hr))
	{
		std::string hello = (char*)errorBuffer->GetBufferPointer();
		hello += "\n\n";
		hello += code;
		MessageBox(0, hello.c_str(), "Error assembling pixel shader", MB_ICONERROR);
		pShader = 0;
	}
	else 
	{
		//create it
		HRESULT hr = D3D::dev->CreatePixelShader((DWORD *)shaderBuffer->GetBufferPointer(), &pShader);
		if (FAILED(hr) || pShader == 0)
		{
			MessageBox(0, "damn", "error creating pixelshader", MB_ICONERROR);
		}
	}

	//cleanup
	if (shaderBuffer)
		shaderBuffer->Release();
	if (errorBuffer)
		errorBuffer->Release();

	return pShader;
}

}  // namespace
