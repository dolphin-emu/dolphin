// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <vector>

#include "Common/Common.h"

namespace DX11
{

#define SAFE_RELEASE(x) { if (x) (x)->Release(); (x) = nullptr; }
#define SAFE_DELETE(x) { delete (x); (x) = nullptr; }
#define SAFE_DELETE_ARRAY(x) { delete[] (x); (x) = nullptr; }
#define CHECK(cond, Message, ...) if (!(cond)) { PanicAlert(__FUNCTION__ " failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); }

class D3DTexture2D;

namespace D3D
{

HRESULT LoadDXGI();
HRESULT LoadD3D();
HRESULT LoadD3DCompiler();
void UnloadDXGI();
void UnloadD3D();
void UnloadD3DCompiler();

D3D_FEATURE_LEVEL GetFeatureLevel(IDXGIAdapter* adapter);
std::vector<DXGI_SAMPLE_DESC> EnumAAModes(IDXGIAdapter* adapter);

HRESULT Create(HWND wnd);
void Close();

extern ID3D11Device* device;
extern ID3D11DeviceContext* context;
extern HWND hWnd;
extern bool bFrameInProgress;

void Reset();
bool BeginFrame();
void EndFrame();
void Present();

unsigned int GetBackBufferWidth();
unsigned int GetBackBufferHeight();
D3DTexture2D* &GetBackBuffer();
const char* PixelShaderVersionString();
const char* GeometryShaderVersionString();
const char* VertexShaderVersionString();
bool BGRATexturesSupported();

unsigned int GetMaxTextureSize();

HRESULT SetFullscreenState(bool enable_fullscreen);
HRESULT GetFullscreenState(bool* fullscreen_state);

// This function will assign a name to the given resource.
// The DirectX debug layer will make it easier to identify resources that way,
// e.g. when listing up all resources who have unreleased references.
template <typename T>
void SetDebugObjectName(T resource, const char* name)
{
	static_assert(std::is_convertible<T, ID3D11DeviceChild*>::value,
		"resource must be convertible to ID3D11DeviceChild*");
#if defined(_DEBUG) || defined(DEBUGFAST)
	if (resource)
		resource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)(name ? strlen(name) : 0), name);
#endif
}

template <typename T>
std::string GetDebugObjectName(T resource)
{
	static_assert(std::is_convertible<T, ID3D11DeviceChild*>::value,
		"resource must be convertible to ID3D11DeviceChild*");
	std::string name;
#if defined(_DEBUG) || defined(DEBUGFAST)
	if (resource)
	{
		UINT size = 0;
		resource->GetPrivateData(WKPDID_D3DDebugObjectName, &size, nullptr); //get required size
		name.resize(size);
		resource->GetPrivateData(WKPDID_D3DDebugObjectName, &size, const_cast<char*>(name.data()));
	}
#endif
	return name;
}

}  // namespace D3D

typedef HRESULT (WINAPI* CREATEDXGIFACTORY)(REFIID, void**);
extern CREATEDXGIFACTORY PCreateDXGIFactory;
typedef HRESULT (WINAPI* D3D11CREATEDEVICE)(IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, CONST D3D_FEATURE_LEVEL*, UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

typedef HRESULT (WINAPI* D3DREFLECT)(LPCVOID, SIZE_T, REFIID, void**);
extern D3DREFLECT PD3DReflect;
extern pD3DCompile PD3DCompile;

}  // namespace DX11
