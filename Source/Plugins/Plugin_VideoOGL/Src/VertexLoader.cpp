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

#include "Globals.h"

#include <assert.h>

#include "Common.h"
#include "Config.h"
#include "Profiler.h"
#include "MemoryUtil.h"
#include "StringUtil.h"
#include "x64Emitter.h"
#include "ABI.h"

#include "LookUpTables.h"
#include "Statistics.h"
#include "VertexManager.h"
#include "VertexLoaderManager.h"
#include "VertexShaderManager.h"
#include "VertexManager.h"
#include "VertexLoader.h"
#include "BPStructs.h"
#include "DataReader.h"

#include "VertexLoader_Position.h"
#include "VertexLoader_Normal.h"
#include "VertexLoader_Color.h"
#include "VertexLoader_TextCoord.h"

#define USE_JIT

#define COMPILED_CODE_SIZE 4096*4

NativeVertexFormat *g_nativeVertexFmt;

//these don't need to be saved
#ifndef _WIN32
	#undef inline
	#define inline
#endif

// Direct
// ==============================================================================
static u8 s_curposmtx;
static u8 s_curtexmtx[8];
static int s_texmtxwrite = 0;
static int s_texmtxread = 0;
static int loop_counter;

// Vertex loaders read these. Although the scale ones should be baked into the shader.
int tcIndex;
int colIndex;
TVtxAttr* pVtxAttr;
int colElements[2];
float posScale;
float tcScale[8];

using namespace Gen;

void LOADERDECL PosMtx_ReadDirect_UByte()
{
	s_curposmtx = DataReadU8() & 0x3f;
	PRIM_LOG("posmtx: %d, ", s_curposmtx);
}

void LOADERDECL PosMtx_Write()
{
	*VertexManager::s_pCurBufferPointer++ = s_curposmtx;
	*VertexManager::s_pCurBufferPointer++ = 0;
	*VertexManager::s_pCurBufferPointer++ = 0;
	*VertexManager::s_pCurBufferPointer++ = 0;
}

void LOADERDECL TexMtx_ReadDirect_UByte()
{
	s_curtexmtx[s_texmtxread] = DataReadU8()&0x3f;
	PRIM_LOG("texmtx%d: %d, ", s_texmtxread, s_curtexmtx[s_texmtxread]);
	s_texmtxread++;
}

void LOADERDECL TexMtx_Write_Float()
{
	*(float*)VertexManager::s_pCurBufferPointer = (float)s_curtexmtx[s_texmtxwrite++];
	VertexManager::s_pCurBufferPointer += 4;
}

void LOADERDECL TexMtx_Write_Float2()
{
	((float*)VertexManager::s_pCurBufferPointer)[0] = 0;
	((float*)VertexManager::s_pCurBufferPointer)[1] = (float)s_curtexmtx[s_texmtxwrite++];
	VertexManager::s_pCurBufferPointer += 8;
}

void LOADERDECL TexMtx_Write_Short3()
{
	((s16*)VertexManager::s_pCurBufferPointer)[0] = 0;
	((s16*)VertexManager::s_pCurBufferPointer)[1] = 0;
	((s16*)VertexManager::s_pCurBufferPointer)[2] = s_curtexmtx[s_texmtxwrite++];
	VertexManager::s_pCurBufferPointer += 8;
}

VertexLoader::VertexLoader(const TVtxDesc &vtx_desc, const VAT &vtx_attr) 
{
	m_numLoadedVertices = 0;
	m_VertexSize = 0;
	m_numPipelineStages = 0;
	m_NativeFmt = new NativeVertexFormat();
	loop_counter = 0;
	VertexLoader_Normal::Init();

	m_VtxDesc = vtx_desc;
	SetVAT(vtx_attr.g0.Hex, vtx_attr.g1.Hex, vtx_attr.g2.Hex);

	m_compiledCode = (u8 *)AllocateExecutableMemory(COMPILED_CODE_SIZE, false);
	if (m_compiledCode) {
		memset(m_compiledCode, 0, COMPILED_CODE_SIZE);
	}
	CompileVertexTranslator();
}

VertexLoader::~VertexLoader() 
{
	FreeMemoryPages(m_compiledCode, COMPILED_CODE_SIZE);
	delete m_NativeFmt;
}

