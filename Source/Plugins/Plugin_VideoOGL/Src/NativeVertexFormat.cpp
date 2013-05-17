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
		GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT, GL_FLOAT
	};
	return lookup[t];
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

	glEnableVertexAttribArray(SHADER_POSITION_ATTRIB);
	glVertexAttribPointer(SHADER_POSITION_ATTRIB, 3, GL_FLOAT, GL_FALSE, vtx_decl.stride, (u8*)NULL);
	
	for (int i = 0; i < 3; i++) {
		if (vtx_decl.num_normals > i) {
			glEnableVertexAttribArray(SHADER_NORM0_ATTRIB+i);
			glVertexAttribPointer(SHADER_NORM0_ATTRIB+i, vtx_decl.normal_gl_size, VarToGL(vtx_decl.normal_gl_type), GL_TRUE, vtx_decl.stride, (u8*)NULL + vtx_decl.normal_offset[i]);
		}
	}

	for (int i = 0; i < 2; i++) {
		if (vtx_decl.color_offset[i] != -1) {
			glEnableVertexAttribArray(SHADER_COLOR0_ATTRIB+i);
			glVertexAttribPointer(SHADER_COLOR0_ATTRIB+i, 4, GL_UNSIGNED_BYTE, GL_TRUE, vtx_decl.stride, (u8*)NULL + vtx_decl.color_offset[i]);
		}
	}

	for (int i = 0; i < 8; i++) {
		if (vtx_decl.texcoord_offset[i] != -1) {
			glEnableVertexAttribArray(SHADER_TEXTURE0_ATTRIB+i);
			glVertexAttribPointer(SHADER_TEXTURE0_ATTRIB+i, vtx_decl.texcoord_size[i], VarToGL(vtx_decl.texcoord_gl_type[i]),
				GL_FALSE, vtx_decl.stride, (u8*)NULL + vtx_decl.texcoord_offset[i]);
		}
	}

	if (vtx_decl.posmtx_offset != -1) {
		glEnableVertexAttribArray(SHADER_POSMTX_ATTRIB);
		glVertexAttribPointer(SHADER_POSMTX_ATTRIB, 4, GL_UNSIGNED_BYTE, GL_FALSE, vtx_decl.stride, (u8*)NULL + vtx_decl.posmtx_offset);
	}

	vm->m_last_vao = VAO;
}

void GLVertexFormat::SetupVertexPointers() {
}

}
