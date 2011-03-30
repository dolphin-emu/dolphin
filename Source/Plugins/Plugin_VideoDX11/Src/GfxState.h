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

#include "D3DUtil.h"

namespace DX11
{

namespace D3D
{

typedef SharedPtr<ID3D11BlendState> AutoBlendState;
typedef SharedPtr<ID3D11DepthStencilState> AutoDepthStencilState;
typedef SharedPtr<ID3D11RasterizerState> AutoRasterizerState;

class StateManager
{
public:
	// call any of these to change the affected states
	void PushBlendState(const AutoBlendState& state)
	{
		blendstates.push(state);
	}

	void PushDepthState(ID3D11DepthStencilState* state)
	{
		state->AddRef();
		depthstates.push(AutoDepthStencilState::FromPtr(state));
	}

	void PushRasterizerState(ID3D11RasterizerState* state)
	{
		state->AddRef();
		raststates.push(AutoRasterizerState::FromPtr(state));
	}

	// call these after drawing
	void PopBlendState() { blendstates.pop(); }
	void PopDepthState() { depthstates.pop(); }
	void PopRasterizerState() { raststates.pop(); }

	// call this before any drawing operation if states could have changed meanwhile
	void Apply();

private:
	std::stack<AutoBlendState> blendstates;
	std::stack<AutoDepthStencilState> depthstates;
	std::stack<AutoRasterizerState> raststates;
};

extern std::unique_ptr<StateManager> stateman;

}  // namespace

}  // namespace DX11