void VertexLoader::CompileVertexTranslator()
{
	m_VertexSize = 0;
	const TVtxAttr &vtx_attr = m_VtxAttr;
	const TVtxDesc &vtx_desc = m_VtxDesc;

#ifdef USE_JIT
	u8 *old_code_ptr = GetWritableCodePtr();
	SetCodePtr(m_compiledCode);
	ABI_EmitPrologue(4);
	MOV(32, R(EBX), M(&loop_counter));
	// Start loop here
	const u8 *loop_start = GetCodePtr();

	// Reset component counters if present in vertex format only.
	if (m_VtxDesc.Tex0Coord || m_VtxDesc.Tex1Coord || m_VtxDesc.Tex2Coord || m_VtxDesc.Tex3Coord ||
		m_VtxDesc.Tex4Coord || m_VtxDesc.Tex5Coord || m_VtxDesc.Tex6Coord || m_VtxDesc.Tex7Coord) {
		MOV(32, M(&tcIndex), Imm32(0));
	}
	if (m_VtxDesc.Color0 || m_VtxDesc.Color1) {
		MOV(32, M(&colIndex), Imm32(0));
	}
	if (m_VtxDesc.Tex0MatIdx || m_VtxDesc.Tex1MatIdx || m_VtxDesc.Tex2MatIdx || m_VtxDesc.Tex3MatIdx ||
		m_VtxDesc.Tex4MatIdx || m_VtxDesc.Tex5MatIdx || m_VtxDesc.Tex6MatIdx || m_VtxDesc.Tex7MatIdx) {
		MOV(32, M(&s_texmtxwrite), Imm32(0));
		MOV(32, M(&s_texmtxread), Imm32(0));
	}
#endif

	// Colors
	const int col[2] = {m_VtxDesc.Color0, m_VtxDesc.Color1};
	// TextureCoord
	// Since m_VtxDesc.Text7Coord is broken across a 32 bit word boundary, retrieve its value manually.
	// If we didn't do this, the vertex format would be read as one bit offset from where it should be, making
	// 01 become 00, and 10/11 become 01
	const int tc[8] = {
		m_VtxDesc.Tex0Coord, m_VtxDesc.Tex1Coord, m_VtxDesc.Tex2Coord, m_VtxDesc.Tex3Coord,
		m_VtxDesc.Tex4Coord, m_VtxDesc.Tex5Coord, m_VtxDesc.Tex6Coord, (m_VtxDesc.Hex >> 31) & 3
	};
	
	// Reset pipeline
	m_numPipelineStages = 0;

	// It's a bit ugly that we poke inside m_NativeFmt in this function. Planning to fix this.
	m_NativeFmt->m_components = 0;

	// Position in pc vertex format.
	int nat_offset = 0;
	PortableVertexDeclaration vtx_decl;
	memset(&vtx_decl, 0, sizeof(vtx_decl));
	for (int i = 0; i < 8; i++) {
		vtx_decl.texcoord_offset[i] = -1;
	}

	// m_VBVertexStride for texmtx and posmtx is computed later when writing.
	
	// Position Matrix Index
	if (m_VtxDesc.PosMatIdx) {
		WriteCall(PosMtx_ReadDirect_UByte);
		m_NativeFmt->m_components |= VB_HAS_POSMTXIDX;
		m_VertexSize += 1;
	}

	if (m_VtxDesc.Tex0MatIdx) {m_VertexSize += 1; m_NativeFmt->m_components |= VB_HAS_TEXMTXIDX0; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex1MatIdx) {m_VertexSize += 1; m_NativeFmt->m_components |= VB_HAS_TEXMTXIDX1; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex2MatIdx) {m_VertexSize += 1; m_NativeFmt->m_components |= VB_HAS_TEXMTXIDX2; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex3MatIdx) {m_VertexSize += 1; m_NativeFmt->m_components |= VB_HAS_TEXMTXIDX3; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex4MatIdx) {m_VertexSize += 1; m_NativeFmt->m_components |= VB_HAS_TEXMTXIDX4; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex5MatIdx) {m_VertexSize += 1; m_NativeFmt->m_components |= VB_HAS_TEXMTXIDX5; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex6MatIdx) {m_VertexSize += 1; m_NativeFmt->m_components |= VB_HAS_TEXMTXIDX6; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex7MatIdx) {m_VertexSize += 1; m_NativeFmt->m_components |= VB_HAS_TEXMTXIDX7; WriteCall(TexMtx_ReadDirect_UByte); }

	// Position
	if (m_VtxDesc.Position != NOT_PRESENT) {
		nat_offset += 12;
	}

	switch (m_VtxDesc.Position) {
	case NOT_PRESENT:	{_assert_msg_(0, "Vertex descriptor without position!", "WTF?");} break;
	case DIRECT:
		{
			switch (m_VtxAttr.PosFormat) {
	        case FORMAT_UBYTE:  m_VertexSize += m_VtxAttr.PosElements?3:2; WriteCall(Pos_ReadDirect_UByte);  break;
			case FORMAT_BYTE:   m_VertexSize += m_VtxAttr.PosElements?3:2; WriteCall(Pos_ReadDirect_Byte);   break;
			case FORMAT_USHORT: m_VertexSize += m_VtxAttr.PosElements?6:4; WriteCall(Pos_ReadDirect_UShort); break;
			case FORMAT_SHORT:  m_VertexSize += m_VtxAttr.PosElements?6:4; WriteCall(Pos_ReadDirect_Short);  break;
			case FORMAT_FLOAT:  m_VertexSize += m_VtxAttr.PosElements?12:8; WriteCall(Pos_ReadDirect_Float);  break;
			default: _assert_(0); break;
			}
		}
		break;
	case INDEX8:		
		switch (m_VtxAttr.PosFormat) {
		case FORMAT_UBYTE:	WriteCall(Pos_ReadIndex8_UByte);  break; //WTF?
		case FORMAT_BYTE:	WriteCall(Pos_ReadIndex8_Byte);   break;
		case FORMAT_USHORT:	WriteCall(Pos_ReadIndex8_UShort); break;
		case FORMAT_SHORT:	WriteCall(Pos_ReadIndex8_Short);  break;
		case FORMAT_FLOAT:	WriteCall(Pos_ReadIndex8_Float);  break;
		default: _assert_(0); break;
		}
		m_VertexSize += 1;
		break;
	case INDEX16:
		switch (m_VtxAttr.PosFormat) {
		case FORMAT_UBYTE:	WriteCall(Pos_ReadIndex16_UByte);  break;
		case FORMAT_BYTE:	WriteCall(Pos_ReadIndex16_Byte);   break;
		case FORMAT_USHORT:	WriteCall(Pos_ReadIndex16_UShort); break;
		case FORMAT_SHORT:	WriteCall(Pos_ReadIndex16_Short);  break;
		case FORMAT_FLOAT:	WriteCall(Pos_ReadIndex16_Float);  break;
		default: _assert_(0); break;
		}
		m_VertexSize += 2;
		break;
	}

	// Normals
	vtx_decl.num_normals = 0;
	if (m_VtxDesc.Normal != NOT_PRESENT) {
		m_VertexSize += VertexLoader_Normal::GetSize(m_VtxDesc.Normal, m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);
		TPipelineFunction pFunc = VertexLoader_Normal::GetFunction(m_VtxDesc.Normal, m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);
		if (pFunc == 0)
		{
			char temp[256];
			sprintf(temp,"%i %i %i %i", m_VtxDesc.Normal, m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);
			g_VideoInitialize.pSysMessage("VertexLoader_Normal::GetFunction returned zero!");
		}
		WriteCall(pFunc);

		vtx_decl.num_normals = vtx_attr.NormalElements ? 3 : 1;
		switch (vtx_attr.NormalFormat) {
		case FORMAT_UBYTE:	
		case FORMAT_BYTE:
			vtx_decl.normal_gl_type = VAR_BYTE;
			vtx_decl.normal_gl_size = 4;
			vtx_decl.normal_offset[0] = nat_offset;
			nat_offset += 4;
			if (vtx_attr.NormalElements) {
				vtx_decl.normal_offset[1] = nat_offset;
				nat_offset += 4;
				vtx_decl.normal_offset[2] = nat_offset;
				nat_offset += 4;
			}
			break;
		case FORMAT_USHORT:
		case FORMAT_SHORT:
			vtx_decl.normal_gl_type = VAR_SHORT;
			vtx_decl.normal_gl_size = 4;
			vtx_decl.normal_offset[0] = nat_offset;
			nat_offset += 8;
			if (vtx_attr.NormalElements) {
				vtx_decl.normal_offset[1] = nat_offset;
				nat_offset += 8;
				vtx_decl.normal_offset[2] = nat_offset;
				nat_offset += 8;
			}
			break;
		case FORMAT_FLOAT:
			vtx_decl.normal_gl_type = VAR_FLOAT;
			vtx_decl.normal_gl_size = 3;
			vtx_decl.normal_offset[0] = nat_offset;
			nat_offset += 12;
			if (vtx_attr.NormalElements) {
				vtx_decl.normal_offset[1] = nat_offset;
				nat_offset += 12;
				vtx_decl.normal_offset[2] = nat_offset;
				nat_offset += 12;
			}
			break;
		default: _assert_(0); break;
		}

		int numNormals = (m_VtxAttr.NormalElements == 1) ? NRM_THREE : NRM_ONE;
		m_NativeFmt->m_components |= VB_HAS_NRM0;

		if (numNormals == NRM_THREE)
			m_NativeFmt->m_components |= VB_HAS_NRM1 | VB_HAS_NRM2;
	}

	vtx_decl.color_gl_type = VAR_UNSIGNED_BYTE;
	for (int i = 0; i < 2; i++) {
		m_NativeFmt->m_components |= VB_HAS_COL0 << i;
		switch (col[i])
		{
		case NOT_PRESENT: 
			m_NativeFmt->m_components &= ~(VB_HAS_COL0 << i);
			break;
		case DIRECT:
			switch (m_VtxAttr.color[i].Comp)
			{
			case FORMAT_16B_565:	WriteCall(Color_ReadDirect_16b_565); break;
			case FORMAT_24B_888:	WriteCall(Color_ReadDirect_24b_888); break;
			case FORMAT_32B_888x:	WriteCall(Color_ReadDirect_32b_888x); break;
			case FORMAT_16B_4444:	WriteCall(Color_ReadDirect_16b_4444); break;
			case FORMAT_24B_6666:	WriteCall(Color_ReadDirect_24b_6666); break;
			case FORMAT_32B_8888:	WriteCall(Color_ReadDirect_32b_8888); break;
			default: _assert_(0); break;
			}
			switch (m_VtxAttr.color[i].Comp)
			{
			case FORMAT_16B_565:	m_VertexSize += 2; break;
			case FORMAT_24B_888:	m_VertexSize += 3; break;
			case FORMAT_32B_888x:	m_VertexSize += 4; break;
			case FORMAT_16B_4444:	m_VertexSize += 2; break;
			case FORMAT_24B_6666:	m_VertexSize += 3; break;
			case FORMAT_32B_8888:	m_VertexSize += 4; break;
			default: _assert_(0); break;
			}									    
			break;
		case INDEX8:	
			switch (m_VtxAttr.color[i].Comp)
			{
			case FORMAT_16B_565:	WriteCall(Color_ReadIndex8_16b_565); break;
			case FORMAT_24B_888:	WriteCall(Color_ReadIndex8_24b_888); break;
			case FORMAT_32B_888x:	WriteCall(Color_ReadIndex8_32b_888x); break;
			case FORMAT_16B_4444:	WriteCall(Color_ReadIndex8_16b_4444); break;
			case FORMAT_24B_6666:	WriteCall(Color_ReadIndex8_24b_6666); break;
			case FORMAT_32B_8888:	WriteCall(Color_ReadIndex8_32b_8888); break;
			default: _assert_(0); break;
			}
			m_VertexSize += 1;
			break;
		case INDEX16:
			switch (m_VtxAttr.color[i].Comp)
			{
			case FORMAT_16B_565:	WriteCall(Color_ReadIndex16_16b_565); break;
			case FORMAT_24B_888:	WriteCall(Color_ReadIndex16_24b_888); break;
			case FORMAT_32B_888x:	WriteCall(Color_ReadIndex16_32b_888x); break;
			case FORMAT_16B_4444:	WriteCall(Color_ReadIndex16_16b_4444); break;
			case FORMAT_24B_6666:	WriteCall(Color_ReadIndex16_24b_6666); break;
			case FORMAT_32B_8888:	WriteCall(Color_ReadIndex16_32b_8888); break;
			default: _assert_(0); break;
			}
			m_VertexSize += 2;
			break;
		}

		if (col[i] != NOT_PRESENT) {
			vtx_decl.color_offset[i] = nat_offset;
			nat_offset += 4;
		} else {
			vtx_decl.color_offset[i] = -1;
		}
	}

	// Texture matrix indices (remove if corresponding texture coordinate isn't enabled)
	for (int i = 0; i < 8; i++) {
		m_NativeFmt->m_components |= VB_HAS_UV0 << i;
		int elements = m_VtxAttr.texCoord[i].Elements;
		switch (tc[i])
		{
		case NOT_PRESENT: 
			m_NativeFmt->m_components &= ~(VB_HAS_UV0 << i);
			break;
		case DIRECT:
			switch (m_VtxAttr.texCoord[i].Format)
			{
			case FORMAT_UBYTE:	m_VertexSize += elements?2:1; WriteCall(elements?TexCoord_ReadDirect_UByte2:TexCoord_ReadDirect_UByte1);  break;
			case FORMAT_BYTE:	m_VertexSize += elements?2:1; WriteCall(elements?TexCoord_ReadDirect_Byte2:TexCoord_ReadDirect_Byte1);   break;
			case FORMAT_USHORT:	m_VertexSize += elements?4:2; WriteCall(elements?TexCoord_ReadDirect_UShort2:TexCoord_ReadDirect_UShort1); break;
			case FORMAT_SHORT:	m_VertexSize += elements?4:2; WriteCall(elements?TexCoord_ReadDirect_Short2:TexCoord_ReadDirect_Short1);  break;
			case FORMAT_FLOAT:	m_VertexSize += elements?8:4; WriteCall(elements?TexCoord_ReadDirect_Float2:TexCoord_ReadDirect_Float1);  break;
			default: _assert_(0); break;
			}
			break;
		case INDEX8:	
			m_VertexSize += 1;
			switch (m_VtxAttr.texCoord[i].Format)
			{
			case FORMAT_UBYTE:	WriteCall(elements?TexCoord_ReadIndex8_UByte2:TexCoord_ReadIndex8_UByte1);  break;
			case FORMAT_BYTE:	WriteCall(elements?TexCoord_ReadIndex8_Byte2:TexCoord_ReadIndex8_Byte1);   break;
			case FORMAT_USHORT:	WriteCall(elements?TexCoord_ReadIndex8_UShort2:TexCoord_ReadIndex8_UShort1); break;
			case FORMAT_SHORT:	WriteCall(elements?TexCoord_ReadIndex8_Short2:TexCoord_ReadIndex8_Short1);  break;
			case FORMAT_FLOAT:	WriteCall(elements?TexCoord_ReadIndex8_Float2:TexCoord_ReadIndex8_Float1);  break;
			default: _assert_(0); break;
			}
			break;
		case INDEX16:
			m_VertexSize += 2;
			switch (m_VtxAttr.texCoord[i].Format)
			{
			case FORMAT_UBYTE:	WriteCall(elements?TexCoord_ReadIndex16_UByte2:TexCoord_ReadIndex16_UByte1);  break;
			case FORMAT_BYTE:	WriteCall(elements?TexCoord_ReadIndex16_Byte2:TexCoord_ReadIndex16_Byte1);   break;
			case FORMAT_USHORT:	WriteCall(elements?TexCoord_ReadIndex16_UShort2:TexCoord_ReadIndex16_UShort1); break;
			case FORMAT_SHORT:	WriteCall(elements?TexCoord_ReadIndex16_Short2:TexCoord_ReadIndex16_Short1);  break;
			case FORMAT_FLOAT:	WriteCall(elements?TexCoord_ReadIndex16_Float2:TexCoord_ReadIndex16_Float1);  break;
			default: _assert_(0);
			}
			break;
		}

		if (m_NativeFmt->m_components & (VB_HAS_TEXMTXIDX0 << i)) {
			if (tc[i] != NOT_PRESENT) {
				// if texmtx is included, texcoord will always be 3 floats, z will be the texmtx index
				vtx_decl.texcoord_offset[i] = nat_offset;
				vtx_decl.texcoord_gl_type[i] = VAR_FLOAT;
				vtx_decl.texcoord_size[i] = 3;
				nat_offset += 12;
				WriteCall(m_VtxAttr.texCoord[i].Elements ? TexMtx_Write_Float : TexMtx_Write_Float2);
			}
			else {
				m_NativeFmt->m_components |= VB_HAS_UV0 << i; // have to include since using now
				vtx_decl.texcoord_offset[i] = nat_offset;
				vtx_decl.texcoord_gl_type[i] = VAR_SHORT;
				vtx_decl.texcoord_size[i] = 4;
				nat_offset += 8; // still include the texture coordinate, but this time as 6 + 2 bytes
				WriteCall(TexMtx_Write_Short3);
			}
		}
		else {
			if (tc[i] != NOT_PRESENT) {
				vtx_decl.texcoord_offset[i] = nat_offset;
				vtx_decl.texcoord_gl_type[i] = VAR_FLOAT;
				vtx_decl.texcoord_size[i] = vtx_attr.texCoord[i].Elements ? 2 : 1;
				nat_offset += 4 * (vtx_attr.texCoord[i].Elements ? 2 : 1);
			} else {
				vtx_decl.texcoord_offset[i] = -1;
			}
		}

		if (tc[i] == NOT_PRESENT) {
			// if there's more tex coords later, have to write a dummy call 
			int j = i + 1;
			for (; j < 8; ++j) {
				if (tc[j] != NOT_PRESENT) {
					WriteCall(TexCoord_Read_Dummy); // important to get indices right!
					break;
				}
			}
			// tricky!
			if (j == 8 && !((m_NativeFmt->m_components & VB_HAS_TEXMTXIDXALL) & (VB_HAS_TEXMTXIDXALL << (i + 1)))) {
				// no more tex coords and tex matrices, so exit loop
				break;
			}
		}
	}

	if (m_VtxDesc.PosMatIdx) {
		WriteCall(PosMtx_Write);
		vtx_decl.posmtx_offset = nat_offset;
		nat_offset += 4;
	} else {
		vtx_decl.posmtx_offset = -1;
	}

	native_stride = nat_offset;
	vtx_decl.stride = native_stride;

#ifdef USE_JIT
	// End loop here
	SUB(32, R(EBX), Imm8(1));
	J_CC(CC_NZ, loop_start, true);
	ABI_EmitEpilogue(4);
	SetCodePtr(old_code_ptr);
#endif
	m_NativeFmt->Initialize(vtx_decl);
}

