// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <stack>

struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;

namespace DX11
{

namespace D3D
{

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

extern StateManager* stateman;

}  // namespace

}  // namespace DX11