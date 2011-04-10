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

#pragma once

#include <vector>

#include <D3DX11.h>
#include <D3Dcompiler.h>

#include "D3DUtil.h"
#include "Common.h"

namespace DX11 
{

#define SAFE_RELEASE(x) { if (x) (x)->Release(); (x) = NULL; }

#define CHECK(cond, Message, ...) if (!(cond)) { \
	PanicAlert(__FUNCTION__ "Failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); }

class D3DTexture2D;

SharedPtr<ID3D11Texture1D> CreateTexture1DShared(
	const D3D11_TEXTURE1D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData);
SharedPtr<ID3D11Texture2D> CreateTexture2DShared(
	const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData);

SharedPtr<ID3D11BlendState> CreateBlendStateShared(const D3D11_BLEND_DESC *pBlendStateDesc);

SharedPtr<ID3D11InputLayout> CreateInputLayoutShared(const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
	UINT NumElements, const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength);

SharedPtr<ID3D11Buffer> CreateBufferShared(const D3D11_BUFFER_DESC *pDesc,
	const D3D11_SUBRESOURCE_DATA *pInitialData);

namespace D3D
{

HRESULT LoadDXGI();
HRESULT LoadD3D();
HRESULT LoadD3DX();
HRESULT LoadD3DCompiler();
void UnloadDXGI();
void UnloadD3D();
void UnloadD3DX();
void UnloadD3DCompiler();

std::vector<DXGI_SAMPLE_DESC> EnumAAModes(IDXGIAdapter* adapter);
DXGI_SAMPLE_DESC GetAAMode(int index);

HRESULT Create(HWND wnd);
void Close();

extern SharedPtr<ID3D11Device> g_device;
extern SharedPtr<ID3D11DeviceContext> g_context;
extern SharedPtr<IDXGISwapChain> g_swapchain;

extern bool bFrameInProgress;

void Reset();
bool BeginFrame();
void EndFrame();
void Present();

unsigned int GetBackBufferWidth();
unsigned int GetBackBufferHeight();
D3DTexture2D* GetBackBuffer();
const char* PixelShaderVersionString();
const char* GeometryShaderVersionString();
const char* VertexShaderVersionString();
bool BGRATexturesSupported();

unsigned int GetMaxTextureSize();

// Ihis function will assign a name to the given resource.
// The DirectX debug layer will make it easier to identify resources that way,
// e.g. when listing up all resources who have unreleased references.
template <typename T>
void SetDebugObjectName(T resource, const char* name)
{
	static_assert(std::is_convertible<T, ID3D11DeviceChild*>::value,
		"resource must be convertible to ID3D11DeviceChild*");

#if defined(_DEBUG) || defined(DEBUGFAST)
	resource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
#endif
}

}  // namespace


// Used to not require the SDK and runtime versions to match:
// Linking with d3dx11.lib makes the most recent d3dx11_xx.dll of the
// compiler's SDK a requirement, but this backend works with DX11 runtimes
// back to August 2009 even if the backend was built with June 2010.
// Add any d3dx11 functions which you want to use here and load them in Create()
typedef HRESULT (WINAPI* D3DX11COMPILEFROMMEMORYTYPE)(LPCSTR, SIZE_T, LPCSTR, const D3D10_SHADER_MACRO*, LPD3D10INCLUDE, LPCSTR, LPCSTR, UINT, UINT, ID3DX11ThreadPump*, ID3D10Blob**, ID3D10Blob**, HRESULT*);
typedef HRESULT (WINAPI* D3DX11FILTERTEXTURETYPE)(ID3D11DeviceContext*, ID3D11Resource*, UINT, UINT);
typedef HRESULT (WINAPI* D3DX11SAVETEXTURETOFILEATYPE)(ID3D11DeviceContext*, ID3D11Resource*, D3DX11_IMAGE_FILE_FORMAT, LPCSTR);
typedef HRESULT (WINAPI* D3DX11SAVETEXTURETOFILEWTYPE)(ID3D11DeviceContext*, ID3D11Resource*, D3DX11_IMAGE_FILE_FORMAT, LPCWSTR);

extern D3DX11COMPILEFROMMEMORYTYPE PD3DX11CompileFromMemory;
extern D3DX11FILTERTEXTURETYPE PD3DX11FilterTexture;
extern D3DX11SAVETEXTURETOFILEATYPE PD3DX11SaveTextureToFileA;
extern D3DX11SAVETEXTURETOFILEWTYPE PD3DX11SaveTextureToFileW;

#ifdef UNICODE
#define PD3DX11SaveTextureToFile PD3DX11SaveTextureToFileW
#else
#define PD3DX11SaveTextureToFile PD3DX11SaveTextureToFileA
#endif

typedef HRESULT (WINAPI* CREATEDXGIFACTORY)(REFIID, void**);
extern CREATEDXGIFACTORY PCreateDXGIFactory;
typedef HRESULT (WINAPI* D3D11CREATEDEVICE)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, CONST D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);
extern D3D11CREATEDEVICE PD3D11CreateDevice;
typedef HRESULT (WINAPI* D3D10CREATEBLOB)(SIZE_T NumBytes, LPD3D10BLOB *ppBuffer);
extern D3D10CREATEBLOB PD3D10CreateBlob;

typedef HRESULT (WINAPI *D3DREFLECT)(LPCVOID, SIZE_T, REFIID, void**);
extern D3DREFLECT PD3DReflect;

}  // namespace DX11
