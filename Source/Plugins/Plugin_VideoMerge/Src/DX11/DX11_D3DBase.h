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

// Common
#include "Common.h"

// DX11
#include <d3dx11.h>
#include "DX11_GfxState.h"
#include "DX11_D3DBlob.h"

namespace DX11
{

#define SAFE_RELEASE(x) { if (x) (x)->Release(); (x) = NULL; }
#define SAFE_DELETE(x) { delete (x); (x) = NULL; }
#define SAFE_DELETE_ARRAY(x) { delete[] (x); (x) = NULL; }
#define CHECK(cond, Message, ...) if (!(cond)) { PanicAlert(__FUNCTION__ "Failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); }

class D3DTexture2D;
namespace D3D
{

HRESULT Create(HWND wnd);
void Close();

extern ID3D11Device* device;
extern ID3D11DeviceContext* context;
extern IDXGISwapChain* swapchain;
extern bool bFrameInProgress;

void Reset();
bool BeginFrame();
void EndFrame();
void Present();

unsigned int GetBackBufferWidth();
unsigned int GetBackBufferHeight();
D3DTexture2D* &GetBackBuffer();
const char* PixelShaderVersionString();
const char* VertexShaderVersionString();
bool BGRATexturesSupported();

unsigned int GetMaxTextureSize();

// Ihis function will assign a name to the given resource.
// The DirectX debug layer will make it easier to identify resources that way,
// e.g. when listing up all resources who have unreleased references.
inline void SetDebugObjectName(ID3D11DeviceChild* resource, const char* name)
{
#if defined(_DEBUG) || defined(DEBUGFAST)
	resource->SetPrivateData( WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
#endif
}

}  // namespace


// Used to not require the SDK and runtime versions to match:
// Linking with d3dx11.lib makes the most recent d3dx11_xx.dll of the
// compiler's SDK a requirement, but this plugin works with DX11 runtimes
// back to August 2009 even if the plugin was built with June 2010.
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

}
