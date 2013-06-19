// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _VERTEXMANAGER_H_
#define _VERTEXMANAGER_H_

#include "CPMemory.h"

#include "VertexManagerBase.h"

namespace OGL
{
	class GLVertexFormat : public NativeVertexFormat
	{
		PortableVertexDeclaration vtx_decl;

	public:
		GLVertexFormat();
		~GLVertexFormat();

		virtual void Initialize(const PortableVertexDeclaration &_vtx_decl);
		virtual void SetupVertexPointers();
		
		GLuint VAO;
	};

// Handles the OpenGL details of drawing lots of vertices quickly.
// Other functionality is moving out.
class VertexManager : public ::VertexManager
{
public:
	VertexManager();
	~VertexManager();
	NativeVertexFormat* CreateNativeVertexFormat();
	void CreateDeviceObjects();
	void DestroyDeviceObjects();
	
	// NativeVertexFormat use this
	GLuint m_vertex_buffers;
	GLuint m_index_buffers; 
	GLuint m_last_vao;
private:
	void Draw(u32 stride);
	void vFlush();
	void PrepareDrawBuffers(u32 stride);
	NativeVertexFormat *m_CurrentVertexFmt;
};

}

#endif  // _VERTEXMANAGER_H_
