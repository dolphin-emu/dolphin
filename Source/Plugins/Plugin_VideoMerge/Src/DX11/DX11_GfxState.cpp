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

#include "VideoConfig.h"
#include "DX11_GfxState.h"

namespace DX11
{

namespace D3D
{

EmuGfxState* gfxstate;
StateManager* stateman;

EmuGfxState::EmuGfxState() : vertexshader(NULL), vsbytecode(NULL), pixelshader(NULL), psbytecode(NULL), apply_called(false)
{
	for (unsigned int k = 0;k < 8;k++)
	{
		float border[4] = {0.f, 0.f, 0.f, 0.f};
		shader_resources[k] = NULL;
		samplerdesc[k] = CD3D11_SAMPLER_DESC(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, D3D11_TEXTURE_ADDRESS_CLAMP, 0.f, 16, D3D11_COMPARISON_ALWAYS, border, -D3D11_FLOAT32_MAX, D3D11_FLOAT32_MAX);
		if(g_ActiveConfig.iMaxAnisotropy > 1) samplerdesc[k].Filter = D3D11_FILTER_ANISOTROPIC;
	}

	memset(&blenddesc, 0, sizeof(blenddesc));
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

	memset(&depthdesc, 0, sizeof(depthdesc));
	depthdesc.DepthEnable        = TRUE;
	depthdesc.DepthWriteMask     = D3D11_DEPTH_WRITE_MASK_ALL;
	depthdesc.DepthFunc          = D3D11_COMPARISON_LESS;
	depthdesc.StencilEnable      = FALSE;
	depthdesc.StencilReadMask    = D3D11_DEFAULT_STENCIL_READ_MASK;
	depthdesc.StencilWriteMask   = D3D11_DEFAULT_STENCIL_WRITE_MASK;

	// this probably must be changed once multisampling support gets added
	rastdesc = CD3D11_RASTERIZER_DESC(D3D11_FILL_SOLID, D3D11_CULL_NONE, false, 0, 0.f, 0, false, true, false, false);

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
void EmuGfxState::SetVShader(ID3D11VertexShader* shader, D3DBlob* bcode)
{
	// TODO: vshaderchanged actually just needs to be true if the signature changed
	if (bcode && vsbytecode != bcode) vshaderchanged = true;
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

void EmuGfxState::SetShaderResource(unsigned int stage, ID3D11ShaderResourceView* srv)
{
	if (shader_resources[stage])
		shader_resources[stage]->Release();
	shader_resources[stage] = srv;
	if (srv)
		srv->AddRef();
}

void EmuGfxState::ApplyState()
{
	HRESULT hr;

	// input layout (only needs to be updated if the vertex shader signature changed)
	if (vshaderchanged)
	{
		SAFE_RELEASE(inp_layout);
		hr = D3D::device->CreateInputLayout(inp_elems, num_inp_elems, vsbytecode->Data(), vsbytecode->Size(), &inp_layout);
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
		unsigned int size = ((sizeof(vsconstants))&(~0xf))+0x10; // must be a multiple of 16
		D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(size, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		hr = device->CreateBuffer(&cbdesc, NULL, &vscbuf);
		CHECK(hr==S_OK, "Create vertex shader constant buffer (size=%u)", size);
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
		unsigned int size = ((sizeof(psconstants))&(~0xf))+0x10; // must be a multiple of 16
		D3D11_BUFFER_DESC cbdesc = CD3D11_BUFFER_DESC(size, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
		device->CreateBuffer(&cbdesc, NULL, &pscbuf);
		CHECK(hr==S_OK, "Create pixel shader constant buffer (size=%u)", size);
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
			if(g_ActiveConfig.iMaxAnisotropy > 1) samplerdesc[stage].Filter = D3D11_FILTER_ANISOTROPIC;
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
	stateman->PushBlendState(blstate);
	SetDebugObjectName((ID3D11DeviceChild*)blstate, "a blend state of EmuGfxState");
	SAFE_RELEASE(blstate);

	rastdesc.FillMode = (g_ActiveConfig.bWireFrame) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
	ID3D11RasterizerState* raststate;
	hr = device->CreateRasterizerState(&rastdesc, &raststate);
	if (FAILED(hr)) PanicAlert("Failed to create rasterizer state at %s %d\n", __FILE__, __LINE__);
	SetDebugObjectName((ID3D11DeviceChild*)raststate, "a rasterizer state of EmuGfxState");
	stateman->PushRasterizerState(raststate);
	SAFE_RELEASE(raststate);

	ID3D11DepthStencilState* depth_state;
	hr = device->CreateDepthStencilState(&depthdesc, &depth_state);
	if (SUCCEEDED(hr)) SetDebugObjectName((ID3D11DeviceChild*)depth_state, "a depth-stencil state of EmuGfxState");
	else PanicAlert("Failed to create depth state at %s %d\n", __FILE__, __LINE__);
	D3D::stateman->PushDepthState(depth_state);
	SAFE_RELEASE(depth_state);

	context->PSSetShader(pixelshader, NULL, 0);
	context->VSSetShader(vertexshader, NULL, 0);
	context->PSSetShaderResources(0, 8, shader_resources);

	stateman->Apply();
	apply_called = true;
}

void EmuGfxState::AlphaPass()
{
	if (!apply_called) ERROR_LOG(VIDEO, "EmuGfxState::AlphaPass called without having called ApplyState before!")
	else stateman->PopBlendState();

	// pixel shader for alpha pass is different, so update it
	context->PSSetShader(pixelshader, NULL, 0);

	ID3D11BlendState* blstate;
	D3D11_BLEND_DESC desc = blenddesc;
	desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALPHA;
	desc.RenderTarget[0].BlendEnable = FALSE;
	HRESULT hr = device->CreateBlendState(&desc, &blstate);
	if (FAILED(hr)) PanicAlert("Failed to create blend state at %s %d\n", __FILE__, __LINE__);
	SetDebugObjectName((ID3D11DeviceChild*)blstate, "a blend state of EmuGfxState (created during alpha pass)");
	stateman->PushBlendState(blstate);
	blstate->Release();

	stateman->Apply();
}

void EmuGfxState::Reset()
{
	for (unsigned int k = 0;k < 8;k++)
		SAFE_RELEASE(shader_resources[k]);

	context->PSSetShaderResources(0, 8, shader_resources); // unbind all textures
	if (apply_called)
	{
		stateman->PopBlendState();
		stateman->PopDepthState();
		stateman->PopRasterizerState();
		apply_called = false;
	}
}

void EmuGfxState::SetAlphaBlendEnable(bool enable)
{
	blenddesc.RenderTarget[0].BlendEnable = enable;
}

void EmuGfxState::SetRenderTargetWriteMask(UINT8 mask)
{
	blenddesc.RenderTarget[0].RenderTargetWriteMask = mask;
}

void EmuGfxState::SetSrcBlend(D3D11_BLEND val)
{
	// TODO: Check whether e.g. the dest color check is needed here
	blenddesc.RenderTarget[0].SrcBlend = val;
	if (val == D3D11_BLEND_SRC_COLOR) blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	else if (val == D3D11_BLEND_INV_SRC_COLOR) blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	else if (val == D3D11_BLEND_DEST_COLOR) blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
	else if (val == D3D11_BLEND_INV_DEST_COLOR) blenddesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
	else blenddesc.RenderTarget[0].SrcBlendAlpha = val;
}

void EmuGfxState::SetDestBlend(D3D11_BLEND val)
{
	// TODO: Check whether e.g. the source color check is needed here
	blenddesc.RenderTarget[0].DestBlend = val;
	if (val == D3D11_BLEND_SRC_COLOR) blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	else if (val == D3D11_BLEND_INV_SRC_COLOR) blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	else if (val == D3D11_BLEND_DEST_COLOR) blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
	else if (val == D3D11_BLEND_INV_DEST_COLOR) blenddesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
	else blenddesc.RenderTarget[0].DestBlendAlpha = val;
}

void EmuGfxState::SetBlendOp(D3D11_BLEND_OP val)
{
	blenddesc.RenderTarget[0].BlendOp = val;
	blenddesc.RenderTarget[0].BlendOpAlpha = val;
}

void EmuGfxState::SetSamplerFilter(DWORD stage, D3D11_FILTER filter)
{
	samplerdesc[stage].Filter = filter;
}

template<typename T> AutoState<T>::AutoState(const T* object) : state(object)
{
	((IUnknown*)state)->AddRef();
}

template<typename T> AutoState<T>::AutoState(const AutoState<T> &source)
{
	state = source.GetPtr();
	((T*)state)->AddRef();
}

template<typename T> AutoState<T>::~AutoState()
{
	if(state) ((T*)state)->Release();
	state = NULL;
}

StateManager::StateManager() : cur_blendstate(NULL), cur_depthstate(NULL), cur_raststate(NULL) {}

void StateManager::PushBlendState(const ID3D11BlendState* state) { blendstates.push(AutoBlendState(state)); }
void StateManager::PushDepthState(const ID3D11DepthStencilState* state) { depthstates.push(AutoDepthStencilState(state)); }
void StateManager::PushRasterizerState(const ID3D11RasterizerState* state) { raststates.push(AutoRasterizerState(state)); }
void StateManager::PopBlendState() { blendstates.pop(); }
void StateManager::PopDepthState() { depthstates.pop(); }
void StateManager::PopRasterizerState() { raststates.pop(); }

void StateManager::Apply()
{
	if (!blendstates.empty())
	{
		if (cur_blendstate != blendstates.top().GetPtr())
		{
			cur_blendstate = (ID3D11BlendState*)blendstates.top().GetPtr();
			D3D::context->OMSetBlendState(cur_blendstate, NULL, 0xFFFFFFFF);
		}
	}
	else ERROR_LOG(VIDEO, "Tried to apply without blend state!");

	if (!depthstates.empty())
	{
		if (cur_depthstate != depthstates.top().GetPtr())
		{
			cur_depthstate = (ID3D11DepthStencilState*)depthstates.top().GetPtr();
			D3D::context->OMSetDepthStencilState(cur_depthstate, 0);
		}
	}
	else ERROR_LOG(VIDEO, "Tried to apply without depth state!");

	if (!raststates.empty())
	{
		if (cur_raststate != raststates.top().GetPtr())
		{
			cur_raststate = (ID3D11RasterizerState*)raststates.top().GetPtr();
			D3D::context->RSSetState(cur_raststate);
		}
	}
	else ERROR_LOG(VIDEO, "Tried to apply without rasterizer state!");
}

}  // namespace

}
