// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <assert.h>

#include "Common.h"
#include "VideoCommon.h"
#include "VideoConfig.h"
#include "MemoryUtil.h"
#include "StringUtil.h"
#include "x64Emitter.h"
#include "x64ABI.h"
#include "PixelEngine.h"
#include "Host.h"

#include "LookUpTables.h"
#include "Statistics.h"
#include "VertexLoaderManager.h"
#include "VertexLoader.h"
#include "BPMemory.h"
#include "DataReader.h"
#include "VertexManagerBase.h"

#include "VertexLoader_Position.h"
#include "VertexLoader_Normal.h"
#include "VertexLoader_Color.h"
#include "VertexLoader_TextCoord.h"

//BBox
#include "XFMemory.h"
extern float GC_ALIGNED16(g_fProjectionMatrix[16]);
#ifndef _M_GENERIC
#ifndef __APPLE__
#define USE_JIT
#endif
#endif

#define COMPILED_CODE_SIZE 4096

NativeVertexFormat *g_nativeVertexFmt;

#ifndef _WIN32
	#undef inline
	#define inline
#endif

// Matrix components are first in GC format but later in PC format - we need to store it temporarily
// when decoding each vertex.
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

// bbox must read vertex position, so convert it to this buffer
static float s_bbox_vertex_buffer[3];
static u8 *s_bbox_pCurBufferPointer_orig;

static const float fractionTable[32] = {
	1.0f / (1U << 0), 1.0f / (1U << 1), 1.0f / (1U << 2), 1.0f / (1U << 3),
	1.0f / (1U << 4), 1.0f / (1U << 5), 1.0f / (1U << 6), 1.0f / (1U << 7),
	1.0f / (1U << 8), 1.0f / (1U << 9), 1.0f / (1U << 10), 1.0f / (1U << 11),
	1.0f / (1U << 12), 1.0f / (1U << 13), 1.0f / (1U << 14), 1.0f / (1U << 15),
	1.0f / (1U << 16), 1.0f / (1U << 17), 1.0f / (1U << 18), 1.0f / (1U << 19),
	1.0f / (1U << 20), 1.0f / (1U << 21), 1.0f / (1U << 22), 1.0f / (1U << 23),
	1.0f / (1U << 24), 1.0f / (1U << 25), 1.0f / (1U << 26), 1.0f / (1U << 27),
	1.0f / (1U << 28), 1.0f / (1U << 29), 1.0f / (1U << 30), 1.0f / (1U << 31),
};

using namespace Gen;

void LOADERDECL PosMtx_ReadDirect_UByte()
{
	s_curposmtx = DataReadU8() & 0x3f;
	PRIM_LOG("posmtx: %d, ", s_curposmtx);
}

void LOADERDECL PosMtx_Write()
{
	DataWrite<u8>(s_curposmtx);
	DataWrite<u8>(0);
	DataWrite<u8>(0);
	DataWrite<u8>(0);
}

void LOADERDECL UpdateBoundingBoxPrepare() 
{
	if (!PixelEngine::bbox_active)
		return;
	
	// set our buffer as videodata buffer, so we will get a copy of the vertex positions
	// this is a big hack, but so we can use the same converting function then without bbox
	s_bbox_pCurBufferPointer_orig = VertexManager::s_pCurBufferPointer;
	VertexManager::s_pCurBufferPointer = (u8*)s_bbox_vertex_buffer;
}

