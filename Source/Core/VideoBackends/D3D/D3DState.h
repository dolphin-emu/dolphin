// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdint>
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

	void SetTexture(u32 index, ID3D11ShaderResourceView* texture)
	{
		if (m_current.textures[index] != texture)
			m_dirtyFlags |= DirtyFlag_Texture0 << index;

		m_pending.textures[index] = texture;
	}

	void SetSampler(u32 index, ID3D11SamplerState* sampler)
	{
		if (m_current.samplers[index] != sampler)
			m_dirtyFlags |= DirtyFlag_Sampler0 << index;

		m_pending.samplers[index] = sampler;
	}

	void SetPixelConstants(ID3D11Buffer* buffer0, ID3D11Buffer* buffer1 = nullptr)
	{
		if (m_current.pixelConstants[0] != buffer0 || m_current.pixelConstants[1] != buffer1)
			m_dirtyFlags |= DirtyFlag_PixelConstants;

		m_pending.pixelConstants[0] = buffer0;
		m_pending.pixelConstants[1] = buffer1;
	}

	void SetVertexConstants(ID3D11Buffer* buffer)
	{
		if (m_current.vertexConstants != buffer)
			m_dirtyFlags |= DirtyFlag_VertexConstants;

		m_pending.vertexConstants = buffer;
	}

	void SetGeometryConstants(ID3D11Buffer* buffer)
	{
		if (m_current.geometryConstants != buffer)
			m_dirtyFlags |= DirtyFlag_GeometryConstants;

		m_pending.geometryConstants = buffer;
	}

	void SetVertexBuffer(ID3D11Buffer* buffer, u32 stride, u32 offset)
	{
		if (m_current.vertexBuffer != buffer ||
			m_current.vertexBufferStride != stride ||
			m_current.vertexBufferOffset != offset)
			m_dirtyFlags |= DirtyFlag_VertexBuffer;

		m_pending.vertexBuffer = buffer;
		m_pending.vertexBufferStride = stride;
		m_pending.vertexBufferOffset = offset;
	}

	void SetIndexBuffer(ID3D11Buffer* buffer)
	{
		if (m_current.indexBuffer != buffer)
			m_dirtyFlags |= DirtyFlag_IndexBuffer;

		m_pending.indexBuffer = buffer;
	}

	void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology)
	{
		if (m_current.topology != topology)
			m_dirtyFlags |= DirtyFlag_InputAssembler;

		m_pending.topology = topology;
	}

	void SetInputLayout(ID3D11InputLayout* layout)
	{
		if (m_current.inputLayout != layout)
			m_dirtyFlags |= DirtyFlag_InputAssembler;

		m_pending.inputLayout = layout;
	}

	void SetPixelShader(ID3D11PixelShader* shader)
	{
		if (m_current.pixelShader != shader)
			m_dirtyFlags |= DirtyFlag_PixelShader;

		m_pending.pixelShader = shader;
	}

	void SetPixelShaderDynamic(ID3D11PixelShader* shader, ID3D11ClassInstance * const * classInstances, u32 classInstancesCount)
	{
		D3D::context->PSSetShader(shader, classInstances, classInstancesCount);
		m_current.pixelShader = shader;
		m_pending.pixelShader = shader;
	}

	void SetVertexShader(ID3D11VertexShader* shader)
	{
		if (m_current.vertexShader != shader)
			m_dirtyFlags |= DirtyFlag_VertexShader;

		m_pending.vertexShader = shader;
	}

	void SetGeometryShader(ID3D11GeometryShader* shader)
	{
		if (m_current.geometryShader != shader)
			m_dirtyFlags |= DirtyFlag_GeometryShader;

		m_pending.geometryShader = shader;
	}

	// removes currently set texture from all slots, returns mask of previously bound slots
	u32 UnsetTexture(ID3D11ShaderResourceView* srv);
	void SetTextureByMask(u64 textureSlotMask, ID3D11ShaderResourceView* srv);

	// call this immediately before any drawing operation or to explicitly apply pending resource state changes
	void Apply();

