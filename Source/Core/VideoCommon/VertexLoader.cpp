// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/MemoryUtil.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"

#include "Core/Host.h"

#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/DataReader.h"
#include "VideoCommon/LookUpTables.h"
#include "VideoCommon/PixelEngine.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoader_Color.h"
#include "VideoCommon/VertexLoader_Normal.h"
#include "VideoCommon/VertexLoader_Position.h"
#include "VideoCommon/VertexLoader_TextCoord.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"


#define COMPILED_CODE_SIZE 4096

#ifndef _WIN32
	#undef inline
	#define inline
#endif

// This pointer is used as the source/dst for all fixed function loader calls
u8* g_video_buffer_read_ptr;
u8* g_vertex_manager_write_ptr;

using namespace Gen;


void* VertexLoader::operator new (size_t size)
{
	return AllocateAlignedMemory(size, 16);
}

void VertexLoader::operator delete (void *p)
{
	FreeAlignedMemory(p);
}

static void LOADERDECL PosMtx_ReadDirect_UByte(VertexLoader* loader)
{
	BoundingBox::posMtxIdx = loader->m_curposmtx = DataReadU8() & 0x3f;
	PRIM_LOG("posmtx: %d, ", loader->m_curposmtx);
}

static void LOADERDECL PosMtx_Write(VertexLoader* loader)
{
	// u8, 0, 0, 0
	DataWrite<u32>(loader->m_curposmtx);
}

static void LOADERDECL TexMtx_ReadDirect_UByte(VertexLoader* loader)
{
	BoundingBox::texMtxIdx[loader->m_texmtxread] = loader->m_curtexmtx[loader->m_texmtxread] = DataReadU8() & 0x3f;

	PRIM_LOG("texmtx%d: %d, ", loader->m_texmtxread, loader->m_curtexmtx[loader->m_texmtxread]);
	loader->m_texmtxread++;
}

static void LOADERDECL TexMtx_Write_Float(VertexLoader* loader)
{
	DataWrite(float(loader->m_curtexmtx[loader->m_texmtxwrite++]));
}

static void LOADERDECL TexMtx_Write_Float2(VertexLoader* loader)
{
	DataWrite(0.f);
	DataWrite(float(loader->m_curtexmtx[loader->m_texmtxwrite++]));
}

static void LOADERDECL TexMtx_Write_Float4(VertexLoader* loader)
{
#if _M_SSE >= 0x200
	__m128 output = _mm_cvtsi32_ss(_mm_castsi128_ps(_mm_setzero_si128()), loader->m_curtexmtx[loader->m_texmtxwrite++]);
	_mm_storeu_ps((float*)g_vertex_manager_write_ptr, _mm_shuffle_ps(output, output, 0x45 /* 1, 1, 0, 1 */));
	g_vertex_manager_write_ptr += sizeof(float) * 4;
#else
	DataWrite(0.f);
	DataWrite(0.f);
	DataWrite(float(loader->m_curtexmtx[loader->m_texmtxwrite++]));
	// Just to fill out with 0.
	DataWrite(0.f);
#endif
}

static void LOADERDECL SkipVertex(VertexLoader* loader)
{
	if (loader->m_vertexSkip)
	{
		// reset the output buffer
		g_vertex_manager_write_ptr -= loader->m_native_vtx_decl.stride;

		loader->m_skippedVertices++;
	}
}

VertexLoader::VertexLoader(const TVtxDesc &vtx_desc, const VAT &vtx_attr)
: VertexLoaderBase(vtx_desc, vtx_attr)
{
	m_compiledCode = nullptr;
	VertexLoader_Normal::Init();
	VertexLoader_Position::Init();
	VertexLoader_TextCoord::Init();

	#ifdef USE_VERTEX_LOADER_JIT
	AllocCodeSpace(COMPILED_CODE_SIZE);
	CompileVertexTranslator();
	WriteProtect();
	#else
	m_numPipelineStages = 0;
	CompileVertexTranslator();
	#endif

	// generate frac factors
	m_posScale[0] = m_posScale[1] = m_posScale[2] = m_posScale[3] = 1.0f / (1U << m_VtxAttr.PosFrac);
	for (int i = 0; i < 8; i++)
		m_tcScale[i][0] = m_tcScale[i][1] = 1.0f / (1U << m_VtxAttr.texCoord[i].Frac);

	for (int i = 0; i < 2; i++)
		m_colElements[i] = m_VtxAttr.color[i].Elements;
}