void LOADERDECL UpdateBoundingBox() 
{
	if (!PixelEngine::bbox_active)
		return;

	// reset videodata pointer
	VertexManager::s_pCurBufferPointer = s_bbox_pCurBufferPointer_orig;

	// copy vertex pointers
	memcpy(VertexManager::s_pCurBufferPointer, s_bbox_vertex_buffer, 12);
	VertexManager::s_pCurBufferPointer += 12;

	// We must transform the just loaded point by the current world and projection matrix - in software.
	// Then convert to screen space and update the bounding box.
	float p[3] = {s_bbox_vertex_buffer[0], s_bbox_vertex_buffer[1], s_bbox_vertex_buffer[2]};

	const float *world_matrix  = (float*)xfmem + MatrixIndexA.PosNormalMtxIdx * 4;
	const float *proj_matrix = &g_fProjectionMatrix[0];

	float t[3];
	t[0] = p[0] * world_matrix[0] + p[1] * world_matrix[1] + p[2] * world_matrix[2] + world_matrix[3];
	t[1] = p[0] * world_matrix[4] + p[1] * world_matrix[5] + p[2] * world_matrix[6] + world_matrix[7];
	t[2] = p[0] * world_matrix[8] + p[1] * world_matrix[9] + p[2] * world_matrix[10] + world_matrix[11];

	float o[3];
	o[0] = t[0] * proj_matrix[0]  + t[1] * proj_matrix[1]  + t[2] * proj_matrix[2] + proj_matrix[3];
	o[1] = t[0] * proj_matrix[4]  + t[1] * proj_matrix[5]  + t[2] * proj_matrix[6] + proj_matrix[7];
	o[2] = t[0] * proj_matrix[12] + t[1] * proj_matrix[13] + t[2] * proj_matrix[14] + proj_matrix[15];

	o[0] /= o[2];
	o[1] /= o[2];

	// Max width seems to be 608, while max height is 480
	// Here height is set to 484 as BBox bottom always seems to be off by a few pixels
	o[0] = (o[0] + 1.0f) * 304.0f;
	o[1] = (1.0f - o[1]) * 242.0f;

	if (o[0] < PixelEngine::bbox[0]) PixelEngine::bbox[0] = (u16) std::max(0.0f, o[0]);
	if (o[0] > PixelEngine::bbox[1]) PixelEngine::bbox[1] = (u16) o[0]; 
	if (o[1] < PixelEngine::bbox[2]) PixelEngine::bbox[2] = (u16) std::max(0.0f, o[1]);
	if (o[1] > PixelEngine::bbox[3]) PixelEngine::bbox[3] = (u16) o[1];
}

void LOADERDECL TexMtx_ReadDirect_UByte()
{
	s_curtexmtx[s_texmtxread] = DataReadU8() & 0x3f;
	PRIM_LOG("texmtx%d: %d, ", s_texmtxread, s_curtexmtx[s_texmtxread]);
	s_texmtxread++;
}

void LOADERDECL TexMtx_Write_Float()
{
	DataWrite(float(s_curtexmtx[s_texmtxwrite++]));
}

void LOADERDECL TexMtx_Write_Float2()
{
	DataWrite(0.f);
	DataWrite(float(s_curtexmtx[s_texmtxwrite++]));
}

void LOADERDECL TexMtx_Write_Float4()
{
	DataWrite(0.f);
	DataWrite(0.f);
	DataWrite(float(s_curtexmtx[s_texmtxwrite++]));
	// Just to fill out with 0.
	DataWrite(0.f);
}

VertexLoader::VertexLoader(const TVtxDesc &vtx_desc, const VAT &vtx_attr) 
{
	m_compiledCode = NULL;
	m_numLoadedVertices = 0;
	m_VertexSize = 0;
	m_numPipelineStages = 0;
	m_NativeFmt = g_vertex_manager->CreateNativeVertexFormat();
	loop_counter = 0;
	VertexLoader_Normal::Init();
	VertexLoader_Position::Init();
	VertexLoader_TextCoord::Init();

	m_VtxDesc = vtx_desc;
	SetVAT(vtx_attr.g0.Hex, vtx_attr.g1.Hex, vtx_attr.g2.Hex);

	#ifdef USE_JIT
	AllocCodeSpace(COMPILED_CODE_SIZE);
	CompileVertexTranslator();
	WriteProtect();
	#else
	CompileVertexTranslator();
	#endif

}

VertexLoader::~VertexLoader() 
{
	#ifdef USE_JIT
	FreeCodeSpace();
	#endif
	delete m_NativeFmt;
}

