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
	
	void LoadBuffers();
	void Draw(UINT stride);
	// temp
	void vFlush();

	UINT m_indexBufferCursor;
	UINT m_vertexBufferCursor;
	UINT m_vertexDrawOffset;
	UINT m_triangleDrawIndex;
	UINT m_lineDrawIndex;
	UINT m_pointDrawIndex;	
	UINT m_activeVertexBuffer;
	UINT m_activeIndexBuffer;
	typedef ID3D11Buffer* PID3D11Buffer;
	PID3D11Buffer* m_indexBuffers;
	PID3D11Buffer* m_vertexBuffers;

	LineGeometryShader m_lineShader;
	PointGeometryShader m_pointShader;
};

}  // namespace

#endif
