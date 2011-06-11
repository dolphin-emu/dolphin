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