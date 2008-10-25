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

void NativeVertexFormat::SetupVertexPointers() const {
	// Cast a pointer to compiled code to a pointer to a function taking no parameters, through a (void *) cast first to
	// get around type checking errors, and call it.
	((void (*)())(void*)m_compiledCode)();
}

void NativeVertexFormat::Initialize(const TVtxDesc &vtx_desc, const TVtxAttr &vtx_attr)
{
	using namespace Gen;
	const int col[2] = {vtx_desc.Color0, vtx_desc.Color1};
	// TextureCoord
	const int tc[8] = {
		vtx_desc.Tex0Coord, vtx_desc.Tex1Coord, vtx_desc.Tex2Coord, vtx_desc.Tex3Coord,
		vtx_desc.Tex4Coord, vtx_desc.Tex5Coord, vtx_desc.Tex6Coord, vtx_desc.Tex7Coord,
	};
	
	DVSTARTPROFILE();

	if (m_VBVertexStride & 3) {
		// make sure all strides are at least divisible by 4 (some gfx cards experience a 3x speed boost)
		m_VBStridePad = 4 - (m_VBVertexStride & 3);
		m_VBVertexStride += m_VBStridePad;
	}

	// compile the pointer set function - why?
	u8 *old_code_ptr = GetWritableCodePtr();
	SetCodePtr(m_compiledCode);
	Util::EmitPrologue(6);
	int offset = 0;
	
	// Position
	if (vtx_desc.Position != NOT_PRESENT) {  // TODO: Why the check? Always present, AFAIK!
		CallCdeclFunction4_I(glVertexPointer, 3, GL_FLOAT, m_VBVertexStride, offset);
		offset += 12;
	}

	// Normals
	if (vtx_desc.Normal != NOT_PRESENT) {
		switch (vtx_attr.NormalFormat) {
		case FORMAT_UBYTE:	
		case FORMAT_BYTE:
			CallCdeclFunction3_I(glNormalPointer, GL_BYTE, m_VBVertexStride, offset); offset += 3;
			if (vtx_attr.NormalElements) {
				CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM1_ATTRIB, 3, GL_BYTE, GL_TRUE, m_VBVertexStride, offset); offset += 3;
				CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM2_ATTRIB, 3, GL_BYTE, GL_TRUE, m_VBVertexStride, offset); offset += 3;
			}
			break;
		case FORMAT_USHORT:
		case FORMAT_SHORT:
			CallCdeclFunction3_I(glNormalPointer, GL_SHORT, m_VBVertexStride, offset); offset += 6;
			if (vtx_attr.NormalElements) {
				CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM1_ATTRIB, 3, GL_SHORT, GL_TRUE, m_VBVertexStride, offset); offset += 6;
				CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM2_ATTRIB, 3, GL_SHORT, GL_TRUE, m_VBVertexStride, offset); offset += 6;
			}
			break;
		case FORMAT_FLOAT:
			CallCdeclFunction3_I(glNormalPointer, GL_FLOAT, m_VBVertexStride, offset); offset += 12;
			if (vtx_attr.NormalElements) {
				CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM1_ATTRIB, 3, GL_FLOAT, GL_TRUE, m_VBVertexStride, offset); offset += 12;
				CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_NORM2_ATTRIB, 3, GL_FLOAT, GL_TRUE, m_VBVertexStride, offset); offset += 12;
			}
			break;
		default: _assert_(0); break;
		}
	}

	// TODO : With byte or short normals above, offset will be misaligned (not 4byte aligned)! Ugh!

	for (int i = 0; i < 2; i++) {
		if (col[i] != NOT_PRESENT) {
			if (i)
				CallCdeclFunction4((void *)glSecondaryColorPointer, 4, GL_UNSIGNED_BYTE, m_VBVertexStride, offset); 
			else
				CallCdeclFunction4_I(glColorPointer, 4, GL_UNSIGNED_BYTE, m_VBVertexStride, offset);
			offset += 4;
		}
	}

	// TextureCoord
	for (int i = 0; i < 8; i++) {
		if (tc[i] != NOT_PRESENT || (m_components & (VB_HAS_TEXMTXIDX0 << i))) {
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
			// TODO : More potential disalignment!
			if (m_components & (VB_HAS_TEXMTXIDX0 << i)) {
				if (tc[i] != NOT_PRESENT) {
					CallCdeclFunction4_I(glTexCoordPointer, 3, GL_FLOAT, m_VBVertexStride, offset);
					offset += 12;
				}
				else {
					CallCdeclFunction4_I(glTexCoordPointer, 3, GL_SHORT, m_VBVertexStride, offset);
					offset += 6;
				}
			}
			else {
				CallCdeclFunction4_I(glTexCoordPointer, vtx_attr.texCoord[i].Elements ? 2 : 1, GL_FLOAT, m_VBVertexStride, offset);
				offset += 4 * (vtx_attr.texCoord[i].Elements?2:1);
			}
		}
	}

	if (vtx_desc.PosMatIdx) {
		CallCdeclFunction6((void *)glVertexAttribPointer, SHADER_POSMTX_ATTRIB, 1, GL_UNSIGNED_BYTE, GL_FALSE, m_VBVertexStride, offset);
		offset += 1;
	}

	_assert_(offset + m_VBStridePad == m_VBVertexStride);

	Util::EmitEpilogue(6);
	if (Gen::GetCodePtr() - (u8*)m_compiledCode > COMPILED_CODE_SIZE)
	{
		Crash();
	}

	SetCodePtr(old_code_ptr);
}
