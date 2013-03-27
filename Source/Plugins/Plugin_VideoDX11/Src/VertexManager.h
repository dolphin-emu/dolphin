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