VertexLoader::~VertexLoader()
{
	#ifdef USE_VERTEX_LOADER_JIT
	FreeCodeSpace();
	#endif
}

void VertexLoader::CompileVertexTranslator()
{
	m_VertexSize = 0;
	const TVtxAttr &vtx_attr = m_VtxAttr;

#ifdef USE_VERTEX_LOADER_JIT
	if (m_compiledCode)
		PanicAlert("Trying to recompile a vertex translator");

	m_compiledCode = GetCodePtr();
	// We only use RAX (caller saved) and RBX (callee saved).
	ABI_PushRegistersAndAdjustStack({RBX, RBP}, 8);

	// save count
	MOV(64, R(RBX), R(ABI_PARAM1));

	// save loader
	MOV(64, R(RBP), R(ABI_PARAM2));

	// Start loop here
	const u8 *loop_start = GetCodePtr();

	// Reset component counters if present in vertex format only.
	if (m_VtxDesc.Tex0Coord || m_VtxDesc.Tex1Coord || m_VtxDesc.Tex2Coord || m_VtxDesc.Tex3Coord ||
		m_VtxDesc.Tex4Coord || m_VtxDesc.Tex5Coord || m_VtxDesc.Tex6Coord || m_VtxDesc.Tex7Coord)
	{
		WriteSetVariable(32, &m_tcIndex, Imm32(0));
	}
	if (m_VtxDesc.Color0 || m_VtxDesc.Color1)
	{
		WriteSetVariable(32, &m_colIndex, Imm32(0));
	}
	if (m_VtxDesc.Tex0MatIdx || m_VtxDesc.Tex1MatIdx || m_VtxDesc.Tex2MatIdx || m_VtxDesc.Tex3MatIdx ||
		m_VtxDesc.Tex4MatIdx || m_VtxDesc.Tex5MatIdx || m_VtxDesc.Tex6MatIdx || m_VtxDesc.Tex7MatIdx)
	{
		WriteSetVariable(32, &m_texmtxwrite, Imm32(0));
		WriteSetVariable(32, &m_texmtxread, Imm32(0));
	}
#else
	// Reset pipeline
	m_numPipelineStages = 0;
#endif

	// Get the pointer to this vertex's buffer data for the bounding box
	if (!g_ActiveConfig.backend_info.bSupportsBBox)
		WriteCall(BoundingBox::SetVertexBufferPosition);

	// Colors
	const u64 col[2] = {m_VtxDesc.Color0, m_VtxDesc.Color1};
	// TextureCoord
	const u64 tc[8] = {
		m_VtxDesc.Tex0Coord, m_VtxDesc.Tex1Coord, m_VtxDesc.Tex2Coord, m_VtxDesc.Tex3Coord,
		m_VtxDesc.Tex4Coord, m_VtxDesc.Tex5Coord, m_VtxDesc.Tex6Coord, m_VtxDesc.Tex7Coord
	};

	u32 components = 0;

	// Position in pc vertex format.
	int nat_offset = 0;
	memset(&m_native_vtx_decl, 0, sizeof(m_native_vtx_decl));

	// Position Matrix Index
	if (m_VtxDesc.PosMatIdx)
	{
		WriteCall(PosMtx_ReadDirect_UByte);
		components |= VB_HAS_POSMTXIDX;
		m_VertexSize += 1;
	}

	if (m_VtxDesc.Tex0MatIdx) {m_VertexSize += 1; components |= VB_HAS_TEXMTXIDX0; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex1MatIdx) {m_VertexSize += 1; components |= VB_HAS_TEXMTXIDX1; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex2MatIdx) {m_VertexSize += 1; components |= VB_HAS_TEXMTXIDX2; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex3MatIdx) {m_VertexSize += 1; components |= VB_HAS_TEXMTXIDX3; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex4MatIdx) {m_VertexSize += 1; components |= VB_HAS_TEXMTXIDX4; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex5MatIdx) {m_VertexSize += 1; components |= VB_HAS_TEXMTXIDX5; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex6MatIdx) {m_VertexSize += 1; components |= VB_HAS_TEXMTXIDX6; WriteCall(TexMtx_ReadDirect_UByte); }
	if (m_VtxDesc.Tex7MatIdx) {m_VertexSize += 1; components |= VB_HAS_TEXMTXIDX7; WriteCall(TexMtx_ReadDirect_UByte); }

	// Write vertex position loader
	WriteCall(VertexLoader_Position::GetFunction(m_VtxDesc.Position, m_VtxAttr.PosFormat, m_VtxAttr.PosElements));

	m_VertexSize += VertexLoader_Position::GetSize(m_VtxDesc.Position, m_VtxAttr.PosFormat, m_VtxAttr.PosElements);
	nat_offset += 12;
	m_native_vtx_decl.position.components = 3;
	m_native_vtx_decl.position.enable = true;
	m_native_vtx_decl.position.offset = 0;
	m_native_vtx_decl.position.type = VAR_FLOAT;
	m_native_vtx_decl.position.integer = false;

	// Normals
	if (m_VtxDesc.Normal != NOT_PRESENT)
	{
		m_VertexSize += VertexLoader_Normal::GetSize(m_VtxDesc.Normal,
			m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);

		TPipelineFunction pFunc = VertexLoader_Normal::GetFunction(m_VtxDesc.Normal,
			m_VtxAttr.NormalFormat, m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);

		if (pFunc == nullptr)
		{
			PanicAlert("VertexLoader_Normal::GetFunction(%i %i %i %i) returned zero!",
				(u32)m_VtxDesc.Normal, m_VtxAttr.NormalFormat,
				m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3);
		}
		WriteCall(pFunc);

		for (int i = 0; i < (vtx_attr.NormalElements ? 3 : 1); i++)
		{
			m_native_vtx_decl.normals[i].components = 3;
			m_native_vtx_decl.normals[i].enable = true;
			m_native_vtx_decl.normals[i].offset = nat_offset;
			m_native_vtx_decl.normals[i].type = VAR_FLOAT;
			m_native_vtx_decl.normals[i].integer = false;
			nat_offset += 12;
		}

		components |= VB_HAS_NRM0;
		if (m_VtxAttr.NormalElements == 1)
			components |= VB_HAS_NRM1 | VB_HAS_NRM2;
	}

	for (int i = 0; i < 2; i++)
	{
		m_native_vtx_decl.colors[i].components = 4;
		m_native_vtx_decl.colors[i].type = VAR_UNSIGNED_BYTE;
		m_native_vtx_decl.colors[i].integer = false;
		switch (col[i])
		{
		case NOT_PRESENT:
			break;
		case DIRECT:
			switch (m_VtxAttr.color[i].Comp)
			{
			case FORMAT_16B_565:  m_VertexSize += 2; WriteCall(Color_ReadDirect_16b_565); break;
			case FORMAT_24B_888:  m_VertexSize += 3; WriteCall(Color_ReadDirect_24b_888); break;
			case FORMAT_32B_888x: m_VertexSize += 4; WriteCall(Color_ReadDirect_32b_888x); break;
			case FORMAT_16B_4444: m_VertexSize += 2; WriteCall(Color_ReadDirect_16b_4444); break;
			case FORMAT_24B_6666: m_VertexSize += 3; WriteCall(Color_ReadDirect_24b_6666); break;
			case FORMAT_32B_8888: m_VertexSize += 4; WriteCall(Color_ReadDirect_32b_8888); break;
			default: _assert_(0); break;
			}
			break;
		case INDEX8:
			m_VertexSize += 1;
			switch (m_VtxAttr.color[i].Comp)
			{
			case FORMAT_16B_565:  WriteCall(Color_ReadIndex8_16b_565); break;
			case FORMAT_24B_888:  WriteCall(Color_ReadIndex8_24b_888); break;
			case FORMAT_32B_888x: WriteCall(Color_ReadIndex8_32b_888x); break;
			case FORMAT_16B_4444: WriteCall(Color_ReadIndex8_16b_4444); break;
			case FORMAT_24B_6666: WriteCall(Color_ReadIndex8_24b_6666); break;
			case FORMAT_32B_8888: WriteCall(Color_ReadIndex8_32b_8888); break;
			default: _assert_(0); break;
			}
			break;
		case INDEX16:
			m_VertexSize += 2;
			switch (m_VtxAttr.color[i].Comp)
			{
			case FORMAT_16B_565:  WriteCall(Color_ReadIndex16_16b_565); break;
			case FORMAT_24B_888:  WriteCall(Color_ReadIndex16_24b_888); break;
			case FORMAT_32B_888x: WriteCall(Color_ReadIndex16_32b_888x); break;
			case FORMAT_16B_4444: WriteCall(Color_ReadIndex16_16b_4444); break;
			case FORMAT_24B_6666: WriteCall(Color_ReadIndex16_24b_6666); break;
			case FORMAT_32B_8888: WriteCall(Color_ReadIndex16_32b_8888); break;
			default: _assert_(0); break;
			}
			break;
		}
		// Common for the three bottom cases
		if (col[i] != NOT_PRESENT)
		{
			components |= VB_HAS_COL0 << i;
			m_native_vtx_decl.colors[i].offset = nat_offset;
			m_native_vtx_decl.colors[i].enable = true;
			nat_offset += 4;
		}
	}

	// Texture matrix indices (remove if corresponding texture coordinate isn't enabled)
	for (int i = 0; i < 8; i++)
	{
		m_native_vtx_decl.texcoords[i].offset = nat_offset;
		m_native_vtx_decl.texcoords[i].type = VAR_FLOAT;
		m_native_vtx_decl.texcoords[i].integer = false;

		const int format = m_VtxAttr.texCoord[i].Format;
		const int elements = m_VtxAttr.texCoord[i].Elements;

		if (tc[i] == NOT_PRESENT)
		{
			components &= ~(VB_HAS_UV0 << i);
		}
		else
		{
			_assert_msg_(VIDEO, DIRECT <= tc[i] && tc[i] <= INDEX16, "Invalid texture coordinates!\n(tc[i] = %d)", (u32)tc[i]);
			_assert_msg_(VIDEO, FORMAT_UBYTE <= format && format <= FORMAT_FLOAT, "Invalid texture coordinates format!\n(format = %d)", format);
			_assert_msg_(VIDEO, 0 <= elements && elements <= 1, "Invalid number of texture coordinates elements!\n(elements = %d)", elements);

			components |= VB_HAS_UV0 << i;
			WriteCall(VertexLoader_TextCoord::GetFunction(tc[i], format, elements));
			m_VertexSize += VertexLoader_TextCoord::GetSize(tc[i], format, elements);
		}

		if (components & (VB_HAS_TEXMTXIDX0 << i))
		{
			m_native_vtx_decl.texcoords[i].enable = true;
			if (tc[i] != NOT_PRESENT)
			{
				// if texmtx is included, texcoord will always be 3 floats, z will be the texmtx index
				m_native_vtx_decl.texcoords[i].components = 3;
				nat_offset += 12;
				WriteCall(m_VtxAttr.texCoord[i].Elements ? TexMtx_Write_Float : TexMtx_Write_Float2);
			}
			else
			{
				components |= VB_HAS_UV0 << i; // have to include since using now
				m_native_vtx_decl.texcoords[i].components = 4;
				nat_offset += 16; // still include the texture coordinate, but this time as 6 + 2 bytes
				WriteCall(TexMtx_Write_Float4);
			}
		}
		else
		{
			if (tc[i] != NOT_PRESENT)
			{
				m_native_vtx_decl.texcoords[i].enable = true;
				m_native_vtx_decl.texcoords[i].components = vtx_attr.texCoord[i].Elements ? 2 : 1;
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
			if (j == 8 && !((components & VB_HAS_TEXMTXIDXALL) & (VB_HAS_TEXMTXIDXALL << (i + 1))))
			{
				// no more tex coords and tex matrices, so exit loop
				break;
			}
		}
	}

	// Update the bounding box
	if (!g_ActiveConfig.backend_info.bSupportsBBox)
		WriteCall(BoundingBox::Update);

	if (m_VtxDesc.PosMatIdx)
	{
		WriteCall(PosMtx_Write);
		m_native_vtx_decl.posmtx.components = 4;
		m_native_vtx_decl.posmtx.enable = true;
		m_native_vtx_decl.posmtx.offset = nat_offset;
		m_native_vtx_decl.posmtx.type = VAR_UNSIGNED_BYTE;
		m_native_vtx_decl.posmtx.integer = true;
		nat_offset += 4;
	}

	// indexed position formats may skip a the vertex
	if (m_VtxDesc.Position & 2)
	{
		WriteCall(SkipVertex);
	}

	m_native_components = components;
	m_native_vtx_decl.stride = nat_offset;

#ifdef USE_VERTEX_LOADER_JIT
	// End loop here
	SUB(64, R(RBX), Imm8(1));

	J_CC(CC_NZ, loop_start);
	ABI_PopRegistersAndAdjustStack({RBX, RBP}, 8);
	RET();
#endif
}

void VertexLoader::WriteCall(TPipelineFunction func)
{
#ifdef USE_VERTEX_LOADER_JIT
	MOV(64, R(ABI_PARAM1), R(RBP));
	ABI_CallFunction((const void*)func);
#else
	m_PipelineStages[m_numPipelineStages++] = func;
#endif
}
// ARMTODO: This should be done in a better way
#ifndef _M_GENERIC
void VertexLoader::WriteGetVariable(int bits, OpArg dest, void *address)
{
#ifdef USE_VERTEX_LOADER_JIT
	MOV(64, R(RAX), Imm64((u64)address));
	MOV(bits, dest, MatR(RAX));
#endif
}

void VertexLoader::WriteSetVariable(int bits, void *address, OpArg value)
{
#ifdef USE_VERTEX_LOADER_JIT
	MOV(64, R(RAX), Imm64((u64)address));
	MOV(bits, MatR(RAX), value);
#endif
}
#endif

int VertexLoader::RunVertices(int primitive, int count, DataReader src, DataReader dst)
{
	dst.WritePointer(&g_vertex_manager_write_ptr);
	src.WritePointer(&g_video_buffer_read_ptr);

	m_numLoadedVertices += count;
	m_skippedVertices = 0;

	// Prepare bounding box
	if (!g_ActiveConfig.backend_info.bSupportsBBox)
		BoundingBox::Prepare(m_vat, primitive, m_VtxDesc, m_native_vtx_decl);

#ifdef USE_VERTEX_LOADER_JIT
	if (count > 0)
	{
		((void (*)(int, VertexLoader* loader))(void*)m_compiledCode)(count, this);
	}
#else
	for (int s = 0; s < count; s++)
	{
		m_tcIndex = 0;
		m_colIndex = 0;
		m_texmtxwrite = m_texmtxread = 0;
		for (int i = 0; i < m_numPipelineStages; i++)
			m_PipelineStages[i](this);
		PRIM_LOG("\n");
	}
#endif

	return count - m_skippedVertices;
}
