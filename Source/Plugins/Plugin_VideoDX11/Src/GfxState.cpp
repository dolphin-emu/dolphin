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
#include "GfxState.h"
#include "VertexShaderCache.h"

namespace D3D
{

EmuGfxState* gfxstate;
StateManager* stateman;

EmuGfxState::EmuGfxState() : apply_called(false)
{
	pscbuf = NULL;
	vscbuf = NULL;

	pscbufchanged = false;
	vscbufchanged = false;
}

EmuGfxState::~EmuGfxState()
{
	SAFE_RELEASE(pscbuf);
	SAFE_RELEASE(vscbuf);
}

void EmuGfxState::ApplyState()
{
	HRESULT hr;

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
		vscbufchanged = false;
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

	apply_called = true;
}

void EmuGfxState::Reset()
{
	if (apply_called)
	{
		apply_called = false;
	}
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
