// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "VideoCommon/VertexManagerBase.h"

namespace DX12
{

class D3DStreamBuffer;

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

	std::unique_ptr<D3DStreamBuffer> m_vertex_stream_buffer;
	std::unique_ptr<D3DStreamBuffer> m_index_stream_buffer;

	bool m_vertex_stream_buffer_reallocated = false;
	bool m_index_stream_buffer_reallocated = false;

	std::vector<u8> m_index_cpu_buffer;
	std::vector<u8> m_vertex_cpu_buffer;
};

}  // namespace
