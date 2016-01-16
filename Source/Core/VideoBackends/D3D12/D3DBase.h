// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#define USE_D3D12_QUEUED_COMMAND_LISTS
//#define USE_D3D12_DEBUG_LAYER

#pragma once

#include <d3d11.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <vector>

#include "../../Externals/d3dx12/d3dx12.h"

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

namespace DX12
{

#define SAFE_RELEASE(x) { if (x) (x)->Release(); (x) = nullptr; }
#define SAFE_DELETE(x) { delete (x); (x) = nullptr; }
#define SAFE_DELETE_ARRAY(x) { delete[] (x); (x) = nullptr; }
#define CHECK(cond, Message, ...) if (!(cond)) { __debugbreak(); PanicAlert(__FUNCTION__ " failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); }

// DEBUGCHECK is for high-frequency functions that we only want to check on debug builds.
#if defined(_DEBUG) || defined(DEBUGFAST)
#define DEBUGCHECK(cond, Message, ...) if (!(cond)) { PanicAlert(__FUNCTION__ " failed in %s at line %d: " Message, __FILE__, __LINE__, __VA_ARGS__); }
#else
#define DEBUGCHECK(cond, Message, ...)
#endif

inline void CheckHR(HRESULT hr)
{
	CHECK(SUCCEEDED(hr), "Failed HRESULT.");
}

class D3DCommandListManager;
class D3DDescriptorHeapManager;
class D3DTexture2D;

namespace D3D
{

#define DESCRIPTOR_TABLE_PS_SRV     0
#define DESCRIPTOR_TABLE_PS_SAMPLER 1
#define DESCRIPTOR_TABLE_GS_CBV     2
#define DESCRIPTOR_TABLE_VS_CBV     3
// #define DESCRIPTOR_TABLE_PS_UAV     4
#define DESCRIPTOR_TABLE_PS_CBVONE  4
#define DESCRIPTOR_TABLE_PS_CBVTWO  5

HRESULT LoadDXGI();
HRESULT LoadD3D();
HRESULT LoadD3DCompiler();
void UnloadDXGI();
void UnloadD3D();
void UnloadD3DCompiler();

D3D_FEATURE_LEVEL GetFeatureLevel(IDXGIAdapter* adapter);
std::vector<DXGI_SAMPLE_DESC> EnumAAModes(IDXGIAdapter* adapter);

bool AlertUserIfSelectedAdapterDoesNotSupportD3D12();

HRESULT Create(HWND wnd);

void CreateDescriptorHeaps();
void CreateRootSignatures();

void WaitForOutstandingRenderingToComplete();
void Close();

extern ID3D12Device* device12;

extern unsigned int resource_descriptor_size;
extern unsigned int sampler_descriptor_size;
extern D3DDescriptorHeapManager* gpu_descriptor_heap_mgr;
extern D3DDescriptorHeapManager* sampler_descriptor_heap_mgr;
extern D3DDescriptorHeapManager* dsv_descriptor_heap_mgr;
extern D3DDescriptorHeapManager* rtv_descriptor_heap_mgr;
extern ID3D12DescriptorHeap* gpu_descriptor_heaps[2];


extern D3D12_CPU_DESCRIPTOR_HANDLE null_srv_cpu;
extern D3D12_CPU_DESCRIPTOR_HANDLE null_srv_cpu_shadow;

extern D3DCommandListManager* command_list_mgr;
extern ID3D12GraphicsCommandList* current_command_list;

extern ID3D12RootSignature* default_root_signature;

extern HWND hWnd;

void Reset();
bool BeginFrame();
void EndFrame();
void Present();

unsigned int GetBackBufferWidth();
unsigned int GetBackBufferHeight();
D3DTexture2D*& GetBackBuffer();
const std::string PixelShaderVersionString();
const std::string GeometryShaderVersionString();
const std::string VertexShaderVersionString();

unsigned int GetMaxTextureSize();

HRESULT SetFullscreenState(bool enable_fullscreen);
HRESULT GetFullscreenState(bool* fullscreen_state);

// This function will assign a name to the given resource.
// The DirectX debug layer will make it easier to identify resources that way,
// e.g. when listing up all resources who have unreleased references.
static void SetDebugObjectName12(ID3D12Resource* resource, LPCSTR name)
{
	HRESULT hr = resource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)(name ? strlen(name) : 0), name);
	if (FAILED(hr))
	{
		throw std::exception("Failure setting name for D3D12 object");
	}
}

static std::string GetDebugObjectName12(ID3D12Resource* resource)
{
	std::string name;
	if (resource)
	{
		UINT size = 0;
		resource->GetPrivateData(WKPDID_D3DDebugObjectName, &size, nullptr); //get required size
		name.resize(size);
		resource->GetPrivateData(WKPDID_D3DDebugObjectName, &size, const_cast<char*>(name.data()));
	}
}

}  // namespace D3D

typedef HRESULT (WINAPI* CREATEDXGIFACTORY)(REFIID, void**);
extern CREATEDXGIFACTORY create_dxgi_factory;

typedef HRESULT(WINAPI* D3D12CREATEDEVICE)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
typedef HRESULT(WINAPI* D3D12SERIALIZEROOTSIGNATURE)(const D3D12_ROOT_SIGNATURE_DESC* pRootSignature, D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob** ppBlob, ID3DBlob** ppErrorBlob);
typedef HRESULT(WINAPI* D3D12GETDEBUGINTERFACE)(REFIID riid, void** ppvDebug);

typedef HRESULT (WINAPI* D3DREFLECT)(LPCVOID, SIZE_T, REFIID, void**);
extern D3DREFLECT d3d_reflect;
extern pD3DCompile d3d_compile;

}  // namespace DX12
