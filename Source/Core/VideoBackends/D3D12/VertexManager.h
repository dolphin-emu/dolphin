// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoCommon/VertexManagerBase.h"

namespace DX12
{

class VertexManager final : public VertexManagerBase
{
public:
	VertexManager();
	~VertexManager();

	NativeVertexFormat* CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl) override;
	void CreateDeviceObjects() override;
	void DestroyDeviceObjects() override;

	void SetIndexBuffer();

protected:
	void ResetBuffer(u32 stride) override;

private:

	void PrepareDrawBuffers(u32 stride);
	void Draw(u32 stride);
	void vFlush(bool use_dst_alpha) override;

	u32 m_vertex_draw_offset;
	u32 m_index_draw_offset;

	ID3D12Resource* m_vertex_buffer;
	void* m_vertex_buffer_data;
	ID3D12Resource* m_index_buffer;
	void* m_index_buffer_data;

	bool m_using_cpu_only_buffer = false;
	u8* m_index_cpu_buffer = nullptr;
	u8* m_vertex_cpu_buffer = nullptr;
};

}  // namespace
