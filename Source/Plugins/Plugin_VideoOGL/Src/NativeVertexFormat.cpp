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
#include "VertexShaderGen.h"

#include "CPMemory.h"
#include "NativeVertexFormat.h"
#include "NativeVertexWriter.h"

#define COMPILED_CODE_SIZE 4096

u32 s_prevcomponents; // previous state set

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

#ifdef USE_JIT
DECLARE_IMPORT(glNormalPointer);
DECLARE_IMPORT(glVertexPointer);
DECLARE_IMPORT(glColorPointer);
DECLARE_IMPORT(glTexCoordPointer);
#endif

class GLVertexFormat : public NativeVertexFormat
{
	u8 *m_compiledCode;
	PortableVertexDeclaration vtx_decl;

public:
	GLVertexFormat();
	~GLVertexFormat();

	virtual void Initialize(const PortableVertexDeclaration &_vtx_decl);
	virtual void SetupVertexPointers() const;
	virtual void EnableComponents(u32 components);
};


NativeVertexFormat *NativeVertexFormat::Create()
{
	return new GLVertexFormat();
}

GLVertexFormat::GLVertexFormat()
{
#ifdef USE_JIT
	m_compiledCode = (u8 *)AllocateExecutableMemory(COMPILED_CODE_SIZE, false);
	if (m_compiledCode) {
		memset(m_compiledCode, 0, COMPILED_CODE_SIZE);
	}
#endif
}

GLVertexFormat::~GLVertexFormat()
{
#ifdef USE_JIT
	FreeMemoryPages(m_compiledCode, COMPILED_CODE_SIZE);
	m_compiledCode = 0;
#endif
}

inline GLuint VarToGL(VarType t)
{
	static const GLuint lookup[5] = {GL_BYTE, GL_UNSIGNED_BYTE, GL_SHORT, GL_UNSIGNED_SHORT, GL_FLOAT};
	return lookup[t];
}

void GLVertexFormat::Initialize(const PortableVertexDeclaration &_vtx_decl)
{
	using namespace Gen;

	if (_vtx_decl.stride & 3) {
		// We will not allow vertex components causing uneven strides.
		PanicAlert("Uneven vertex stride: %i", _vtx_decl.stride);
	}

#ifdef USE_JIT
	Gen::XEmitter emit(m_compiledCode);
	// Alright, we have our vertex declaration. Compile some crazy code to set it quickly using GL.
	emit.ABI_EmitPrologue(6);
	
	emit.CallCdeclFunction4_I(glVertexPointer, 3, GL_FLOAT, _vtx_decl.stride, 0);

	if (_vtx_decl.num_normals >= 1) {
		emit.CallCdeclFunction3_I(glNormalPointer, VarToGL(_vtx_decl.normal_gl_type), _vtx_decl.stride, _vtx_decl.normal_offset[0]);
		if (_vtx_decl.num_normals == 3) {
			emit.CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM1_ATTRIB, _vtx_decl.normal_gl_size, VarToGL(_vtx_decl.normal_gl_type), GL_TRUE, _vtx_decl.stride, _vtx_decl.normal_offset[1]);
			emit.CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM2_ATTRIB, _vtx_decl.normal_gl_size, VarToGL(_vtx_decl.normal_gl_type), GL_TRUE, _vtx_decl.stride, _vtx_decl.normal_offset[2]);
		}
	}

	for (int i = 0; i < 2; i++) {
		if (_vtx_decl.color_offset[i] != -1) {
			if (i == 0)
				emit.CallCdeclFunction4_I(glColorPointer, 4, GL_UNSIGNED_BYTE, _vtx_decl.stride, _vtx_decl.color_offset[i]);
			else
				emit.CallCdeclFunction4((void *)glSecondaryColorPointer, 4, GL_UNSIGNED_BYTE, _vtx_decl.stride, _vtx_decl.color_offset[i]); 
		}
	}

	for (int i = 0; i < 8; i++)
	{
		if (_vtx_decl.texcoord_offset[i] != -1)
		{
			int id = GL_TEXTURE0 + i;
#ifdef _M_X64
#ifdef _MSC_VER
			emit.MOV(32, R(RCX), Imm32(id));
#else
			emit.MOV(32, R(RDI), Imm32(id));
#endif
#else
			emit.ABI_AlignStack(1 * 4);
			emit.PUSH(32, Imm32(id));
#endif
			emit.CALL((void *)glClientActiveTexture);
#ifndef _M_X64
#ifdef _WIN32
			// don't inc stack on windows, stdcall
#else
			emit.ABI_RestoreStack(1 * 4);
#endif
#endif
			emit.CallCdeclFunction4_I(
				glTexCoordPointer, _vtx_decl.texcoord_size[i], VarToGL(_vtx_decl.texcoord_gl_type[i]),
				_vtx_decl.stride, _vtx_decl.texcoord_offset[i]);
		}
	}

	if (_vtx_decl.posmtx_offset != -1) {
		emit.CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_POSMTX_ATTRIB, 4, GL_UNSIGNED_BYTE, GL_FALSE, _vtx_decl.stride, _vtx_decl.posmtx_offset);
	}

	emit.ABI_EmitEpilogue(6);
	if (emit.GetCodePtr() - (u8*)m_compiledCode > COMPILED_CODE_SIZE)
	{
		Crash();
	}

