// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D/LineGeometryShader.h"
#include "VideoBackends/D3D/PointGeometryShader.h"
#include "VideoCommon/VertexManagerBase.h"

namespace DX11
{

class VertexManager : public ::VertexManager
{
public:
	VertexManager();
	~VertexManager();

	NativeVertexFormat* CreateNativeVertexFormat() override;
	void CreateDeviceObjects() override;
	void DestroyDeviceObjects() override;

protected:
	virtual void ResetBuffer(u32 stride) override;
	u16* GetIndexBuffer() { return &LocalIBuffer[0]; }

private:

	void PrepareDrawBuffers();
	void Draw(u32 stride);
	// temp
	void vFlush(bool useDstAlpha) override;

	u32 m_vertex_buffer_cursor;
	u32 m_vertex_draw_offset;
	u32 m_index_buffer_cursor;
	u32 m_index_draw_offset;
	u32 m_current_vertex_buffer;
	u32 m_current_index_buffer;
	typedef ID3D11Buffer* PID3D11Buffer;
	PID3D11Buffer* m_index_buffers;
	PID3D11Buffer* m_vertex_buffers;

	LineGeometryShader m_lineShader;
	PointGeometryShader m_pointShader;

	std::vector<u8> LocalVBuffer;
	std::vector<u16> LocalIBuffer;
};

}  // namespace
