// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "GLUtil.h"
#include "x64Emitter.h"
#include "x64ABI.h"
#include "MemoryUtil.h"
#include "ProgramShaderCache.h"
#include "VertexShaderGen.h"

#include "CPMemory.h"
#include "NativeVertexFormat.h"
#include "VertexManager.h"

// Here's some global state. We only use this to keep track of what we've sent to the OpenGL state
// machine.

namespace OGL
{

NativeVertexFormat* VertexManager::CreateNativeVertexFormat()
{
	return new GLVertexFormat();
}

GLVertexFormat::GLVertexFormat()
{

}

GLVertexFormat::~GLVertexFormat()
{
	glDeleteVertexArrays(1, &VAO);
}

inline GLuint VarToGL(VarType t)
{
	static const GLuint lookup[5] = {
		GL_UNSIGNED_BYTE, GL_BYTE, GL_UNSIGNED_SHORT, GL_SHORT, GL_FLOAT
	};
	return lookup[t];
}

static void SetPointer(u32 attrib, u32 stride, const AttributeFormat &format)
{
	if (!format.enable)
		return;

	glEnableVertexAttribArray(attrib);
	if (format.integer)
		glVertexAttribIPointer(attrib, format.components, VarToGL(format.type), stride, (u8*)NULL + format.offset);
	else
		glVertexAttribPointer(attrib, format.components, VarToGL(format.type), true, stride, (u8*)NULL + format.offset);
}

void GLVertexFormat::Initialize(const PortableVertexDeclaration &_vtx_decl)
{
	this->vtx_decl = _vtx_decl;
	vertex_stride = vtx_decl.stride;

	// We will not allow vertex components causing uneven strides.
	if (vertex_stride & 3)
		PanicAlert("Uneven vertex stride: %i", vertex_stride);

	VertexManager *vm = (OGL::VertexManager*)g_vertex_manager;

	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

	// the element buffer is bound directly to the vao, so we must it set for every vao
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vm->m_index_buffers);
	glBindBuffer(GL_ARRAY_BUFFER, vm->m_vertex_buffers);

	SetPointer(SHADER_POSITION_ATTRIB, vertex_stride, vtx_decl.position);

	for (int i = 0; i < 3; i++) {
		SetPointer(SHADER_NORM0_ATTRIB+i, vertex_stride, vtx_decl.normals[i]);
	}

	for (int i = 0; i < 2; i++) {
		SetPointer(SHADER_COLOR0_ATTRIB+i, vertex_stride, vtx_decl.colors[i]);
	}

	for (int i = 0; i < 8; i++) {
		SetPointer(SHADER_TEXTURE0_ATTRIB+i, vertex_stride, vtx_decl.texcoords[i]);
	}

	SetPointer(SHADER_POSMTX_ATTRIB, vertex_stride, vtx_decl.posmtx);

	vm->m_last_vao = VAO;
}

void GLVertexFormat::SetupVertexPointers() {
}

}
