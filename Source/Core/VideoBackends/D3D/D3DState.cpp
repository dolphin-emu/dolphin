// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Logging/Log.h"

#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"

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

}  // namespace D3D

ID3D11SamplerState* StateCache::Get(SamplerState state)
{
	auto it = m_sampler.find(state.packed);

	if (it != m_sampler.end())
	{
		return it->second;
	}

	const unsigned int d3dMipFilters[4] =
	{
		TexMode0::TEXF_NONE,
		TexMode0::TEXF_POINT,
		TexMode0::TEXF_LINEAR,
		TexMode0::TEXF_NONE, //reserved
	};
	const D3D11_TEXTURE_ADDRESS_MODE d3dClamps[4] =
	{
		D3D11_TEXTURE_ADDRESS_CLAMP,
		D3D11_TEXTURE_ADDRESS_WRAP,
		D3D11_TEXTURE_ADDRESS_MIRROR,
		D3D11_TEXTURE_ADDRESS_WRAP //reserved
	};

	D3D11_SAMPLER_DESC sampdc = CD3D11_SAMPLER_DESC(CD3D11_DEFAULT());

	unsigned int mip = d3dMipFilters[state.min_filter & 3];

	if (state.max_anisotropy)
	{
		sampdc.Filter = D3D11_FILTER_ANISOTROPIC;
		sampdc.MaxAnisotropy = (u32)state.max_anisotropy;
	}
	else if (state.min_filter & 4) // linear min filter
	{
		if (state.mag_filter) // linear mag filter
		{
			if (mip == TexMode0::TEXF_NONE)
				sampdc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			else if (mip == TexMode0::TEXF_POINT)
				sampdc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			else if (mip == TexMode0::TEXF_LINEAR)
				sampdc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		}
		else // point mag filter
		{
			if (mip == TexMode0::TEXF_NONE)
				sampdc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
			else if (mip == TexMode0::TEXF_POINT)
				sampdc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
			else if (mip == TexMode0::TEXF_LINEAR)
				sampdc.Filter = D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
		}
	}
	else // point min filter
	{
		if (state.mag_filter) // linear mag filter
		{
			if (mip == TexMode0::TEXF_NONE)
				sampdc.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
			else if (mip == TexMode0::TEXF_POINT)
				sampdc.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
			else if (mip == TexMode0::TEXF_LINEAR)
				sampdc.Filter = D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
		}
		else // point mag filter
		{
			if (mip == TexMode0::TEXF_NONE)
				sampdc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			else if (mip == TexMode0::TEXF_POINT)
				sampdc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			else if (mip == TexMode0::TEXF_LINEAR)
				sampdc.Filter = D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
		}
	}

	sampdc.AddressU = d3dClamps[state.wrap_s];
	sampdc.AddressV = d3dClamps[state.wrap_t];

	sampdc.MaxLOD = (mip == TexMode0::TEXF_NONE) ? 0.0f : (float)state.max_lod / 16.f;
	sampdc.MinLOD = (float)state.min_lod / 16.f;
	sampdc.MipLODBias = (s32)state.lod_bias / 32.0f;

	ID3D11SamplerState* res = nullptr;

	HRESULT hr = D3D::device->CreateSamplerState(&sampdc, &res);
	if (FAILED(hr)) PanicAlert("Fail %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)res, "sampler state used to emulate the GX pipeline");
	m_sampler.insert(std::make_pair(state.packed, res));

	return res;
}

ID3D11BlendState* StateCache::Get(BlendState state)
{
	if (!state.blend_enable)
	{
		state.src_blend = D3D11_BLEND_ONE;
		state.dst_blend = D3D11_BLEND_ZERO;
		state.blend_op = D3D11_BLEND_OP_ADD;
		state.use_dst_alpha = false;
	}

	auto it = m_blend.find(state.packed);

	if (it != m_blend.end())
	{
		return it->second;
	}

	D3D11_BLEND_DESC blenddc = CD3D11_BLEND_DESC(CD3D11_DEFAULT());

	blenddc.AlphaToCoverageEnable = FALSE;
	blenddc.IndependentBlendEnable = FALSE;
	blenddc.RenderTarget[0].BlendEnable = state.blend_enable;
	blenddc.RenderTarget[0].RenderTargetWriteMask = (u32)state.write_mask;
	blenddc.RenderTarget[0].SrcBlend = state.src_blend;
	blenddc.RenderTarget[0].DestBlend = state.dst_blend;
	blenddc.RenderTarget[0].BlendOp = state.blend_op;
	blenddc.RenderTarget[0].SrcBlendAlpha = state.src_blend;
	blenddc.RenderTarget[0].DestBlendAlpha = state.dst_blend;
	blenddc.RenderTarget[0].BlendOpAlpha = state.blend_op;

	if (blenddc.RenderTarget[0].SrcBlend == D3D11_BLEND_SRC_COLOR)
		blenddc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	else if (blenddc.RenderTarget[0].SrcBlend == D3D11_BLEND_INV_SRC_COLOR)
		blenddc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	else if (blenddc.RenderTarget[0].SrcBlend == D3D11_BLEND_DEST_COLOR)
		blenddc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_DEST_ALPHA;
	else if (blenddc.RenderTarget[0].SrcBlend == D3D11_BLEND_INV_DEST_COLOR)
		blenddc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
	else
		blenddc.RenderTarget[0].SrcBlendAlpha = blenddc.RenderTarget[0].SrcBlend;

	if (blenddc.RenderTarget[0].DestBlend == D3D11_BLEND_SRC_COLOR)
		blenddc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_SRC_ALPHA;
	else if (blenddc.RenderTarget[0].DestBlend == D3D11_BLEND_INV_SRC_COLOR)
		blenddc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	else if (blenddc.RenderTarget[0].DestBlend == D3D11_BLEND_DEST_COLOR)
		blenddc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
	else if (blenddc.RenderTarget[0].DestBlend == D3D11_BLEND_INV_DEST_COLOR)
		blenddc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_DEST_ALPHA;
	else
		blenddc.RenderTarget[0].DestBlendAlpha = blenddc.RenderTarget[0].DestBlend;

	if (state.use_dst_alpha)
	{
		// Colors should blend against SRC1_ALPHA
		if (blenddc.RenderTarget[0].SrcBlend == D3D11_BLEND_SRC_ALPHA)
			blenddc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC1_ALPHA;
		else if (blenddc.RenderTarget[0].SrcBlend == D3D11_BLEND_INV_SRC_ALPHA)
			blenddc.RenderTarget[0].SrcBlend = D3D11_BLEND_INV_SRC1_ALPHA;

		// Colors should blend against SRC1_ALPHA
		if (blenddc.RenderTarget[0].DestBlend == D3D11_BLEND_SRC_ALPHA)
			blenddc.RenderTarget[0].DestBlend = D3D11_BLEND_SRC1_ALPHA;
		else if (blenddc.RenderTarget[0].DestBlend == D3D11_BLEND_INV_SRC_ALPHA)
			blenddc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC1_ALPHA;

		blenddc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blenddc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blenddc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	}

	ID3D11BlendState* res = nullptr;

	HRESULT hr = D3D::device->CreateBlendState(&blenddc, &res);
	if (FAILED(hr)) PanicAlert("Failed to create blend state at %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)res, "blend state used to emulate the GX pipeline");
	m_blend.insert(std::make_pair(state.packed, res));

	return res;
}

ID3D11RasterizerState* StateCache::Get(RasterizerState state)
{
	auto it = m_raster.find(state.packed);

	if (it != m_raster.end())
	{
		return it->second;
	}

	D3D11_RASTERIZER_DESC rastdc = CD3D11_RASTERIZER_DESC(state.wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID,
		state.cull_mode,
		false, 0, 0.f, 0, true, true, false, false);

	ID3D11RasterizerState* res = nullptr;

	HRESULT hr = D3D::device->CreateRasterizerState(&rastdc, &res);
	if (FAILED(hr)) PanicAlert("Failed to create rasterizer state at %s %d\n", __FILE__, __LINE__);
	D3D::SetDebugObjectName((ID3D11DeviceChild*)res, "rasterizer state used to emulate the GX pipeline");
	m_raster.insert(std::make_pair(state.packed, res));

	return res;
}

ID3D11DepthStencilState* StateCache::Get(ZMode state)
{
	auto it = m_depth.find(state.hex);

	if (it != m_depth.end())
	{
		return it->second;
	}

	D3D11_DEPTH_STENCIL_DESC depthdc = CD3D11_DEPTH_STENCIL_DESC(CD3D11_DEFAULT());

	depthdc.DepthEnable = TRUE;
	depthdc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthdc.DepthFunc = D3D11_COMPARISON_LESS;
	depthdc.StencilEnable = FALSE;
	depthdc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	depthdc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

	const D3D11_COMPARISON_FUNC d3dCmpFuncs[8] =
	{
		D3D11_COMPARISON_NEVER,
		D3D11_COMPARISON_LESS,
		D3D11_COMPARISON_EQUAL,
		D3D11_COMPARISON_LESS_EQUAL,
		D3D11_COMPARISON_GREATER,
		D3D11_COMPARISON_NOT_EQUAL,
		D3D11_COMPARISON_GREATER_EQUAL,
		D3D11_COMPARISON_ALWAYS
	};

	if (state.testenable)
	{
		depthdc.DepthEnable = TRUE;
		depthdc.DepthWriteMask = state.updateenable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		depthdc.DepthFunc = d3dCmpFuncs[state.func];
	}
	else
	{
		// if the test is disabled write is disabled too
		depthdc.DepthEnable = FALSE;
		depthdc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	}

	ID3D11DepthStencilState* res = nullptr;

	HRESULT hr = D3D::device->CreateDepthStencilState(&depthdc, &res);
	if (SUCCEEDED(hr)) D3D::SetDebugObjectName((ID3D11DeviceChild*)res, "depth-stencil state used to emulate the GX pipeline");
	else PanicAlert("Failed to create depth state at %s %d\n", __FILE__, __LINE__);

	m_depth.insert(std::make_pair(state.hex, res));

	return res;
}

void StateCache::Clear()
{
	for (auto it : m_depth)
	{
		SAFE_RELEASE(it.second);
	}
	m_depth.clear();

	for (auto it : m_raster)
	{
		SAFE_RELEASE(it.second);
	}
	m_raster.clear();

	for (auto it : m_blend)
	{
		SAFE_RELEASE(it.second);
	}
	m_blend.clear();

	for (auto it : m_sampler)
	{
		SAFE_RELEASE(it.second);
	}
	m_sampler.clear();
}

}  // namespace DX11
