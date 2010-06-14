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

#include "D3DBase.h"
#include "D3DTexture.h"
#include "D3DShader.h"
#include "D3Dcompiler.h"
#include "VideoConfig.h"
#include "Render.h"
#include "XFStructs.h"

#include <map>
#include <string>
#include <algorithm>
using namespace std;

namespace D3D
{

ID3D11Device* device = NULL;
ID3D11DeviceContext* context = NULL;
IDXGISwapChain* swapchain = NULL;
D3D_FEATURE_LEVEL featlevel;
D3DTexture2D* backbuf = NULL;
HWND hWnd;

unsigned int xres, yres;

bool bFrameInProgress = false;

EmuGfxState::EmuGfxState() : vertexshader(NULL), vsbytecode(NULL), pixelshader(NULL), psbytecode(NULL)
{
	for (unsigned int k = 0;k < 8;k++)
	{
		float border[4] = {0.f, 0.f, 0.f, 0.f};
		shader_resources[k] = NULL;
		samplerdesc[k] = CD3D11_SAMPLER_DESC(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, 0.f, 16, D3D11_COMPARISON_ALWAYS, border, -D3D11_FLOAT32_MAX, D3D11_FLOAT32_MAX);
	}

	blenddesc.AlphaToCoverageEnable = FALSE;
	blenddesc.IndependentBlendEnable = FALSE;
	blenddesc.RenderTarget[0].BlendEnable = FALSE;
	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	blenddesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blenddesc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	blenddesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blenddesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

	depthdesc.DepthEnable        = TRUE;
	depthdesc.DepthWriteMask     = D3D11_DEPTH_WRITE_MASK_ALL;
	depthdesc.DepthFunc          = D3D11_COMPARISON_LESS;
	depthdesc.StencilEnable      = FALSE;
	depthdesc.StencilReadMask    = D3D11_DEFAULT_STENCIL_READ_MASK;
	depthdesc.StencilWriteMask   = D3D11_DEFAULT_STENCIL_WRITE_MASK;

	// this probably must be changed once multisampling support gets added
	rastdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE, false, 0, 0.f, 0, false, false, false, false);

	pscbuf = NULL;
	vscbuf = NULL;
	vshaderchanged = false;
	inp_layout = NULL;
	num_inp_elems = 0;

	pscbufchanged = false;
	vscbufchanged = false;
}

EmuGfxState::~EmuGfxState()
{
	for (unsigned int k = 0;k < 8;k++)
		SAFE_RELEASE(shader_resources[k])

	SAFE_RELEASE(vsbytecode);
	SAFE_RELEASE(psbytecode);
	SAFE_RELEASE(vertexshader);
	SAFE_RELEASE(pixelshader);

	SAFE_RELEASE(pscbuf);
	SAFE_RELEASE(vscbuf);

	SAFE_RELEASE(inp_layout);
}

// TODO: No need to store the whole bytecode, signature might be enough (?)
void EmuGfxState::SetVShader(ID3D11VertexShader* shader, ID3D10Blob* bcode)
{
	// TODO: vshaderchanged actually just needs to be true if the signature changed
	if (bcode && vsbytecode != bcode->GetBufferPointer()) vshaderchanged = true;
	SAFE_RELEASE(vsbytecode);
	SAFE_RELEASE(vertexshader);

	if (shader && bcode)
	{
		vertexshader = shader;
		shader->AddRef();
		vsbytecode = bcode;
		bcode->AddRef();
	}
	else if (shader || bcode)
	{
		PanicAlert("Invalid parameters!\n");
	}
}

void EmuGfxState::SetPShader(ID3D11PixelShader* shader)
{
	if (pixelshader) pixelshader->Release();
	pixelshader = shader;
	if (shader) shader->AddRef();
}

void EmuGfxState::SetInputElements(const D3D11_INPUT_ELEMENT_DESC* elems, UINT num)
{
	num_inp_elems = num;
	memcpy(inp_elems, elems, num*sizeof(D3D11_INPUT_ELEMENT_DESC));
}

