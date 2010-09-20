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

#include <stack>

// VideoCommon
#include "VertexShaderGen.h"
#include "PixelShaderGen.h"

// DX11
#include "DX11_D3DBase.h"
#include "DX11_D3DBlob.h"

namespace DX11
{

namespace D3D
{

// stores the pipeline state to use when calling VertexManager::Flush()
class EmuGfxState
{
public:
	EmuGfxState();
	~EmuGfxState();

	void SetVShader(ID3D11VertexShader* shader, D3DBlob* bcode);
	void SetPShader(ID3D11PixelShader* shader);
	void SetInputElements(const D3D11_INPUT_ELEMENT_DESC* elems, UINT num);
	void SetShaderResource(unsigned int stage, ID3D11ShaderResourceView* srv);

	void ApplyState();            // apply current state
	void AlphaPass();             // only modify the current state to enable the alpha pass
	void Reset();

	// blend state
	void SetAlphaBlendEnable(bool enable);
	void SetRenderTargetWriteMask(UINT8 mask);
	void SetSrcBlend(D3D11_BLEND val);
	void SetDestBlend(D3D11_BLEND val);
	void SetBlendOp(D3D11_BLEND_OP val);

	// sampler states
	void SetSamplerFilter(DWORD stage, D3D11_FILTER filter);

	// TODO: add methods for changing the other states instead of modifying them directly

	D3D11_SAMPLER_DESC samplerdesc[8];
	D3D11_RASTERIZER_DESC rastdesc;
	D3D11_DEPTH_STENCIL_DESC depthdesc;

	float psconstants[C_PENVCONST_END*4];
	float vsconstants[C_VENVCONST_END*4];
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

	bool apply_called;
};

template<typename T> class AutoState
{
public:
	AutoState(const T* object);
	AutoState(const AutoState<T> &source);
	~AutoState();

	const inline T* GetPtr() const { return state; }

private:
	const T* state;
};

typedef AutoState<ID3D11BlendState> AutoBlendState;
typedef AutoState<ID3D11DepthStencilState> AutoDepthStencilState;
typedef AutoState<ID3D11RasterizerState> AutoRasterizerState;

class StateManager
{
public:
	StateManager();

	// call any of these to change the affected states
	void PushBlendState(const ID3D11BlendState* state);
	void PushDepthState(const ID3D11DepthStencilState* state);
	void PushRasterizerState(const ID3D11RasterizerState* state);

	// call these after drawing
	void PopBlendState();
	void PopDepthState();
	void PopRasterizerState();

	// call this before any drawing operation if states could have changed meanwhile
	void Apply();

private:
	std::stack<AutoBlendState> blendstates;
	std::stack<AutoDepthStencilState> depthstates;
	std::stack<AutoRasterizerState> raststates;
	ID3D11BlendState* cur_blendstate;
	ID3D11DepthStencilState* cur_depthstate;
	ID3D11RasterizerState* cur_raststate;
};

extern EmuGfxState* gfxstate;
extern StateManager* stateman;

}  // namespace

}
