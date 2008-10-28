// Copyright (C) 2003-2008 Dolphin Project.

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
#include "Profiler.h"
#include "x64Emitter.h"
#include "ABI.h"
#include "MemoryUtil.h"
#include "VertexShader.h"

#include "CPMemory.h"
#include "NativeVertexFormat.h"

#define COMPILED_CODE_SIZE 4096

#ifdef _WIN32
#define USE_JIT
#endif

// Note the use of CallCdeclFunction3I etc.
// This is a horrible hack that is necessary because in 64-bit mode, Opengl32.dll is based way, way above the 32-bit
// address space that is within reach of a CALL, and just doing &fn gives us these high uncallable addresses. So we
// want to grab the function pointers from the import table instead.

// This problem does not apply to glew functions, only core opengl32 functions.

// Here's some global state. We only use this to keep track of what we've sent to the OpenGL state
// machine.

DECLARE_IMPORT(glNormalPointer);
DECLARE_IMPORT(glVertexPointer);
DECLARE_IMPORT(glColorPointer);
DECLARE_IMPORT(glTexCoordPointer);

NativeVertexFormat::NativeVertexFormat()
{
	m_compiledCode = (u8 *)AllocateExecutableMemory(COMPILED_CODE_SIZE, false);
	if (m_compiledCode) {
		memset(m_compiledCode, 0, COMPILED_CODE_SIZE);
	}
}

NativeVertexFormat::~NativeVertexFormat()
{
	FreeMemoryPages(m_compiledCode, COMPILED_CODE_SIZE);
	m_compiledCode = 0;
}

inline GLuint VarToGL(VarType t)
{
	switch (t) {
	case VAR_BYTE: return GL_BYTE;
	case VAR_UNSIGNED_BYTE: return GL_UNSIGNED_BYTE;
	case VAR_SHORT: return GL_SHORT;
	case VAR_UNSIGNED_SHORT: return GL_UNSIGNED_SHORT;
	case VAR_FLOAT: return GL_FLOAT;
	}
}

void NativeVertexFormat::Initialize(const PortableVertexDeclaration &vtx_decl)
{
	using namespace Gen;

	if (vtx_decl.stride & 3) {
		// We will not allow vertex components causing uneven strides.
		PanicAlert("Uneven vertex stride: %i", vtx_decl.stride);
	}

	// Alright, we have our vertex declaration. Compile some crazy code to set it quickly using GL.
	u8 *old_code_ptr = GetWritableCodePtr();
	SetCodePtr(m_compiledCode);
	Util::EmitPrologue(6);
	
	CallCdeclFunction4_I(glVertexPointer, 3, GL_FLOAT, vtx_decl.stride, 0);

	if (vtx_decl.num_normals >= 1) {
		CallCdeclFunction3_I(glNormalPointer, VarToGL(vtx_decl.normal_gl_type), vtx_decl.stride, vtx_decl.normal_offset[0]);
		if (vtx_decl.num_normals == 3) {
			CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM1_ATTRIB, vtx_decl.normal_gl_size, VarToGL(vtx_decl.normal_gl_type), GL_TRUE, vtx_decl.stride, vtx_decl.normal_offset[1]);
			CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM2_ATTRIB, vtx_decl.normal_gl_size, VarToGL(vtx_decl.normal_gl_type), GL_TRUE, vtx_decl.stride, vtx_decl.normal_offset[2]);
		}
	}

	for (int i = 0; i < 2; i++) {
		if (vtx_decl.color_offset[i] != -1) {
			if (i == 0)
				CallCdeclFunction4_I(glColorPointer, 4, GL_UNSIGNED_BYTE, vtx_decl.stride, vtx_decl.color_offset[i]);
			else
				CallCdeclFunction4((void *)glSecondaryColorPointer, 4, GL_UNSIGNED_BYTE, vtx_decl.stride, vtx_decl.color_offset[i]); 
		}
	}

	for (int i = 0; i < 8; i++) {
		if (vtx_decl.texcoord_offset[i] != -1) {
			int id = GL_TEXTURE0 + i;
#ifdef _M_X64
#ifdef _MSC_VER
			MOV(32, R(RCX), Imm32(id));
#else
			MOV(32, R(RDI), Imm32(id));
#endif
#else
			ABI_AlignStack(1 * 4);
			PUSH(32, Imm32(id));
#endif
			CALL((void *)glClientActiveTexture);
#ifndef _M_X64
#ifdef _WIN32
			// don't inc stack on windows, stdcall
#else
			ABI_RestoreStack(1 * 4);
#endif
#endif
			CallCdeclFunction4_I(
				glTexCoordPointer, vtx_decl.texcoord_size[i], VarToGL(vtx_decl.texcoord_gl_type[i]),
				vtx_decl.stride, vtx_decl.texcoord_offset[i]);
		}
	}

	if (vtx_decl.posmtx_offset != -1) {
		CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_POSMTX_ATTRIB, 4, GL_UNSIGNED_BYTE, GL_FALSE, vtx_decl.stride, vtx_decl.posmtx_offset);
	}

	Util::EmitEpilogue(6);
	if (Gen::GetCodePtr() - (u8*)m_compiledCode > COMPILED_CODE_SIZE)
	{
		Crash();
	}

	SetCodePtr(old_code_ptr);
	this->vtx_decl = vtx_decl;
}

void NativeVertexFormat::SetupVertexPointers() const {
	// Cast a pointer to compiled code to a pointer to a function taking no parameters, through a (void *) cast first to
	// get around type checking errors, and call it.
#ifdef USE_JIT
	((void (*)())(void*)m_compiledCode)();
#else
	glVertexPointer(3, GL_FLOAT, vtx_decl.stride, 0);
	if (vtx_decl.num_normals >= 1) {
		glNormalPointer(VarToGL(vtx_decl.normal_gl_type), vtx_decl.stride, (void *)vtx_decl.normal_offset[0]);
		if (vtx_decl.num_normals == 3) {
			glVertexAttribPointer(SHADER_NORM1_ATTRIB, vtx_decl.normal_gl_size, VarToGL(vtx_decl.normal_gl_type), GL_TRUE, vtx_decl.stride, (void *)vtx_decl.normal_offset[1]);
			glVertexAttribPointer(SHADER_NORM2_ATTRIB, vtx_decl.normal_gl_size, VarToGL(vtx_decl.normal_gl_type), GL_TRUE, vtx_decl.stride, (void *)vtx_decl.normal_offset[2]);
		}
	}

	for (int i = 0; i < 2; i++) {
		if (vtx_decl.color_offset[i] != -1) {
			if (i == 0)
				glColorPointer(4, GL_UNSIGNED_BYTE, vtx_decl.stride, (void *)vtx_decl.color_offset[i]);
			else
				glSecondaryColorPointer(4, GL_UNSIGNED_BYTE, vtx_decl.stride, (void *)vtx_decl.color_offset[i]); 
		}
	}

	for (int i = 0; i < 8; i++) {
		if (vtx_decl.texcoord_offset[i] != -1) {
			int id = GL_TEXTURE0 + i;
			glClientActiveTexture(id);
			glTexCoordPointer(vtx_decl.texcoord_size[i], VarToGL(vtx_decl.texcoord_gl_type[i]),
				vtx_decl.stride, (void *)vtx_decl.texcoord_offset[i]);
		}
	}

	if (vtx_decl.posmtx_offset != -1) {
		glVertexAttribPointer(SHADER_POSMTX_ATTRIB, 4, GL_UNSIGNED_BYTE, GL_FALSE, vtx_decl.stride, (void *)vtx_decl.posmtx_offset);
	}
#endif
}