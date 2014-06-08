// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>

#include "Common/CommonTypes.h"
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoCommon/BPMemory.h"

namespace DX11
{

union RasterizerState
{
	struct
	{
		u32 cull_mode : 2;
		u32 wireframe : 1;
	};

	u32 packed;
};

union BlendState
{
	struct
	{
		u32 blend_enable : 1;
		u32 blend_op : 3;
		u32 write_mask : 4;
		u32 src_blend : 5;
		u32 dst_blend : 5;
		u32 use_dst_alpha : 1;
	};

	u32 packed;
};

union SamplerState
{
	struct
	{
		u64 min_filter : 3;
		u64 mag_filter : 1;
		u64 min_lod : 8;
		u64 max_lod : 8;
		s64 lod_bias : 8;
		u64 wrap_s : 2;
		u64 wrap_t : 2;
		u64 max_anisotropy : 5;
	};

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

}