private:

	std::stack<AutoBlendState> m_blendStates;
	std::stack<AutoDepthStencilState> m_depthStates;
	std::stack<AutoRasterizerState> m_rasterizerStates;

	ID3D11BlendState* m_currentBlendState;
	ID3D11DepthStencilState* m_currentDepthState;
	ID3D11RasterizerState* m_currentRasterizerState;

	enum DirtyFlags : u64
	{
		DirtyFlag_Texture0 = UINT64_C(1) << 0,
		DirtyFlag_Texture1 = UINT64_C(1) << 1,
		DirtyFlag_Texture2 = UINT64_C(1) << 2,
		DirtyFlag_Texture3 = UINT64_C(1) << 3,
		DirtyFlag_Texture4 = UINT64_C(1) << 4,
		DirtyFlag_Texture5 = UINT64_C(1) << 5,
		DirtyFlag_Texture6 = UINT64_C(1) << 6,
		DirtyFlag_Texture7 = UINT64_C(1) << 7,
		DirtyFlag_Texture8 = UINT64_C(1) << 8,
		DirtyFlag_Texture9 = UINT64_C(1) << 9,
		DirtyFlag_Texture10 = UINT64_C(1) << 10,
		DirtyFlag_Texture11 = UINT64_C(1) << 11,
		DirtyFlag_Texture12 = UINT64_C(1) << 12,
		DirtyFlag_Texture13 = UINT64_C(1) << 13,
		DirtyFlag_Texture14 = UINT64_C(1) << 14,
		DirtyFlag_Texture15 = UINT64_C(1) << 15,

		DirtyFlag_Sampler0 = UINT64_C(1) << 16,
		DirtyFlag_Sampler1 = UINT64_C(1) << 17,
		DirtyFlag_Sampler2 = UINT64_C(1) << 18,
		DirtyFlag_Sampler3 = UINT64_C(1) << 19,
		DirtyFlag_Sampler4 = UINT64_C(1) << 20,
		DirtyFlag_Sampler5 = UINT64_C(1) << 21,
		DirtyFlag_Sampler6 = UINT64_C(1) << 22,
		DirtyFlag_Sampler7 = UINT64_C(1) << 23,
		DirtyFlag_Sampler8 = UINT64_C(1) << 24,
		DirtyFlag_Sampler9 = UINT64_C(1) << 25,
		DirtyFlag_Sampler10 = UINT64_C(1) << 26,
		DirtyFlag_Sampler11 = UINT64_C(1) << 27,
		DirtyFlag_Sampler12 = UINT64_C(1) << 28,
		DirtyFlag_Sampler13 = UINT64_C(1) << 29,
		DirtyFlag_Sampler14 = UINT64_C(1) << 30,
		DirtyFlag_Sampler15 = UINT64_C(1) << 31,

		DirtyFlag_PixelConstants = UINT64_C(1) << 32,
		DirtyFlag_VertexConstants = UINT64_C(1) << 33,
		DirtyFlag_GeometryConstants = UINT64_C(1) << 34,

		DirtyFlag_VertexBuffer = UINT64_C(1) << 35,
		DirtyFlag_IndexBuffer = UINT64_C(1) << 36,

		DirtyFlag_PixelShader = UINT64_C(1) << 37,
		DirtyFlag_VertexShader = UINT64_C(1) << 38,
		DirtyFlag_GeometryShader = UINT64_C(1) << 39,

		DirtyFlag_InputAssembler = UINT64_C(1) << 40,
	};

	u64 m_dirtyFlags;

	struct Resources
	{
		ID3D11ShaderResourceView* textures[16];
		ID3D11SamplerState* samplers[16];
		ID3D11Buffer* pixelConstants[2];
		ID3D11Buffer* vertexConstants;
		ID3D11Buffer* geometryConstants;
		ID3D11Buffer* vertexBuffer;
		ID3D11Buffer* indexBuffer;
		u32 vertexBufferStride;
		u32 vertexBufferOffset;
		D3D11_PRIMITIVE_TOPOLOGY topology;
		ID3D11InputLayout* inputLayout;
		ID3D11PixelShader* pixelShader;
		ID3D11VertexShader* vertexShader;
		ID3D11GeometryShader* geometryShader;
	};

	Resources m_pending;
	Resources m_current;
};

extern StateManager* stateman;

}  // namespace

}  // namespace DX11