void EmuGfxState::SetShaderResource(int stage, ID3D11ShaderResourceView* srv)
{
	if (shader_resources[stage]) shader_resources[stage]->Release();
	shader_resources[stage] = srv;
	if (srv) srv->AddRef();
}

void EmuGfxState::SetAlphaBlendEnable(bool enable)
{
	blenddesc.RenderTarget[0].BlendEnable = enable;
}

void EmuGfxState::SetRenderTargetWriteMask(UINT8 mask)
{
	blenddesc.RenderTarget[0].RenderTargetWriteMask = mask;
}

void EmuGfxState::ApplyState()
{
	HRESULT hr;

	// input layout (only needs to be updated if the vertex shader signature changed)
	if (vshaderchanged)
	{
		if (inp_layout) inp_layout->Release();
		hr = D3D::device->CreateInputLayout(inp_elems, num_inp_elems, vsbytecode->GetBufferPointer(), vsbytecode->GetBufferSize(), &inp_layout);
		if (FAILED(hr)) PanicAlert("Failed to create input layout, %s %d\n", __FILE__, __LINE__);
		SetDebugObjectName((ID3D11DeviceChild*)inp_layout, "an input layout of EmuGfxState");
		vshaderchanged = false;
	}
	D3D::context->IASetInputLayout(inp_layout);

	// vertex shader
	// TODO: divide the global variables of the generated shaders into about 5 constant buffers
	// TODO: improve interaction between EmuGfxState and global state management, so that we don't need to set the constant buffers every time
	if (!vscbuf)
	{
		unsigned int size = ((sizeof(float)*952)&(~0xffff))+0x10000; // must be a multiple of 16
		D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(size, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		device->CreateBuffer(&cbdesc, NULL, &vscbuf);
		SetDebugObjectName((ID3D11DeviceChild*)vscbuf, "a vertex shader constant buffer of EmuGfxState");
	}
	if (vscbufchanged)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		context->Map(vscbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, vsconstants, sizeof(vsconstants));
		context->Unmap(vscbuf, 0);
	}
	D3D::context->VSSetConstantBuffers(0, 1, &vscbuf);

	// pixel shader
	if (!pscbuf)
	{
		unsigned int size = ((sizeof(float)*116)&(~0xffff))+0x10000; // must be a multiple of 16
		D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(size, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		device->CreateBuffer(&cbdesc, NULL, &pscbuf);
		SetDebugObjectName((ID3D11DeviceChild*)pscbuf, "a pixel shader constant buffer of EmuGfxState");
	}
	if (pscbufchanged)
	{
		D3D11_MAPPED_SUBRESOURCE map;
		context->Map(pscbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		memcpy(map.pData, psconstants, sizeof(psconstants));
		context->Unmap(pscbuf, 0);
		pscbufchanged = false;
	}
	D3D::context->PSSetConstantBuffers(0, 1, &pscbuf);

	ID3D11SamplerState* samplerstate[8];
	for (unsigned int stage = 0; stage < 8; stage++)
	{
		if (shader_resources[stage])
		{
			hr = D3D::device->CreateSamplerState(&samplerdesc[stage], &samplerstate[stage]);
			if (FAILED(hr)) PanicAlert("Fail %s %d, stage=%d\n", __FILE__, __LINE__, stage);
			else SetDebugObjectName((ID3D11DeviceChild*)samplerstate[stage], "a sampler state of EmuGfxState");
		}
		else samplerstate[stage] = NULL;
	}
	D3D::context->PSSetSamplers(0, 8, samplerstate);
	for (unsigned int stage = 0; stage < 8; stage++)
		SAFE_RELEASE(samplerstate[stage]);

	ID3D11BlendState* blstate;
	hr = device->CreateBlendState(&blenddesc, &blstate);
	if (FAILED(hr)) PanicAlert("Failed to create blend state at %s %d\n", __FILE__, __LINE__);
	context->OMSetBlendState(blstate, NULL, 0xFFFFFFFF);
	SetDebugObjectName((ID3D11DeviceChild*)blstate, "a blend state of EmuGfxState");
	blstate->Release();

	rastdesc.FillMode = (g_ActiveConfig.bWireFrame) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
	ID3D11RasterizerState* raststate;
	hr = device->CreateRasterizerState(&rastdesc, &raststate);
	if (FAILED(hr)) PanicAlert("Failed to create rasterizer state at %s %d\n", __FILE__, __LINE__);
	SetDebugObjectName((ID3D11DeviceChild*)raststate, "a rasterizer state of EmuGfxState");
	context->RSSetState(raststate);
	raststate->Release();

	ID3D11DepthStencilState* depth_state;
	hr = device->CreateDepthStencilState(&depthdesc, &depth_state);
	if (SUCCEEDED(hr)) SetDebugObjectName((ID3D11DeviceChild*)depth_state, "a depth-stencil state of EmuGfxState");
	else PanicAlert("Failed to create depth state at %s %d\n", __FILE__, __LINE__);
	context->OMSetDepthStencilState(depth_state, 0);
	depth_state->Release();

	context->PSSetShader(pixelshader, NULL, 0);
	context->VSSetShader(vertexshader, NULL, 0);
	context->PSSetShaderResources(0, 8, shader_resources);
}

void EmuGfxState::AlphaPass()
{
	context->PSSetShader(pixelshader, NULL, 0);

	ID3D11BlendState* blstate;
	D3D11_BLEND_DESC desc = blenddesc;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALPHA;
	desc.RenderTarget[0].BlendEnable = FALSE;
	HRESULT hr = device->CreateBlendState(&desc, &blstate);
	if (FAILED(hr)) PanicAlert("Failed to create blend state at %s %d\n", __FILE__, __LINE__);
	SetDebugObjectName((ID3D11DeviceChild*)blstate, "a blend state of EmuGfxState (created during alpha pass)");
	context->OMSetBlendState(blstate, NULL, 0xFFFFFFFF);
	blstate->Release();
}

void EmuGfxState::ResetShaderResources()
{
	for (unsigned int k = 0;k < 8;k++)
		SAFE_RELEASE(shader_resources[k]);
}

EmuGfxState* gfxstate = NULL;

HRESULT Create(HWND wnd)
{
	hWnd = wnd;
	HRESULT hr;

	RECT client;
	GetClientRect(hWnd, &client);
	xres = client.right - client.left;
	yres = client.bottom - client.top;

	IDXGIFactory* factory;
	IDXGIAdapter* adapter;
	IDXGIOutput* output;
	hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
	if (FAILED(hr)) MessageBox(wnd, _T("Failed to create IDXGIFactory object"), _T("Dolphin Direct3D 11 plugin"), MB_OK | MB_ICONERROR);

	hr = factory->EnumAdapters(g_ActiveConfig.iAdapter, &adapter);
	if (FAILED(hr))
	{
		// try using the first one
		hr = factory->EnumAdapters(0, &adapter);
		if (FAILED(hr)) MessageBox(wnd, _T("Failed to enumerate adapter"), _T("Dolphin Direct3D 11 plugin"), MB_OK | MB_ICONERROR);
	}

	// TODO: Make this configurable
	hr = adapter->EnumOutputs(0, &output);
	if (FAILED(hr))
	{
		// try using the first one
		hr = adapter->EnumOutputs(0, &output);
		if (FAILED(hr)) MessageBox(wnd, _T("Failed to enumerate output"), _T("Dolphin Direct3D 11 plugin"), MB_OK | MB_ICONERROR);
	}

	// this will need to be changed once multisampling gets implemented
	DXGI_SWAP_CHAIN_DESC swap_chain_desc;
	memset(&swap_chain_desc, 0, sizeof(swap_chain_desc));
	swap_chain_desc.BufferCount = 1;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.OutputWindow = wnd;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.Windowed = TRUE;

	DXGI_MODE_DESC mode_desc;
	memset(&mode_desc, 0, sizeof(mode_desc));
	mode_desc.Width = xres;
	mode_desc.Height = yres;
	mode_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	mode_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	hr = output->FindClosestMatchingMode(&mode_desc, &swap_chain_desc.BufferDesc, NULL);
	if (FAILED(hr)) MessageBox(wnd, _T("Failed to find a supported video mode"), _T("Dolphin Direct3D 11 plugin"), MB_OK | MB_ICONERROR);
// TODO: Enable these two lines, they're breaking stuff for SOME reason right now
//	xres = swap_chain_desc.BufferDesc.Width;
//	yres = swap_chain_desc.BufferDesc.Height;

#if defined(_DEBUG) || defined(DEBUGFAST)
	D3D11_CREATE_DEVICE_FLAG device_flags = (D3D11_CREATE_DEVICE_FLAG)(D3D11_CREATE_DEVICE_DEBUG|D3D11_CREATE_DEVICE_SINGLETHREADED);
#else
	D3D11_CREATE_DEVICE_FLAG device_flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#endif
	hr = D3D11CreateDeviceAndSwapChain(adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, device_flags, NULL, 0, D3D11_SDK_VERSION, &swap_chain_desc, &swapchain, &device, &featlevel, &context);
	if (FAILED(hr) || !device || !context || !swapchain)
	{
		MessageBox(wnd, _T("Failed to initialize Direct3D."), _T("Dolphin Direct3D 11 plugin"), MB_OK | MB_ICONERROR);
		SAFE_RELEASE(device);
		SAFE_RELEASE(context);
		SAFE_RELEASE(swapchain);
		return E_FAIL;
	}
	SetDebugObjectName((ID3D11DeviceChild*)context, "device context");
	factory->Release();
	output->Release();
	adapter->Release();

	ID3D11Texture2D* buf;
	hr = swapchain->GetBuffer(0, IID_ID3D11Texture2D, (void**)&buf);
	if (FAILED(hr))
	{
		MessageBox(wnd, _T("Failed to get swapchain buffer"), _T("Dolphin Direct3D 11 plugin"), MB_OK | MB_ICONERROR);
		device->Release();
		context->Release();
		swapchain->Release();
		return E_FAIL;
	}
	backbuf = new D3DTexture2D(buf, D3D11_BIND_RENDER_TARGET);
	buf->Release();
	SetDebugObjectName((ID3D11DeviceChild*)backbuf->GetTex(), "backbuffer texture");
	SetDebugObjectName((ID3D11DeviceChild*)backbuf->GetRTV(), "backbuffer render target view");

	context->OMSetRenderTargets(1, &backbuf->GetRTV(), NULL);

	gfxstate = new EmuGfxState;
	return S_OK;
}

void Close()
{
	// release all bound resources
	context->ClearState();

	if (gfxstate) delete gfxstate;
	SAFE_RELEASE(backbuf);
	SAFE_RELEASE(context);
	SAFE_RELEASE(swapchain);
	ULONG references = device->Release();
	if (references)
	{
		ERROR_LOG(VIDEO, "Unreleased references: %i.", references);
	}
	else
	{
		NOTICE_LOG(VIDEO, "Successfully released all device references!");
	}
	device = NULL;
}

/* just returning the 4_0 ones here */
const char* VertexShaderVersionString() { return "vs_4_0"; }
const char* PixelShaderVersionString() { return "ps_4_0"; }

D3DTexture2D* &GetBackBuffer() { return backbuf; }
unsigned int GetBackBufferWidth() { return xres; }
unsigned int GetBackBufferHeight() { return yres; }

void Reset()
{
	// TODO: Check whether we need to do anything here
}

bool BeginFrame()
{
	if (bFrameInProgress)
	{
		PanicAlert("BeginFrame called although a frame is already in progress");
		return false;
	}
	bFrameInProgress = true;
	return (device != NULL);
}

void EndFrame()
{
	if (!bFrameInProgress)
	{
		PanicAlert("EndFrame called although no frame is in progress");
		return;
	}
	bFrameInProgress = false;
}

void Present()
{
	// TODO: Is 1 the correct value for vsyncing?
	swapchain->Present((UINT)g_ActiveConfig.bVSync, 0);
}

}  // namespace
