// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace DX12
{

class StaticShaderCache final
{
public:
	static void Init();
	static void InvalidateMSAAShaders();
	static void Shutdown();

	// Pixel shaders
	static D3D12_SHADER_BYTECODE GetColorMatrixPixelShader(bool multisampled);
	static D3D12_SHADER_BYTECODE GetColorCopyPixelShader(bool multisampled);
	static D3D12_SHADER_BYTECODE GetDepthMatrixPixelShader(bool multisampled);
	static D3D12_SHADER_BYTECODE GetDepthResolveToColorPixelShader();
	static D3D12_SHADER_BYTECODE GetClearPixelShader();
	static D3D12_SHADER_BYTECODE GetAnaglyphPixelShader();
	static D3D12_SHADER_BYTECODE GetReinterpRGBA6ToRGB8PixelShader(bool multisampled);
	static D3D12_SHADER_BYTECODE GetReinterpRGB8ToRGBA6PixelShader(bool multisampled);
	static D3D12_SHADER_BYTECODE GetXFBEncodePixelShader();
	static D3D12_SHADER_BYTECODE GetXFBDecodePixelShader();

	// Vertex shaders
	static D3D12_SHADER_BYTECODE GetSimpleVertexShader();
	static D3D12_SHADER_BYTECODE GetClearVertexShader();
	static D3D12_INPUT_LAYOUT_DESC GetSimpleVertexShaderInputLayout();
	static D3D12_INPUT_LAYOUT_DESC GetClearVertexShaderInputLayout();

	// Geometry shaders
	static D3D12_SHADER_BYTECODE GetClearGeometryShader();
	static D3D12_SHADER_BYTECODE GetCopyGeometryShader();
};

}