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

#include "GLUtil.h"
#include "x64Emitter.h"
#include "ABI.h"
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

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, vtx_decl.stride, (u8*)NULL);
	
	if (vtx_decl.num_normals >= 1) {
		glEnableClientState(GL_NORMAL_ARRAY);
		glNormalPointer(VarToGL(vtx_decl.normal_gl_type), vtx_decl.stride, (u8*)NULL + vtx_decl.normal_offset[0]);
		if (vtx_decl.num_normals == 3) {
			glEnableVertexAttribArray(SHADER_NORM1_ATTRIB);
			glEnableVertexAttribArray(SHADER_NORM2_ATTRIB);
			glVertexAttribPointer(SHADER_NORM1_ATTRIB, vtx_decl.normal_gl_size, VarToGL(vtx_decl.normal_gl_type), GL_TRUE, vtx_decl.stride, (u8*)NULL + vtx_decl.normal_offset[1]);
			glVertexAttribPointer(SHADER_NORM2_ATTRIB, vtx_decl.normal_gl_size, VarToGL(vtx_decl.normal_gl_type), GL_TRUE, vtx_decl.stride, (u8*)NULL + vtx_decl.normal_offset[2]);
		}
	}

	for (int i = 0; i < 2; i++) {
		if (vtx_decl.color_offset[i] != -1) {
			if (i == 0) {
				glEnableClientState(GL_COLOR_ARRAY);
				glColorPointer(4, GL_UNSIGNED_BYTE, vtx_decl.stride, (u8*)NULL + vtx_decl.color_offset[i]);
			} else {
				glEnableClientState(GL_SECONDARY_COLOR_ARRAY);
				glSecondaryColorPointer(4, GL_UNSIGNED_BYTE, vtx_decl.stride, (u8*)NULL + vtx_decl.color_offset[i]); 
			}
		}
	}

	for (int i = 0; i < 8; i++) {
		if (vtx_decl.texcoord_offset[i] != -1) {
			int id = GL_TEXTURE0 + i;
			glClientActiveTexture(id);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer(vtx_decl.texcoord_size[i], VarToGL(vtx_decl.texcoord_gl_type[i]),
				vtx_decl.stride, (u8*)NULL + vtx_decl.texcoord_offset[i]);
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
