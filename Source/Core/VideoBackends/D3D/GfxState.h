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
	template < typename T >
	using StackWithVector = std::stack<T,std::vector<T>>;

	StackWithVector<ID3D11BlendState const *> blendstates;
	StackWithVector<ID3D11DepthStencilState const *> depthstates;
	StackWithVector<ID3D11RasterizerState const *> raststates;
	ID3D11BlendState* cur_blendstate;
	ID3D11DepthStencilState* cur_depthstate;
	ID3D11RasterizerState* cur_raststate;
};

extern StateManager* stateman;

}  // namespace

}  // namespace DX11