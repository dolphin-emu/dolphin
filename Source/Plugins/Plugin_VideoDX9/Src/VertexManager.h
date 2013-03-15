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

#include "CPMemory.h"
#include "VertexLoader.h"

#include "VertexManagerBase.h"

namespace DX9
{

class VertexManager : public ::VertexManager
{
public:
	NativeVertexFormat* CreateNativeVertexFormat();
	void GetElements(NativeVertexFormat* format, D3DVERTEXELEMENT9** elems, int* num);
	void CreateDeviceObjects();
	void DestroyDeviceObjects();
private:
	u32 m_vertex_buffer_cursor;
	u32 m_vertex_buffer_size;
	u32 m_index_buffer_cursor;
	u32 m_index_buffer_size;
	u32 m_buffers_count;
	u32 m_current_vertex_buffer;
	u32 m_current_stride;
	u32 m_current_index_buffer;
	LPDIRECT3DVERTEXBUFFER9 *m_vertex_buffers;
	LPDIRECT3DINDEXBUFFER9 *m_index_buffers; 
	void PrepareDrawBuffers(u32 stride);
	void DrawVertexBuffer(int stride);
	void DrawVertexArray(int stride);
	// temp
	void vFlush();
};

}

#endif
