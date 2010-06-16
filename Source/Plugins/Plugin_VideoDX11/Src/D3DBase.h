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

#define SAFE_RELEASE(x) { if (x) (x)->Release(); (x) = NULL; }

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

// stores the pipeline state to use when calling VertexManager::Flush()
class EmuGfxState
{
public:
	EmuGfxState();
	~EmuGfxState();

	void SetVShader(ID3D11VertexShader* shader, D3DBlob* bcode);
	void SetPShader(ID3D11PixelShader* shader);
	void SetInputElements(const D3D11_INPUT_ELEMENT_DESC* elems, UINT num);
	void SetShaderResource(int stage, ID3D11ShaderResourceView* srv);

	void ApplyState();            // apply current state
	void AlphaPass();             // only modify the current state to enable the alpha pass
	void ResetShaderResources();  // disable all shader resources

	// blend state
	void SetAlphaBlendEnable(bool enable);
	void SetRenderTargetWriteMask(UINT8 mask);
	void SetSrcBlend(D3D11_BLEND val) // TODO: Check whether e.g. the dest color check is needed here
	{
		blenddesc.RenderTarget[0].SrcBlend = val;
		if (val == D3D11_BLEND_SRC_COLOR) blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		else if (val == D3D11_BLEND_INV_SRC_COLOR) blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		else if (val == D3D11_BLEND_DEST_COLOR) blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
		else if (val == D3D11_BLEND_INV_DEST_COLOR) blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
		else blenddesc.RenderTarget[0].SrcBlendAlpha = val;
	}
	void SetDestBlend(D3D11_BLEND val)
	{
		blenddesc.RenderTarget[0].DestBlend = val;
		if (val == D3D11_BLEND_SRC_COLOR) blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		else if (val == D3D11_BLEND_INV_SRC_COLOR) blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		else if (val == D3D11_BLEND_DEST_COLOR) blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
		else if (val == D3D11_BLEND_INV_DEST_COLOR) blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
		else blenddesc.RenderTarget[0].DestBlendAlpha = val;
	}
	void SetBlendOp(D3D11_BLEND_OP val)
	{
		blenddesc.RenderTarget[0].BlendOp = val;
		blenddesc.RenderTarget[0].BlendOpAlpha = val;
	}

	// sampler states
	void SetSamplerFilter(DWORD stage, D3D11_FILTER filter) { samplerdesc[stage].Filter = filter; }

	// TODO: add methods for changing the other states instead of modifying them directly

	D3D11_SAMPLER_DESC samplerdesc[8];
	D3D11_RASTERIZER_DESC rastdesc;
	D3D11_DEPTH_STENCIL_DESC depthdesc;

	float psconstants[116];
	float vsconstants[952];
	bool vscbufchanged;
	bool pscbufchanged;

private:
	ID3D11VertexShader* vertexshader;
	D3DBlob* vsbytecode;
	ID3D11PixelShader* pixelshader;
	D3DBlob* psbytecode;
	bool vshaderchanged;

	ID3D11Buffer* vscbuf;
	ID3D11Buffer* pscbuf;

	ID3D11InputLayout* inp_layout;
	D3D11_INPUT_ELEMENT_DESC inp_elems[32];
	int num_inp_elems;

	ID3D11ShaderResourceView* shader_resources[8];
	D3D11_BLEND_DESC blenddesc;
};

extern EmuGfxState* gfxstate;

}  // namespace
