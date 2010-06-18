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

#include <d3d11.h>
#include "Common.h"
#include "D3DBlob.h"
#include "GfxState.h"

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

// Ihis function will assign a name to the given resource.
// The DirectX debug layer will make it easier to identify resources that way,
// e.g. when listing up all resources who have unreleased references.
inline void SetDebugObjectName(ID3D11DeviceChild* resource, const char* name)
{
#if defined(_DEBUG) || defined(DEBUGFAST)
	resource->SetPrivateData( WKPDID_D3DDebugObjectName, strlen(name), name);
#endif
}

}  // namespace