#endif
	this->vtx_decl = _vtx_decl;
}

void GLVertexFormat::SetupVertexPointers() const {
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

void GLVertexFormat::EnableComponents(u32 components)
{
	if (s_prevcomponents != components) {
		VertexManager::Flush();

		// matrices
		if ((components & VB_HAS_POSMTXIDX) != (s_prevcomponents & VB_HAS_POSMTXIDX)) {
			if (components & VB_HAS_POSMTXIDX)
				glEnableVertexAttribArray(SHADER_POSMTX_ATTRIB);
			else
				glDisableVertexAttribArray(SHADER_POSMTX_ATTRIB);
		}

		// normals
		if ((components & VB_HAS_NRM0) != (s_prevcomponents & VB_HAS_NRM0)) {
			if (components & VB_HAS_NRM0)
				glEnableClientState(GL_NORMAL_ARRAY);
			else
				glDisableClientState(GL_NORMAL_ARRAY);
		}
		if ((components & VB_HAS_NRM1) != (s_prevcomponents & VB_HAS_NRM1)) {
			if (components & VB_HAS_NRM1) {
				glEnableVertexAttribArray(SHADER_NORM1_ATTRIB);
				glEnableVertexAttribArray(SHADER_NORM2_ATTRIB);
			}
			else {
				glDisableVertexAttribArray(SHADER_NORM1_ATTRIB);
				glDisableVertexAttribArray(SHADER_NORM2_ATTRIB);
			}
		}

		// color
		for (int i = 0; i < 2; ++i) {
			if ((components & (VB_HAS_COL0 << i)) != (s_prevcomponents & (VB_HAS_COL0 << i))) {
				if (components & (VB_HAS_COL0 << 0))
					glEnableClientState(i ? GL_SECONDARY_COLOR_ARRAY : GL_COLOR_ARRAY);
				else
					glDisableClientState(i ? GL_SECONDARY_COLOR_ARRAY : GL_COLOR_ARRAY);
			}
		}

		// tex
		if (!g_Config.bDisableTexturing) {
			for (int i = 0; i < 8; ++i) {
				if ((components & (VB_HAS_UV0 << i)) != (s_prevcomponents & (VB_HAS_UV0 << i))) {
					glClientActiveTexture(GL_TEXTURE0 + i);
					if (components & (VB_HAS_UV0 << i))
						glEnableClientState(GL_TEXTURE_COORD_ARRAY);
					else
						glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				}
			}
		}
		else // Disable Texturing
		{
			for (int i = 0; i < 8; ++i) {
				glClientActiveTexture(GL_TEXTURE0 + i);
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			}
		}

		// Disable Lighting	
		// TODO - move to better spot
		if (g_Config.bDisableLighting) {
			for (int i = 0; i < xfregs.nNumChans; i++)
			{
				xfregs.colChans[i].alpha.enablelighting = false;
				xfregs.colChans[i].color.enablelighting = false;
			}
		}
		s_prevcomponents = components;
	}
}
