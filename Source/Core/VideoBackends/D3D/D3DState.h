// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <stack>
#include <unordered_map>

#include "Common/BitField.h"
#include "Common/CommonTypes.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoCommon/BPMemory.h"

struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;

namespace DX11
{

union RasterizerState
{
	BitField<0, 2, D3D11_CULL_MODE> cull_mode;
	BitField<2, 1, u32> wireframe;

	u32 packed;
};

union BlendState
{
	BitField<0, 1, u32> blend_enable;
	BitField<1, 3, D3D11_BLEND_OP> blend_op;
	BitField<4, 4, u32> write_mask;
	BitField<8, 5, D3D11_BLEND> src_blend;
	BitField<13, 5, D3D11_BLEND> dst_blend;
	BitField<18, 1, u32> use_dst_alpha;

	u32 packed;
};

union SamplerState
{
	BitField<0, 3, u64> min_filter;
	BitField<3, 1, u64> mag_filter;
	BitField<4, 8, u64> min_lod;
	BitField<12, 8, u64> max_lod;
	BitField<20, 8, s64> lod_bias;
	BitField<28, 2, u64> wrap_s;
	BitField<30, 2, u64> wrap_t;
	BitField<32, 5, u64> max_anisotropy;

	u64 packed;
};

class StateCache
{
public:

	// Get existing or create new render state.
	// Returned objects is owned by the cache and does not need to be released.
	ID3D11SamplerState* Get(SamplerState state);
	ID3D11BlendState* Get(BlendState state);
	ID3D11RasterizerState* Get(RasterizerState state);
	ID3D11DepthStencilState* Get(ZMode state);

	// Release all cached states and clear hash tables.
	void Clear();

private:

	std::unordered_map<u32, ID3D11DepthStencilState*> m_depth;
	std::unordered_map<u32, ID3D11RasterizerState*> m_raster;
	std::unordered_map<u32, ID3D11BlendState*> m_blend;
	std::unordered_map<u64, ID3D11SamplerState*> m_sampler;

};

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