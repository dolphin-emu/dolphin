// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _VERTEXMANAGER_H
#define _VERTEXMANAGER_H

#include "VertexManagerBase.h"
#include "LineGeometryShader.h"
#include "PointGeometryShader.h"

namespace DX11
{

class VertexManager : public ::VertexManager
{
public:
	VertexManager();
	~VertexManager();

	NativeVertexFormat* CreateNativeVertexFormat();
	void CreateDeviceObjects();
	void DestroyDeviceObjects();

private:
	
	void PrepareDrawBuffers();
	void Draw(u32 stride);
	// temp
	void vFlush();

	u32 m_vertex_buffer_cursor;
	u32 m_vertex_draw_offset;
	u32 m_index_buffer_cursor;
	u32 m_current_vertex_buffer;
	u32 m_current_index_buffer;
	u32 m_triangle_draw_index;
	u32 m_line_draw_index;
	u32 m_point_draw_index;
	typedef ID3D11Buffer* PID3D11Buffer;
	PID3D11Buffer* m_index_buffers;
	PID3D11Buffer* m_vertex_buffers;

	LineGeometryShader m_lineShader;
	PointGeometryShader m_pointShader;
};

}  // namespace

#endif