void VertexLoader::WriteCall(TPipelineFunction func)
{
#ifdef USE_JIT
	CALL((void*)func);
#else
	m_PipelineStages[m_numPipelineStages++] = func;
#endif
}

void VertexLoader::RunVertices(int vtx_attr_group, int primitive, int count)
{
	DVSTARTPROFILE();

	m_numLoadedVertices += count;

	// Flush if our vertex format is different from the currently set.
	if (g_nativeVertexFmt != NULL && g_nativeVertexFmt != m_NativeFmt)
	{
		VertexManager::Flush();
		// Also move the Set() here?
	}
	g_nativeVertexFmt = m_NativeFmt;

	if (bpmem.genMode.cullmode == 3 && primitive < 5)
	{
		// if cull mode is none, ignore triangles and quads
		DataSkip(count * m_VertexSize);
		return;
	}

	VertexManager::EnableComponents(m_NativeFmt->m_components);

	// Load position and texcoord scale factors.
	// TODO - figure out if we should leave these independent, or compile them into
	// the vertexloaders.
	m_VtxAttr.PosFrac				= g_VtxAttr[vtx_attr_group].g0.PosFrac;
	m_VtxAttr.texCoord[0].Frac		= g_VtxAttr[vtx_attr_group].g0.Tex0Frac;
	m_VtxAttr.texCoord[1].Frac		= g_VtxAttr[vtx_attr_group].g1.Tex1Frac;
	m_VtxAttr.texCoord[2].Frac		= g_VtxAttr[vtx_attr_group].g1.Tex2Frac;
	m_VtxAttr.texCoord[3].Frac      = g_VtxAttr[vtx_attr_group].g1.Tex3Frac;
	m_VtxAttr.texCoord[4].Frac		= g_VtxAttr[vtx_attr_group].g2.Tex4Frac;
	m_VtxAttr.texCoord[5].Frac		= g_VtxAttr[vtx_attr_group].g2.Tex5Frac;
	m_VtxAttr.texCoord[6].Frac		= g_VtxAttr[vtx_attr_group].g2.Tex6Frac;
	m_VtxAttr.texCoord[7].Frac		= g_VtxAttr[vtx_attr_group].g2.Tex7Frac;

	pVtxAttr = &m_VtxAttr;
	posScale = shiftLookup[m_VtxAttr.PosFrac];
	if (m_NativeFmt->m_components & VB_HAS_UVALL) {
		for (int i = 0; i < 8; i++)
			tcScale[i] = shiftLookup[m_VtxAttr.texCoord[i].Frac];
	}
	for (int i = 0; i < 2; i++)
		colElements[i] = m_VtxAttr.color[i].Elements;

	// if strips or fans, make sure all vertices can fit in buffer, otherwise flush
	int granularity = 1;
	switch (primitive) {
		case 3: // strip .. hm, weird
		case 4: // fan
			if (VertexManager::GetRemainingSize() < 3 * native_stride)
				VertexManager::Flush();
			break;
		case 6: // line strip
			if (VertexManager::GetRemainingSize() < 2 * native_stride)
				VertexManager::Flush();
			break;
		case 0: granularity = 4; break; // quads
		case 2: granularity = 3; break; // tris
		case 5: granularity = 2; break; // lines
	}

	int startv = 0, extraverts = 0;
	int v = 0;

	while (v < count)
	{
		int remainingVerts = VertexManager::GetRemainingSize() / native_stride;
		if (remainingVerts < granularity) {
			INCSTAT(stats.thisFrame.numBufferSplits);
			// This buffer full - break current primitive and flush, to switch to the next buffer.
			u8* plastptr = VertexManager::s_pCurBufferPointer;
			if (v - startv > 0)
				VertexManager::AddVertices(primitive, v - startv + extraverts);
			VertexManager::Flush();
			// Why does this need to be so complicated?
			switch (primitive) {
				case 3: // triangle strip, copy last two vertices
					// a little trick since we have to keep track of signs
					if (v & 1) {
						memcpy_gc(VertexManager::s_pCurBufferPointer, plastptr-2*native_stride, native_stride);
						memcpy_gc(VertexManager::s_pCurBufferPointer+native_stride, plastptr-native_stride*2, 2*native_stride);
						VertexManager::s_pCurBufferPointer += native_stride*3;
						extraverts = 3;
					}
					else {
						memcpy_gc(VertexManager::s_pCurBufferPointer, plastptr-native_stride*2, native_stride*2);
						VertexManager::s_pCurBufferPointer += native_stride*2;
						extraverts = 2;
					}
					break;
				case 4: // tri fan, copy first and last vert
					memcpy_gc(VertexManager::s_pCurBufferPointer, plastptr-native_stride*(v-startv+extraverts), native_stride);
					VertexManager::s_pCurBufferPointer += native_stride;
					memcpy_gc(VertexManager::s_pCurBufferPointer, plastptr-native_stride, native_stride);
					VertexManager::s_pCurBufferPointer += native_stride;
					extraverts = 2;
					break;
				case 6: // line strip
					memcpy_gc(VertexManager::s_pCurBufferPointer, plastptr-native_stride, native_stride);
					VertexManager::s_pCurBufferPointer += native_stride;
					extraverts = 1;
					break;
				default:
					extraverts = 0;
					break;
			}
			startv = v;
		}
		int remainingPrims = remainingVerts / granularity;
		remainingVerts = remainingPrims * granularity;
		if (count - v < remainingVerts)
			remainingVerts = count - v;

		// Clean tight loader loop. Todo - build the loop into the JIT code.
	#ifdef USE_JIT
		if (remainingVerts > 0) {
			loop_counter = remainingVerts;
			((void (*)())(void*)m_compiledCode)();
		}
	#else
		for (int s = 0; s < remainingVerts; s++)
		{
			tcIndex = 0;
			colIndex = 0;
			s_texmtxwrite = s_texmtxread = 0;
			for (int i = 0; i < m_numPipelineStages; i++)
				m_PipelineStages[i]();
			PRIM_LOG("\n");
		}
	#endif
		v += remainingVerts;
	}

	if (startv < count)
		VertexManager::AddVertices(primitive, count - startv + extraverts);
}

