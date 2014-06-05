// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Logging/Log.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/GfxState.h"

namespace DX11
{

namespace D3D
{

StateManager* stateman;


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
	if (state) ((T*)state)->Release();
	state = nullptr;
}

StateManager::StateManager() : cur_blendstate(nullptr), cur_depthstate(nullptr), cur_raststate(nullptr) {}

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
			D3D::context->OMSetBlendState(cur_blendstate, nullptr, 0xFFFFFFFF);
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

}  // namespace DX11
