// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/OGL/GLExtensions/GLExtensions.h"
#include "VideoCommon/CPMemory.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/VertexManagerBase.h"

namespace OGL
{
	class GLVertexFormat : public NativeVertexFormat
	{
		PortableVertexDeclaration vtx_decl;

	public:
		GLVertexFormat();
		~GLVertexFormat();

		virtual void Initialize(const PortableVertexDeclaration &_vtx_decl) override;
		virtual void SetupVertexPointers() override;

		GLuint VAO;
	};

// Handles the OpenGL details of drawing lots of vertices quickly.
// Other functionality is moving out.
class VertexManager : public ::VertexManager
{
public:
	VertexManager();
	~VertexManager();
	NativeVertexFormat* CreateNativeVertexFormat() override;
	void CreateDeviceObjects() override;
	void DestroyDeviceObjects() override;

	// NativeVertexFormat use this
	GLuint m_vertex_buffers;
	GLuint m_index_buffers;
	GLuint m_last_vao;
protected:
	virtual void ResetBuffer(u32 stride) override;
private:
	void Draw(u32 stride);
	void vFlush(bool useDstAlpha) override;
	void PrepareDrawBuffers(u32 stride);
};

}
