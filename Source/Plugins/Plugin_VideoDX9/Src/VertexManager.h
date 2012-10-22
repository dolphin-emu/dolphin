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
	u32 CurrentVBufferIndex;
	u32 CurrentVBufferSize;
	u32 CurrentIBufferIndex;
	u32 CurrentIBufferSize;
	u32 NumVBuffers;
	u32 CurrentVBuffer;
	u32 CurrentIBuffer;
	LPDIRECT3DVERTEXBUFFER9 *VBuffers;
	LPDIRECT3DINDEXBUFFER9 *IBuffers; 
	void PrepareVBuffers(int stride);
	void DrawVB(int stride);
	void DrawVA(int stride);
	// temp
	void vFlush();
};

}

#endif