void VertexLoader::SetVAT(u32 _group0, u32 _group1, u32 _group2) 
{
	VAT vat;
	vat.g0.Hex = _group0;
	vat.g1.Hex = _group1;
	vat.g2.Hex = _group2;

	m_VtxAttr.PosElements			= vat.g0.PosElements;
	m_VtxAttr.PosFormat				= vat.g0.PosFormat;
	m_VtxAttr.PosFrac				= vat.g0.PosFrac;
	m_VtxAttr.NormalElements		= vat.g0.NormalElements;
	m_VtxAttr.NormalFormat			= vat.g0.NormalFormat;
	m_VtxAttr.color[0].Elements		= vat.g0.Color0Elements;
	m_VtxAttr.color[0].Comp			= vat.g0.Color0Comp;
	m_VtxAttr.color[1].Elements		= vat.g0.Color1Elements;
	m_VtxAttr.color[1].Comp			= vat.g0.Color1Comp;
	m_VtxAttr.texCoord[0].Elements	= vat.g0.Tex0CoordElements;
	m_VtxAttr.texCoord[0].Format	= vat.g0.Tex0CoordFormat;
	m_VtxAttr.texCoord[0].Frac		= vat.g0.Tex0Frac;
	m_VtxAttr.ByteDequant			= vat.g0.ByteDequant;
	m_VtxAttr.NormalIndex3			= vat.g0.NormalIndex3;

	m_VtxAttr.texCoord[1].Elements	= vat.g1.Tex1CoordElements;
	m_VtxAttr.texCoord[1].Format	= vat.g1.Tex1CoordFormat;
	m_VtxAttr.texCoord[1].Frac		= vat.g1.Tex1Frac;
	m_VtxAttr.texCoord[2].Elements	= vat.g1.Tex2CoordElements;
	m_VtxAttr.texCoord[2].Format	= vat.g1.Tex2CoordFormat;
	m_VtxAttr.texCoord[2].Frac		= vat.g1.Tex2Frac;
	m_VtxAttr.texCoord[3].Elements	= vat.g1.Tex3CoordElements;
	m_VtxAttr.texCoord[3].Format	= vat.g1.Tex3CoordFormat;
	m_VtxAttr.texCoord[3].Frac      = vat.g1.Tex3Frac;
	m_VtxAttr.texCoord[4].Elements	= vat.g1.Tex4CoordElements;
	m_VtxAttr.texCoord[4].Format	= vat.g1.Tex4CoordFormat;

	m_VtxAttr.texCoord[4].Frac		= vat.g2.Tex4Frac;
	m_VtxAttr.texCoord[5].Elements	= vat.g2.Tex5CoordElements;
	m_VtxAttr.texCoord[5].Format	= vat.g2.Tex5CoordFormat;
	m_VtxAttr.texCoord[5].Frac		= vat.g2.Tex5Frac;
	m_VtxAttr.texCoord[6].Elements	= vat.g2.Tex6CoordElements;
	m_VtxAttr.texCoord[6].Format	= vat.g2.Tex6CoordFormat;
	m_VtxAttr.texCoord[6].Frac		= vat.g2.Tex6Frac;
	m_VtxAttr.texCoord[7].Elements	= vat.g2.Tex7CoordElements;
	m_VtxAttr.texCoord[7].Format	= vat.g2.Tex7CoordFormat;
	m_VtxAttr.texCoord[7].Frac		= vat.g2.Tex7Frac;
};

void VertexLoader::AppendToString(std::string *dest) {
	static const char *posMode[4] = {
		"Invalid",
		"Direct",
		"Idx8",
		"Idx16",
	};
	static const char *posFormats[5] = {
		"u8", "s8", "u16", "s16", "flt",
	};
	dest->append(StringFromFormat("sz: %i skin: %i Pos: %i %s %s Nrm: %i %s %s - %i vtx\n",
		m_VertexSize, m_VtxDesc.PosMatIdx, m_VtxAttr.PosElements ? 3 : 2, posMode[m_VtxDesc.Position], posFormats[m_VtxAttr.PosFormat],
		m_VtxAttr.NormalElements, posMode[m_VtxDesc.Normal], posFormats[m_VtxAttr.NormalFormat], m_numLoadedVertices));
}