// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>
#include <string>

#include "Common/FileUtil.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
#include "VideoBackends/D3D12/D3DBase.h"
#include "VideoBackends/D3D12/D3DShader.h"
#include "VideoCommon/VideoConfig.h"

namespace DX12
{

namespace D3D
{

bool CompileShader(const std::string& code, ID3DBlob** blob, const D3D_SHADER_MACRO* defines, std::string shader_version_string)
{
	ID3D10Blob* shader_buffer = nullptr;
	ID3D10Blob* error_buffer = nullptr;

#if defined(_DEBUG) || defined(DEBUGFAST)
	UINT flags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY | D3DCOMPILE_DEBUG;
#else
	UINT flags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY | D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_SKIP_VALIDATION;
#endif
	HRESULT hr = d3d_compile(code.c_str(), code.length(), nullptr, defines, nullptr, "main", shader_version_string.data(),
		flags, 0, &shader_buffer, &error_buffer);

	if (error_buffer)
	{
		INFO_LOG(VIDEO, "Shader compiler messages:\n%s\n",
			static_cast<const char*>(error_buffer->GetBufferPointer()));
	}

	if (FAILED(hr))
	{
		static int num_failures = 0;
		std::string filename = StringFromFormat("%sbad_%s_%04i.txt", File::GetUserPath(D_DUMP_IDX).c_str(), shader_version_string, num_failures++);
		std::ofstream file;
		OpenFStream(file, filename, std::ios_base::out);
		file << code;
		file.close();

		PanicAlert("Failed to compile shader: %s\nDebug info (%s):\n%s",
			filename.c_str(), shader_version_string, static_cast<const char*>(error_buffer->GetBufferPointer()));

		*blob = nullptr;
		error_buffer->Release();
	}
	else
	{
		*blob = shader_buffer;
	}

	return SUCCEEDED(hr);
}

// code->bytecode
bool CompileVertexShader(const std::string& code, ID3DBlob** blob)
{
	return CompileShader(code, blob, nullptr, D3D::VertexShaderVersionString());
}

// code->bytecode
bool CompileGeometryShader(const std::string& code, ID3DBlob** blob, const D3D_SHADER_MACRO* defines)
{
	return CompileShader(code, blob, defines, D3D::GeometryShaderVersionString());
}

// code->bytecode
bool CompilePixelShader(const std::string& code, ID3DBlob** blob, const D3D_SHADER_MACRO* defines)
{
	return CompileShader(code, blob, defines, D3D::PixelShaderVersionString());
}

}  // namespace

}  // namespace DX12
