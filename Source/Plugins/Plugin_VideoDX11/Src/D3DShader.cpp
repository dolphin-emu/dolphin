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

#include <string>

#include "VideoConfig.h"

#include "D3DShader.h"

namespace DX11
{

namespace D3D
{

// bytecode->shader
SharedPtr<ID3D11VertexShader> CreateVertexShaderFromByteCode(const void* bytecode, unsigned int len)
{
	ID3D11VertexShader* v_shader = nullptr;
	HRESULT hr = D3D::g_device->CreateVertexShader(bytecode, len, NULL, &v_shader);
	if (FAILED(hr))
		PanicAlert("CreateVertexShaderFromByteCode failed from %p (size %d) at %s %d\n",
			bytecode, len, __FILE__, __LINE__);

	return SharedPtr<ID3D11VertexShader>::FromPtr(v_shader);
}

// bytecode->shader
SharedPtr<ID3D11GeometryShader> CreateGeometryShaderFromByteCode(const void* bytecode, unsigned int len)
{
	ID3D11GeometryShader* g_shader = nullptr;
	HRESULT hr = D3D::g_device->CreateGeometryShader(bytecode, len, NULL, &g_shader);
	if (FAILED(hr))
		PanicAlert("CreateGeometryShaderFromByteCode failed from %p (size %d) at %s %d\n",
			bytecode, len, __FILE__, __LINE__);

	return SharedPtr<ID3D11GeometryShader>::FromPtr(g_shader);
}

// bytecode->shader
SharedPtr<ID3D11PixelShader> CreatePixelShaderFromByteCode(const void* bytecode, unsigned int len)
{
	ID3D11PixelShader* p_shader = nullptr;
	HRESULT hr = D3D::g_device->CreatePixelShader(bytecode, len, NULL, &p_shader);
	if (FAILED(hr))
		PanicAlert("CreatePixelShaderFromByteCode failed at %s %d\n", __FILE__, __LINE__);

	return SharedPtr<ID3D11PixelShader>::FromPtr(p_shader);
}

static SharedPtr<ID3D10Blob> CompileShader(const char* code, unsigned int len,
	const char* ver_str, const D3D_SHADER_MACRO* pDefines = NULL)
{
	static const UINT shader_compilation_flags = D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY

#if defined(_DEBUG) || defined(DEBUGFAST)
		| D3D10_SHADER_DEBUG | D3D10_SHADER_WARNINGS_ARE_ERRORS;
#else
		| D3D10_SHADER_OPTIMIZATION_LEVEL3 | D3D10_SHADER_SKIP_VALIDATION;
#endif

	ID3D10Blob* shaderBuffer = nullptr;
	ID3D10Blob* errorBuffer = nullptr;

	HRESULT hr = PD3DX11CompileFromMemory(code, len, NULL, pDefines, NULL, "main", ver_str,
		shader_compilation_flags, 0, NULL, &shaderBuffer, &errorBuffer, NULL);

	if (FAILED(hr) && g_ActiveConfig.bShowShaderErrors)
	{
		std::string msg = (const char*)errorBuffer->GetBufferPointer();
		msg += "\n\n";
		msg += ver_str;
		msg += "\n\n";
		msg += code;
		MessageBoxA(0, msg.c_str(), "Error compiling shader", MB_ICONERROR);
	}

	if (errorBuffer)
	{
		INFO_LOG(VIDEO, "Shader %s compiler messages:\n%s\n", ver_str,
			(const char*)errorBuffer->GetBufferPointer());

		errorBuffer->Release();
	}

	return SharedPtr<ID3D10Blob>::FromPtr(shaderBuffer);
}

// code->bytecode
SharedPtr<ID3D10Blob> CompileVertexShader(const char* code, unsigned int len)
{
	return CompileShader(code, len, D3D::VertexShaderVersionString());
}

// code->bytecode
SharedPtr<ID3D10Blob> CompileGeometryShader(const char* code, unsigned int len,
	const D3D_SHADER_MACRO* pDefines)
{
	return CompileShader(code, len, D3D::GeometryShaderVersionString());
}

// code->bytecode
SharedPtr<ID3D10Blob> CompilePixelShader(const char* code, unsigned int len,
	const D3D_SHADER_MACRO* pDefines)
{
	return CompileShader(code, len, D3D::PixelShaderVersionString(), pDefines);
}

SharedPtr<ID3D11VertexShader> CompileAndCreateVertexShader(const char* code, unsigned int len,
	SharedPtr<ID3D10Blob>* bytecode)
{
	auto const blob = CompileVertexShader(code, len);
	if (blob)
	{
		if (bytecode)
			*bytecode = blob;
		return CreateVertexShaderFromByteCode(blob->GetBufferPointer(), (unsigned int)blob->GetBufferSize());
	}
	else
	{
		PanicAlert("Failed to compile and create vertex shader from %p (size %d) at %s %d\n",
			code, len, __FILE__, __LINE__);
		return SharedPtr<ID3D11VertexShader>::FromPtr(nullptr);
	}
}

SharedPtr<ID3D11GeometryShader> CompileAndCreateGeometryShader(const char* code, unsigned int len,
	const D3D_SHADER_MACRO* pDefines, SharedPtr<ID3D10Blob>* bytecode)
{
	auto const blob = CompileGeometryShader(code, len, pDefines);
	if (blob)
	{
		if (bytecode)
			*bytecode = blob;
		return CreateGeometryShaderFromByteCode(blob->GetBufferPointer(), (unsigned int)blob->GetBufferSize());
	}
	else
	{
		PanicAlert("Failed to compile and create geometry shader from %p (size %d) at %s %d\n",
			code, len, __FILE__, __LINE__);
		return SharedPtr<ID3D11GeometryShader>::FromPtr(nullptr);
	}
}

SharedPtr<ID3D11PixelShader> CompileAndCreatePixelShader(const char* code, unsigned int len,
	const D3D_SHADER_MACRO* pDefines, SharedPtr<ID3D10Blob>* bytecode)
{
	auto const blob = CompilePixelShader(code, len, pDefines);
	if (blob)
	{
		if (bytecode)
			*bytecode = blob;
		return CreatePixelShaderFromByteCode(blob->GetBufferPointer(), (unsigned int)blob->GetBufferSize());
	}
	else
	{
		PanicAlert("Failed to compile and create pixel shader, %s %d\n", __FILE__, __LINE__);
		return SharedPtr<ID3D11PixelShader>::FromPtr(nullptr);
	}
}

}  // namespace

}  // namespace DX11