void VertexLoader::CompileVertexTranslator()
{
	m_VertexSize = 0;
	const TVtxAttr &vtx_attr = m_VtxAttr;

#ifdef USE_JIT
	if (m_compiledCode)
		PanicAlert("Trying to recompile a vertex translator");

	m_compiledCode = GetCodePtr();
	ABI_EmitPrologue(4);

	// Start loop here
	const u8 *loop_start = GetCodePtr();

	// Reset component counters if present in vertex format only.
	if (m_VtxDesc.Tex0Coord || m_VtxDesc.Tex1Coord || m_VtxDesc.Tex2Coord || m_VtxDesc.Tex3Coord ||
		m_VtxDesc.Tex4Coord || m_VtxDesc.Tex5Coord || m_VtxDesc.Tex6Coord || m_VtxDesc.Tex7Coord)
	{
		WriteSetVariable(32, &tcIndex, Imm32(0));
	}
	if (m_VtxDesc.Color0 || m_VtxDesc.Color1)
	{
		WriteSetVariable(32, &colIndex, Imm32(0));
	}
	if (m_VtxDesc.Tex0MatIdx || m_VtxDesc.Tex1MatIdx || m_VtxDesc.Tex2MatIdx || m_VtxDesc.Tex3MatIdx ||
		m_VtxDesc.Tex4MatIdx || m_VtxDesc.Tex5MatIdx || m_VtxDesc.Tex6MatIdx || m_VtxDesc.Tex7MatIdx)
	{
		WriteSetVariable(32, &s_texmtxwrite, Imm32(0));
		WriteSetVariable(32, &s_texmtxread, Imm32(0));
	}
#endif

	// Colors
	const u32 col[2] = {m_VtxDesc.Color0, m_VtxDesc.Color1};
	// TextureCoord
	// Since m_VtxDesc.Text7Coord is broken across a 32 bit word boundary, retrieve its value manually.
	// If we didn't do this, the vertex format would be read as one bit offset from where it should be, making
	// 01 become 00, and 10/11 become 01
	const u32 tc[8] = {
		m_VtxDesc.Tex0Coord, m_VtxDesc.Tex1Coord, m_VtxDesc.Tex2Coord, m_VtxDesc.Tex3Coord,
		m_VtxDesc.Tex4Coord, m_VtxDesc.Tex5Coord, m_VtxDesc.Tex6Coord, (const u32)((m_VtxDesc.Hex >> 31) & 3)
	};
	
	// Reset pipeline
	m_numPipelineStages = 0;

	// It's a bit ugly that we poke inside m_NativeFmt in this function. Planning to fix this.
	m_NativeFmt->m_components = 0;

	// Position in pc vertex format.
	int nat_offset = 0;
	PortableVertexDeclaration vtx_decl;
	memset(&vtx_decl, 0, sizeof(vtx_decl));
	for (int i = 0; i < 8; i++)
	{
		vtx_decl.texcoord_offset[i] = -1;
	}

	// m_VBVertexStride for texmtx and posmtx is computed later when writing.
	
	// Position Matrix Index
	if (m_VtxDesc.PosMatIdx)
	{
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

	// Write vertex position loader
	if(g_ActiveConfig.bUseBBox)
	{
		WriteCall(UpdateBoundingBoxPrepare);
		WriteCall(VertexLoader_Position::GetFunction(m_VtxDesc.Position, m_VtxAttr.PosFormat, m_VtxAttr.PosElements));
		WriteCall(UpdateBoundingBox);
	}
	else
	{
		WriteCall(VertexLoader_Position::GetFunction(m_VtxDesc.Position, m_VtxAttr.PosFormat, m_VtxAttr.PosElements));
	}
	m_VertexSize += VertexLoader_Position::GetSize(m_VtxDesc.Position, m_VtxAttr.PosFormat, m_VtxAttr.PosElements);
	nat_offset += 12;

	// Normals
	vtx_decl.num_normals = 0;
	if (m_VtxDesc.Normal != NOT_PRESENT)
	{
		m_VertexSize += VertexLoader_Normal::GetSize(m_VtxDesc.Normal,
			m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);
		
		TPipelineFunction pFunc = VertexLoader_Normal::GetFunction(m_VtxDesc.Normal,
			m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);

		if (pFunc == 0)
		{
			char temp[256];
			sprintf(temp,"%i %i %i %i", m_VtxDesc.Normal, m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);
			Host_SysMessage("VertexLoader_Normal::GetFunction returned zero!");
		}
		WriteCall(pFunc);

		vtx_decl.num_normals = vtx_attr.NormalElements ? 3 : 1;
		vtx_decl.normal_offset[0] = -1;
		vtx_decl.normal_offset[1] = -1;
		vtx_decl.normal_offset[2] = -1;
		vtx_decl.normal_gl_type = VAR_FLOAT;
		vtx_decl.normal_gl_size = 3;
		vtx_decl.normal_offset[0] = nat_offset;
		nat_offset += 12;

		if (vtx_attr.NormalElements)
		{
			vtx_decl.normal_offset[1] = nat_offset;
			nat_offset += 12;
			vtx_decl.normal_offset[2] = nat_offset;
			nat_offset += 12;
		}

		int numNormals = (m_VtxAttr.NormalElements == 1) ? NRM_THREE : NRM_ONE;
		m_NativeFmt->m_components |= VB_HAS_NRM0;

		if (numNormals == NRM_THREE)
			m_NativeFmt->m_components |= VB_HAS_NRM1 | VB_HAS_NRM2;
	}

	vtx_decl.color_gl_type = VAR_UNSIGNED_BYTE;
	vtx_decl.color_offset[0] = -1;
	vtx_decl.color_offset[1] = -1;
	for (int i = 0; i < 2; i++)
	{
		m_NativeFmt->m_components |= VB_HAS_COL0 << i;
		switch (col[i])
		{
		case NOT_PRESENT: 
			m_NativeFmt->m_components &= ~(VB_HAS_COL0 << i);
			vtx_decl.color_offset[i] = -1;
			break;
		case DIRECT:
			switch (m_VtxAttr.color[i].Comp)
			{
			case FORMAT_16B_565:	m_VertexSize += 2; WriteCall(Color_ReadDirect_16b_565); break;
			case FORMAT_24B_888:	m_VertexSize += 3; WriteCall(Color_ReadDirect_24b_888); break;
			case FORMAT_32B_888x:	m_VertexSize += 4; WriteCall(Color_ReadDirect_32b_888x); break;
			case FORMAT_16B_4444:	m_VertexSize += 2; WriteCall(Color_ReadDirect_16b_4444); break;
			case FORMAT_24B_6666:	m_VertexSize += 3; WriteCall(Color_ReadDirect_24b_6666); break;
			case FORMAT_32B_8888:	m_VertexSize += 4; WriteCall(Color_ReadDirect_32b_8888); break;
			default: _assert_(0); break;
			}
			break;
		case INDEX8:
			m_VertexSize += 1;
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
			break;
		case INDEX16:
			m_VertexSize += 2;
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
			break;
		}
		// Common for the three bottom cases
		if (col[i] != NOT_PRESENT)
		{
			vtx_decl.color_offset[i] = nat_offset;
			nat_offset += 4;
		}
	}

	// Texture matrix indices (remove if corresponding texture coordinate isn't enabled)
	for (int i = 0; i < 8; i++)
	{
		vtx_decl.texcoord_offset[i] = -1;
		const int format = m_VtxAttr.texCoord[i].Format;
		const int elements = m_VtxAttr.texCoord[i].Elements;

		if (tc[i] == NOT_PRESENT)
		{
			m_NativeFmt->m_components &= ~(VB_HAS_UV0 << i);
		}
		else
		{
			_assert_msg_(VIDEO, DIRECT <= tc[i] && tc[i] <= INDEX16, "Invalid texture coordinates!\n(tc[i] = %d)", tc[i]);
			_assert_msg_(VIDEO, FORMAT_UBYTE <= format && format <= FORMAT_FLOAT, "Invalid texture coordinates format!\n(format = %d)", format);
			_assert_msg_(VIDEO, 0 <= elements && elements <= 1, "Invalid number of texture coordinates elements!\n(elements = %d)", elements);

			m_NativeFmt->m_components |= VB_HAS_UV0 << i;
			WriteCall(VertexLoader_TextCoord::GetFunction(tc[i], format, elements));
			m_VertexSize += VertexLoader_TextCoord::GetSize(tc[i], format, elements);
		}

		if (m_NativeFmt->m_components & (VB_HAS_TEXMTXIDX0 << i))
		{
			if (tc[i] != NOT_PRESENT)
			{
				// if texmtx is included, texcoord will always be 3 floats, z will be the texmtx index
				vtx_decl.texcoord_offset[i] = nat_offset;
				vtx_decl.texcoord_gl_type[i] = VAR_FLOAT;
				vtx_decl.texcoord_size[i] = 3;
				nat_offset += 12;
				WriteCall(m_VtxAttr.texCoord[i].Elements ? TexMtx_Write_Float : TexMtx_Write_Float2);
			}
			else
			{
				m_NativeFmt->m_components |= VB_HAS_UV0 << i; // have to include since using now
				vtx_decl.texcoord_offset[i] = nat_offset;
				vtx_decl.texcoord_gl_type[i] = VAR_FLOAT;
				vtx_decl.texcoord_size[i] = 4;
				nat_offset += 16; // still include the texture coordinate, but this time as 6 + 2 bytes
				WriteCall(TexMtx_Write_Float4);
			}
		}
		else
		{
			if (tc[i] != NOT_PRESENT)
			{
				vtx_decl.texcoord_offset[i] = nat_offset;
				vtx_decl.texcoord_gl_type[i] = VAR_FLOAT;
				vtx_decl.texcoord_size[i] = vtx_attr.texCoord[i].Elements ? 2 : 1;
				nat_offset += 4 * (vtx_attr.texCoord[i].Elements ? 2 : 1);
			}
		}

		if (tc[i] == NOT_PRESENT)
		{
			// if there's more tex coords later, have to write a dummy call 
			int j = i + 1;
			for (; j < 8; ++j)
			{
				if (tc[j] != NOT_PRESENT)
				{
					WriteCall(VertexLoader_TextCoord::GetDummyFunction()); // important to get indices right!
					break;
				}
			}
			// tricky!
			if (j == 8 && !((m_NativeFmt->m_components & VB_HAS_TEXMTXIDXALL) & (VB_HAS_TEXMTXIDXALL << (i + 1))))
			{
				// no more tex coords and tex matrices, so exit loop
				break;
			}
		}
	}

	if (m_VtxDesc.PosMatIdx)
	{
		WriteCall(PosMtx_Write);
		vtx_decl.posmtx_offset = nat_offset;
		nat_offset += 4;
	}
	else
	{
		vtx_decl.posmtx_offset = -1;
	}

	native_stride = nat_offset;
	vtx_decl.stride = native_stride;

#ifdef USE_JIT
	// End loop here
#ifdef _M_X64
	MOV(64, R(RAX), Imm64((u64)&loop_counter));
	SUB(32, MatR(RAX), Imm8(1));
#else
	SUB(32, M(&loop_counter), Imm8(1));
#endif

	J_CC(CC_NZ, loop_start, true);
	ABI_EmitEpilogue(4);
#endif
	m_NativeFmt->Initialize(vtx_decl);
}

void VertexLoader::WriteCall(TPipelineFunction func)
{
#ifdef USE_JIT
#ifdef _M_X64
	MOV(64, R(RAX), Imm64((u64)func));
	CALLptr(R(RAX));
#else
	CALL((void*)func);
#endif
#else
	m_PipelineStages[m_numPipelineStages++] = func;
#endif
}
// ARMTODO: This should be done in a better way
#ifndef _M_GENERIC
void VertexLoader::WriteGetVariable(int bits, OpArg dest, void *address)
{
#ifdef USE_JIT
#ifdef _M_X64
	MOV(64, R(RAX), Imm64((u64)address));
	MOV(bits, dest, MatR(RAX));
#else
	MOV(bits, dest, M(address));
#endif
#endif
}

void VertexLoader::WriteSetVariable(int bits, void *address, OpArg value)
{
#ifdef USE_JIT
#ifdef _M_X64
	MOV(64, R(RAX), Imm64((u64)address));
	MOV(bits, MatR(RAX), value);
#else
	MOV(bits, M(address), value);
#endif
#endif
}
#endif

int VertexLoader::SetupRunVertices(int vtx_attr_group, int primitive, int const count)
{
	m_numLoadedVertices += count;

	// Flush if our vertex format is different from the currently set.
	if (g_nativeVertexFmt != NULL && g_nativeVertexFmt != m_NativeFmt)
	{
		// We really must flush here. It's possible that the native representations
		// of the two vtx formats are the same, but we have no way to easily check that 
		// now. 
		VertexManager::Flush();
		// Also move the Set() here?
	}
	g_nativeVertexFmt = m_NativeFmt;

	if (bpmem.genMode.cullmode == 3 && primitive < 5)
	{
		// if cull mode is none, ignore triangles and quads
		DataSkip(count * m_VertexSize);
		return 0;
	}

	m_NativeFmt->EnableComponents(m_NativeFmt->m_components);

	// Load position and texcoord scale factors.
	m_VtxAttr.PosFrac				= g_VtxAttr[vtx_attr_group].g0.PosFrac;
	m_VtxAttr.texCoord[0].Frac		= g_VtxAttr[vtx_attr_group].g0.Tex0Frac;
	m_VtxAttr.texCoord[1].Frac		= g_VtxAttr[vtx_attr_group].g1.Tex1Frac;
	m_VtxAttr.texCoord[2].Frac		= g_VtxAttr[vtx_attr_group].g1.Tex2Frac;
	m_VtxAttr.texCoord[3].Frac		= g_VtxAttr[vtx_attr_group].g1.Tex3Frac;
	m_VtxAttr.texCoord[4].Frac		= g_VtxAttr[vtx_attr_group].g2.Tex4Frac;
	m_VtxAttr.texCoord[5].Frac		= g_VtxAttr[vtx_attr_group].g2.Tex5Frac;
	m_VtxAttr.texCoord[6].Frac		= g_VtxAttr[vtx_attr_group].g2.Tex6Frac;
	m_VtxAttr.texCoord[7].Frac		= g_VtxAttr[vtx_attr_group].g2.Tex7Frac;

	pVtxAttr = &m_VtxAttr;
	posScale = fractionTable[m_VtxAttr.PosFrac];
	if (m_NativeFmt->m_components & VB_HAS_UVALL)
		for (int i = 0; i < 8; i++)
			tcScale[i] = fractionTable[m_VtxAttr.texCoord[i].Frac];
	for (int i = 0; i < 2; i++)
		colElements[i] = m_VtxAttr.color[i].Elements;

	VertexManager::PrepareForAdditionalData(primitive, count, native_stride);
	
	return count;
}

void VertexLoader::RunVertices(int vtx_attr_group, int primitive, int const count)
{
	auto const new_count = SetupRunVertices(vtx_attr_group, primitive, count);
	ConvertVertices(new_count);
	VertexManager::AddVertices(primitive, new_count);
}

void VertexLoader::ConvertVertices ( int count )
{
#ifdef USE_JIT
	if (count > 0)
	{
		loop_counter = count;
		((void (*)())(void*)m_compiledCode)();
	}
#else
	for (int s = 0; s < count; s++)
	{
		tcIndex = 0;
		colIndex = 0;
		s_texmtxwrite = s_texmtxread = 0;
		for (int i = 0; i < m_numPipelineStages; i++)
			m_PipelineStages[i]();
		PRIM_LOG("\n");
	}
#endif
}

void VertexLoader::RunCompiledVertices(int vtx_attr_group, int primitive, int const count, u8* Data)
{
	auto const new_count = SetupRunVertices(vtx_attr_group, primitive, count);

	memcpy_gc(VertexManager::s_pCurBufferPointer, Data, native_stride * new_count);
	VertexManager::s_pCurBufferPointer += native_stride * new_count;
	DataSkip(new_count * m_VertexSize);

	VertexManager::AddVertices(primitive, new_count);	
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
	m_VtxAttr.texCoord[3].Frac		= vat.g1.Tex3Frac;
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
	
	if(!m_VtxAttr.ByteDequant) {
		ERROR_LOG(VIDEO, "ByteDequant is set to zero");
	}
};

void VertexLoader::AppendToString(std::string *dest) const
{
	dest->reserve(250);
	static const char *posMode[4] = {
		"Inv",
		"Dir",
		"I8",
		"I16",
	};
	static const char *posFormats[5] = {
		"u8", "s8", "u16", "s16", "flt",
	};
	static const char *colorFormat[8] = {
		"565",
		"888",
		"888x",
		"4444",
		"6666",
		"8888",
		"Inv",
		"Inv",
	};

	dest->append(StringFromFormat("%ib skin: %i P: %i %s-%s ",
		m_VertexSize, m_VtxDesc.PosMatIdx,
		m_VtxAttr.PosElements ? 3 : 2, posMode[m_VtxDesc.Position], posFormats[m_VtxAttr.PosFormat]));

	if (m_VtxDesc.Normal)
	{
		dest->append(StringFromFormat("Nrm: %i %s-%s ",
			m_VtxAttr.NormalElements, posMode[m_VtxDesc.Normal], posFormats[m_VtxAttr.NormalFormat]));
	}

	u32 color_mode[2] = {m_VtxDesc.Color0, m_VtxDesc.Color1};
	for (int i = 0; i < 2; i++)
	{
		if (color_mode[i])
		{
			dest->append(StringFromFormat("C%i: %i %s-%s ", i, m_VtxAttr.color[i].Elements, posMode[color_mode[i]], colorFormat[m_VtxAttr.color[i].Comp]));
		}
	}
	u32 tex_mode[8] = {
		m_VtxDesc.Tex0Coord, m_VtxDesc.Tex1Coord, m_VtxDesc.Tex2Coord, m_VtxDesc.Tex3Coord, 
		m_VtxDesc.Tex4Coord, m_VtxDesc.Tex5Coord, m_VtxDesc.Tex6Coord, m_VtxDesc.Tex7Coord
	};
	for (int i = 0; i < 8; i++)
	{
		if (tex_mode[i])
		{
			dest->append(StringFromFormat("T%i: %i %s-%s ",
				i, m_VtxAttr.texCoord[i].Elements, posMode[tex_mode[i]], posFormats[m_VtxAttr.texCoord[i].Format]));
		}
	}
	dest->append(StringFromFormat(" - %i v\n", m_numLoadedVertices));
}
