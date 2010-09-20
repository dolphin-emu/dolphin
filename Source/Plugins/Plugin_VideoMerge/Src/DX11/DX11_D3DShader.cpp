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

#include <d3dx11.h>
#include <d3dcompiler.h>
#include <string>

#include "VideoConfig.h"
#include "DX11_D3DShader.h"

namespace DX11
{

namespace D3D
{

// bytecode->shader
ID3D11VertexShader* CreateVertexShaderFromByteCode(void* bytecode, unsigned int len)
{
	ID3D11VertexShader* v_shader;
	HRESULT hr = D3D::device->CreateVertexShader(bytecode, len, NULL, &v_shader);
	if (FAILED(hr))
	{
		PanicAlert("CreateVertexShaderFromByteCode failed from %p (size %d) at %s %d\n", bytecode, len, __FILE__, __LINE__);
		v_shader = NULL;
	}
	return v_shader;
}

// code->bytecode
bool CompileVertexShader(const char* code, unsigned int len, D3DBlob** blob)
{
	ID3D10Blob* shaderBuffer = NULL;
	ID3D10Blob* errorBuffer = NULL;

#if defined(_DEBUG) || defined(DEBUGFAST)
	UINT flags = D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY|D3D10_SHADER_DEBUG|D3D10_SHADER_WARNINGS_ARE_ERRORS;
#else
	UINT flags = D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY|D3D10_SHADER_OPTIMIZATION_LEVEL3|D3D10_SHADER_SKIP_VALIDATION;
#endif
	HRESULT hr = PD3DX11CompileFromMemory(code, len, NULL, NULL, NULL, "main", D3D::VertexShaderVersionString(),
							flags, 0, NULL, &shaderBuffer, &errorBuffer, NULL);

	if (FAILED(hr) || errorBuffer)
	{
		std::string msg = (char*)errorBuffer->GetBufferPointer();
		msg += "\n\n";
		msg += code;
		MessageBoxA(0, msg.c_str(), "Error compiling pixel shader", MB_ICONERROR);

		*blob = NULL;
		errorBuffer->Release();
	}
	else
	{
		*blob = new D3DBlob(shaderBuffer);
		shaderBuffer->Release();
	}
	return SUCCEEDED(hr);
}

// bytecode->shader
ID3D11PixelShader* CreatePixelShaderFromByteCode(void* bytecode, unsigned int len)
{
	ID3D11PixelShader* p_shader;
	HRESULT hr = D3D::device->CreatePixelShader(bytecode, len, NULL, &p_shader);
	if (FAILED(hr))
	{
		PanicAlert("CreatePixelShaderFromByteCode failed at %s %d\n", __FILE__, __LINE__);
		p_shader = NULL;
	}
	return p_shader;
}

// code->bytecode
bool CompilePixelShader(const char* code, unsigned int len, D3DBlob** blob)
{
	ID3D10Blob* shaderBuffer = NULL;
	ID3D10Blob* errorBuffer = NULL;

#if defined(_DEBUG) || defined(DEBUGFAST)
	UINT flags = D3D10_SHADER_DEBUG|D3D10_SHADER_WARNINGS_ARE_ERRORS;
#else
	UINT flags = D3D10_SHADER_OPTIMIZATION_LEVEL3;
#endif
	HRESULT hr = PD3DX11CompileFromMemory(code, len, NULL, NULL, NULL, "main", D3D::PixelShaderVersionString(),
							flags, 0, NULL, &shaderBuffer, &errorBuffer, NULL);

	if (FAILED(hr) || errorBuffer)
	{
		std::string msg = (char*)errorBuffer->GetBufferPointer();
		msg += "\n\n";
		msg += code;
		MessageBoxA(0, msg.c_str(), "Error compiling pixel shader", MB_ICONERROR);

		*blob = NULL;
		errorBuffer->Release();
	}
	else
	{
		*blob = new D3DBlob(shaderBuffer);
		shaderBuffer->Release();
	}
	return SUCCEEDED(hr);
}

ID3D11VertexShader* CompileAndCreateVertexShader(const char* code, unsigned int len)
{
	D3DBlob* blob = NULL;
	if (CompileVertexShader(code, len, &blob))
	{
		ID3D11VertexShader* v_shader = CreateVertexShaderFromByteCode(blob);
		blob->Release();
		return v_shader;
	}
	PanicAlert("Failed to compile and create vertex shader from %p (size %d) at %s %d\n", code, len, __FILE__, __LINE__);
	return NULL;
}

ID3D11PixelShader* CompileAndCreatePixelShader(const char* code, unsigned int len)
{
	D3DBlob* blob = NULL;
	CompilePixelShader(code, len, &blob);
	if (blob)
	{
		ID3D11PixelShader* p_shader = CreatePixelShaderFromByteCode(blob);
		blob->Release();
		return p_shader;
	}
	PanicAlert("Failed to compile and create pixel shader, %s %d\n", __FILE__, __LINE__);
	return NULL;
}

}  // namespace

}
