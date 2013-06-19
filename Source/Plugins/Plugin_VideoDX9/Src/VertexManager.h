// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
