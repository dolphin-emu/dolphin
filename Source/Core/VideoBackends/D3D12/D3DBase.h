// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#define USE_D3D12_QUEUED_COMMAND_LISTS

// D3D12TODO: Support this from Graphics Settings, not require a recompile to enable.
//#define USE_D3D12_DEBUG_LAYER

#pragma once

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <memory>
#include <vector>

#include "../../Externals/d3dx12/d3dx12.h"

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"

namespace DX12
{

#define SAFE_RELEASE(x) { if (x) (x)->Release(); (x) = nullptr; }
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

enum GRAPHICS_ROOT_PARAMETER : u32
{
	DESCRIPTOR_TABLE_PS_SRV,
	DESCRIPTOR_TABLE_PS_SAMPLER,
	DESCRIPTOR_TABLE_GS_CBV,
	DESCRIPTOR_TABLE_VS_CBV,
	DESCRIPTOR_TABLE_PS_CBVONE,
	DESCRIPTOR_TABLE_PS_CBVTWO,
	DESCRIPTOR_TABLE_PS_UAV,
	NUM_GRAPHICS_ROOT_PARAMETERS
};

namespace D3D
{

HRESULT LoadDXGI();
HRESULT LoadD3D();
HRESULT LoadD3DCompiler();
void UnloadDXGI();
void UnloadD3D();
void UnloadD3DCompiler();

std::vector<DXGI_SAMPLE_DESC> EnumAAModes(ID3D12Device* device);

HRESULT Create(HWND wnd);

void CreateDescriptorHeaps();
void CreateRootSignatures();

void WaitForOutstandingRenderingToComplete();
void Close();

extern ID3D12Device* device12;

extern unsigned int resource_descriptor_size;
extern unsigned int sampler_descriptor_size;
extern std::unique_ptr<D3DDescriptorHeapManager> gpu_descriptor_heap_mgr;
extern std::unique_ptr<D3DDescriptorHeapManager> sampler_descriptor_heap_mgr;
extern std::unique_ptr<D3DDescriptorHeapManager> dsv_descriptor_heap_mgr;
extern std::unique_ptr<D3DDescriptorHeapManager> rtv_descriptor_heap_mgr;
extern std::array<ID3D12DescriptorHeap*, 2> gpu_descriptor_heaps;


extern D3D12_CPU_DESCRIPTOR_HANDLE null_srv_cpu;
extern D3D12_CPU_DESCRIPTOR_HANDLE null_srv_cpu_shadow;

extern std::unique_ptr<D3DCommandListManager> command_list_mgr;
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

	return name;
}

}  // namespace D3D

using CREATEDXGIFACTORY = HRESULT(WINAPI*)(REFIID, void**);
extern CREATEDXGIFACTORY create_dxgi_factory;

using D3D12CREATEDEVICE = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
extern D3D12CREATEDEVICE d3d12_create_device;

using D3D12SERIALIZEROOTSIGNATURE = HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC* pRootSignature, D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob** ppBlob, ID3DBlob** ppErrorBlob);
using D3D12GETDEBUGINTERFACE = HRESULT(WINAPI*)(REFIID riid, void** ppvDebug);

using D3DREFLECT = HRESULT(WINAPI*)(LPCVOID, SIZE_T, REFIID, void**);
extern D3DREFLECT d3d_reflect;

using D3DCREATEBLOB = HRESULT(WINAPI*)(SIZE_T, ID3DBlob**);
extern D3DCREATEBLOB d3d_create_blob;

extern pD3DCompile d3d_compile;

}  // namespace DX12
