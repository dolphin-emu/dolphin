// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

#include "VideoBackends/Software/CPMemLoader.h"
#include "VideoBackends/Software/SetupUnit.h"
#include "VideoBackends/Software/SWStatistics.h"
#include "VideoBackends/Software/SWVertexLoader.h"
#include "VideoBackends/Software/TransformUnit.h"
#include "VideoBackends/Software/XFMemLoader.h"

#include "VideoCommon/DataReader.h"
#include "VideoCommon/VertexLoader.h"
#include "VideoCommon/VertexLoader_Color.h"
#include "VideoCommon/VertexLoader_Normal.h"
#include "VideoCommon/VertexLoader_Position.h"
#include "VideoCommon/VertexLoader_TextCoord.h"
#include "VideoCommon/VertexManagerBase.h"

SWVertexLoader::SWVertexLoader() :
	m_VertexSize(0),
	m_NumAttributeLoaders(0)
{
	VertexLoader_Normal::Init();
	VertexLoader_Position::Init();
	VertexLoader_TextCoord::Init();

	m_SetupUnit = new SetupUnit;
}

SWVertexLoader::~SWVertexLoader()
{
	delete m_SetupUnit;
	m_SetupUnit = nullptr;
}

void SWVertexLoader::SetFormat(u8 attributeIndex, u8 primitiveType)
{
	m_CurrentVat = &g_main_cp_state.vtx_attr[attributeIndex];

	posScale = 1.0f / float(1 << m_CurrentVat->g0.PosFrac);
	tcScale[0] = 1.0f / float(1 << m_CurrentVat->g0.Tex0Frac);
	tcScale[1] = 1.0f / float(1 << m_CurrentVat->g1.Tex1Frac);
	tcScale[2] = 1.0f / float(1 << m_CurrentVat->g1.Tex2Frac);
	tcScale[3] = 1.0f / float(1 << m_CurrentVat->g1.Tex3Frac);
	tcScale[4] = 1.0f / float(1 << m_CurrentVat->g2.Tex4Frac);
	tcScale[5] = 1.0f / float(1 << m_CurrentVat->g2.Tex5Frac);
	tcScale[6] = 1.0f / float(1 << m_CurrentVat->g2.Tex6Frac);
	tcScale[7] = 1.0f / float(1 << m_CurrentVat->g2.Tex7Frac);

	//TexMtx
	const u64 tmDesc[8] = {
		g_main_cp_state.vtx_desc.Tex0MatIdx, g_main_cp_state.vtx_desc.Tex1MatIdx, g_main_cp_state.vtx_desc.Tex2MatIdx, g_main_cp_state.vtx_desc.Tex3MatIdx,
		g_main_cp_state.vtx_desc.Tex4MatIdx, g_main_cp_state.vtx_desc.Tex5MatIdx, g_main_cp_state.vtx_desc.Tex6MatIdx, g_main_cp_state.vtx_desc.Tex7MatIdx
	};

	// Colors
	const u64 colDesc[2] = {g_main_cp_state.vtx_desc.Color0, g_main_cp_state.vtx_desc.Color1};
	colElements[0] = m_CurrentVat->g0.Color0Elements;
	colElements[1] = m_CurrentVat->g0.Color1Elements;
	const u32 colComp[2] = {m_CurrentVat->g0.Color0Comp, m_CurrentVat->g0.Color1Comp};

	// TextureCoord
	const u64 tcDesc[8] = {
		g_main_cp_state.vtx_desc.Tex0Coord, g_main_cp_state.vtx_desc.Tex1Coord, g_main_cp_state.vtx_desc.Tex2Coord, g_main_cp_state.vtx_desc.Tex3Coord,
		g_main_cp_state.vtx_desc.Tex4Coord, g_main_cp_state.vtx_desc.Tex5Coord, g_main_cp_state.vtx_desc.Tex6Coord, g_main_cp_state.vtx_desc.Tex7Coord
	};
	const u32 tcElements[8] = {
		m_CurrentVat->g0.Tex0CoordElements, m_CurrentVat->g1.Tex1CoordElements, m_CurrentVat->g1.Tex2CoordElements,
		m_CurrentVat->g1.Tex3CoordElements, m_CurrentVat->g1.Tex4CoordElements, m_CurrentVat->g2.Tex5CoordElements,
		m_CurrentVat->g2.Tex6CoordElements, m_CurrentVat->g2.Tex7CoordElements
	};

	const u32 tcFormat[8] = {
		m_CurrentVat->g0.Tex0CoordFormat, m_CurrentVat->g1.Tex1CoordFormat, m_CurrentVat->g1.Tex2CoordFormat,
		m_CurrentVat->g1.Tex3CoordFormat, m_CurrentVat->g1.Tex4CoordFormat, m_CurrentVat->g2.Tex5CoordFormat,
		m_CurrentVat->g2.Tex6CoordFormat, m_CurrentVat->g2.Tex7CoordFormat
	};

	m_VertexSize = 0;

	// Reset pipeline
	m_positionLoader = nullptr;
	m_normalLoader = nullptr;
	m_NumAttributeLoaders = 0;

	// Reset vertex
	// matrix index from xf regs or cp memory?
	if (xfmem.MatrixIndexA.PosNormalMtxIdx != g_main_cp_state.matrix_index_a.PosNormalMtxIdx ||
		xfmem.MatrixIndexA.Tex0MtxIdx != g_main_cp_state.matrix_index_a.Tex0MtxIdx ||
		xfmem.MatrixIndexA.Tex1MtxIdx != g_main_cp_state.matrix_index_a.Tex1MtxIdx ||
		xfmem.MatrixIndexA.Tex2MtxIdx != g_main_cp_state.matrix_index_a.Tex2MtxIdx ||
		xfmem.MatrixIndexA.Tex3MtxIdx != g_main_cp_state.matrix_index_a.Tex3MtxIdx ||
		xfmem.MatrixIndexB.Tex4MtxIdx != g_main_cp_state.matrix_index_b.Tex4MtxIdx ||
		xfmem.MatrixIndexB.Tex5MtxIdx != g_main_cp_state.matrix_index_b.Tex5MtxIdx ||
		xfmem.MatrixIndexB.Tex6MtxIdx != g_main_cp_state.matrix_index_b.Tex6MtxIdx ||
		xfmem.MatrixIndexB.Tex7MtxIdx != g_main_cp_state.matrix_index_b.Tex7MtxIdx)
	{
		WARN_LOG(VIDEO, "Matrix indices don't match");

		// Just show the assert once
		static bool showedAlert = false;
		_assert_msg_(VIDEO, showedAlert, "Matrix indices don't match");
		showedAlert = true;
	}

#if(1)
	m_Vertex.posMtx = xfmem.MatrixIndexA.PosNormalMtxIdx;
	m_Vertex.texMtx[0] = xfmem.MatrixIndexA.Tex0MtxIdx;
	m_Vertex.texMtx[1] = xfmem.MatrixIndexA.Tex1MtxIdx;
	m_Vertex.texMtx[2] = xfmem.MatrixIndexA.Tex2MtxIdx;
	m_Vertex.texMtx[3] = xfmem.MatrixIndexA.Tex3MtxIdx;
	m_Vertex.texMtx[4] = xfmem.MatrixIndexB.Tex4MtxIdx;
	m_Vertex.texMtx[5] = xfmem.MatrixIndexB.Tex5MtxIdx;
	m_Vertex.texMtx[6] = xfmem.MatrixIndexB.Tex6MtxIdx;
	m_Vertex.texMtx[7] = xfmem.MatrixIndexB.Tex7MtxIdx;
#else
	m_Vertex.posMtx = g_main_cp_state.matrix_index_a.PosNormalMtxIdx;
	m_Vertex.texMtx[0] = g_main_cp_state.matrix_index_a.Tex0MtxIdx;
	m_Vertex.texMtx[1] = g_main_cp_state.matrix_index_a.Tex1MtxIdx;
	m_Vertex.texMtx[2] = g_main_cp_state.matrix_index_a.Tex2MtxIdx;
	m_Vertex.texMtx[3] = g_main_cp_state.matrix_index_a.Tex3MtxIdx;
	m_Vertex.texMtx[4] = g_main_cp_state.matrix_index_b.Tex4MtxIdx;
	m_Vertex.texMtx[5] = g_main_cp_state.matrix_index_b.Tex5MtxIdx;
	m_Vertex.texMtx[6] = g_main_cp_state.matrix_index_b.Tex6MtxIdx;
	m_Vertex.texMtx[7] = g_main_cp_state.matrix_index_b.Tex7MtxIdx;
#endif

	if (g_main_cp_state.vtx_desc.PosMatIdx != NOT_PRESENT)
	{
		AddAttributeLoader(LoadPosMtx);
		m_VertexSize++;
	}

	for (int i = 0; i < 8; ++i)
	{
		if (tmDesc[i] != NOT_PRESENT)
		{
			AddAttributeLoader(LoadTexMtx, i);
			m_VertexSize++;
		}
	}

	// Write vertex position loader
	m_positionLoader = VertexLoader_Position::GetFunction(g_main_cp_state.vtx_desc.Position, m_CurrentVat->g0.PosFormat, m_CurrentVat->g0.PosElements);
	m_VertexSize += VertexLoader_Position::GetSize(g_main_cp_state.vtx_desc.Position, m_CurrentVat->g0.PosFormat, m_CurrentVat->g0.PosElements);
	AddAttributeLoader(LoadPosition);

	// Normals
	if (g_main_cp_state.vtx_desc.Normal != NOT_PRESENT)
	{
		m_VertexSize += VertexLoader_Normal::GetSize(g_main_cp_state.vtx_desc.Normal,
			m_CurrentVat->g0.NormalFormat, m_CurrentVat->g0.NormalElements, m_CurrentVat->g0.NormalIndex3);

		m_normalLoader = VertexLoader_Normal::GetFunction(g_main_cp_state.vtx_desc.Normal,
			m_CurrentVat->g0.NormalFormat, m_CurrentVat->g0.NormalElements, m_CurrentVat->g0.NormalIndex3);

		if (m_normalLoader == nullptr)
		{
			ERROR_LOG(VIDEO, "VertexLoader_Normal::GetFunction returned zero!");
		}
		AddAttributeLoader(LoadNormal);
	}

	for (int i = 0; i < 2; i++)
	{
		switch (colDesc[i])
		{
		case NOT_PRESENT:
			m_colorLoader[i] = nullptr;
			break;
		case DIRECT:
			switch (colComp[i])
			{
			case FORMAT_16B_565:  m_VertexSize += 2; m_colorLoader[i] = (Color_ReadDirect_16b_565); break;
			case FORMAT_24B_888:  m_VertexSize += 3; m_colorLoader[i] = (Color_ReadDirect_24b_888); break;
			case FORMAT_32B_888x: m_VertexSize += 4; m_colorLoader[i] = (Color_ReadDirect_32b_888x); break;
			case FORMAT_16B_4444: m_VertexSize += 2; m_colorLoader[i] = (Color_ReadDirect_16b_4444); break;
			case FORMAT_24B_6666: m_VertexSize += 3; m_colorLoader[i] = (Color_ReadDirect_24b_6666); break;
			case FORMAT_32B_8888: m_VertexSize += 4; m_colorLoader[i] = (Color_ReadDirect_32b_8888); break;
			default: _assert_(0); break;
			}
			AddAttributeLoader(LoadColor, i);
			break;
		case INDEX8:
			m_VertexSize += 1;
			switch (colComp[i])
			{
			case FORMAT_16B_565:  m_colorLoader[i] = (Color_ReadIndex8_16b_565); break;
			case FORMAT_24B_888:  m_colorLoader[i] = (Color_ReadIndex8_24b_888); break;
			case FORMAT_32B_888x: m_colorLoader[i] = (Color_ReadIndex8_32b_888x); break;
			case FORMAT_16B_4444: m_colorLoader[i] = (Color_ReadIndex8_16b_4444); break;
			case FORMAT_24B_6666: m_colorLoader[i] = (Color_ReadIndex8_24b_6666); break;
			case FORMAT_32B_8888: m_colorLoader[i] = (Color_ReadIndex8_32b_8888); break;
			default: _assert_(0); break;
			}
			AddAttributeLoader(LoadColor, i);
			break;
		case INDEX16:
			m_VertexSize += 2;
			switch (colComp[i])
			{
			case FORMAT_16B_565:  m_colorLoader[i] = (Color_ReadIndex16_16b_565); break;
			case FORMAT_24B_888:  m_colorLoader[i] = (Color_ReadIndex16_24b_888); break;
			case FORMAT_32B_888x: m_colorLoader[i] = (Color_ReadIndex16_32b_888x); break;
			case FORMAT_16B_4444: m_colorLoader[i] = (Color_ReadIndex16_16b_4444); break;
			case FORMAT_24B_6666: m_colorLoader[i] = (Color_ReadIndex16_24b_6666); break;
			case FORMAT_32B_8888: m_colorLoader[i] = (Color_ReadIndex16_32b_8888); break;
			default: _assert_(0); break;
			}
			AddAttributeLoader(LoadColor, i);
			break;
		}
	}

	// Texture matrix indices (remove if corresponding texture coordinate isn't enabled)
	for (int i = 0; i < 8; i++)
	{
		const int desc = (int)tcDesc[i];
		const int format = tcFormat[i];
		const int elements = tcElements[i];
		_assert_msg_(VIDEO, NOT_PRESENT <= desc && desc <= INDEX16, "Invalid texture coordinates description!\n(desc = %d)", desc);
		_assert_msg_(VIDEO, FORMAT_UBYTE <= format && format <= FORMAT_FLOAT, "Invalid texture coordinates format!\n(format = %d)", format);
		_assert_msg_(VIDEO, 0 <= elements && elements <= 1, "Invalid number of texture coordinates elements!\n(elements = %d)", elements);

		m_texCoordLoader[i] = VertexLoader_TextCoord::GetFunction(desc, format, elements);
		m_VertexSize += VertexLoader_TextCoord::GetSize(desc, format, elements);
		if (m_texCoordLoader[i])
			AddAttributeLoader(LoadTexCoord, i);
	}

	// special case if only pos and tex coord 0 and tex coord input is AB11
	m_TexGenSpecialCase =
		((g_main_cp_state.vtx_desc.Hex & 0x60600L) == g_main_cp_state.vtx_desc.Hex) && // only pos and tex coord 0
		(g_main_cp_state.vtx_desc.Tex0Coord != NOT_PRESENT) &&
		(xfmem.texMtxInfo[0].projection == XF_TEXPROJ_ST);

	m_SetupUnit->Init(primitiveType);
}


void SWVertexLoader::LoadVertex()
{
	for (int i = 0; i < m_NumAttributeLoaders; i++)
		m_AttributeLoaders[i].loader(this, &m_Vertex, m_AttributeLoaders[i].index);

	OutputVertexData* outVertex = m_SetupUnit->GetVertex();

	// transform input data
	TransformUnit::TransformPosition(&m_Vertex, outVertex);

	if (g_main_cp_state.vtx_desc.Normal != NOT_PRESENT)
	{
		TransformUnit::TransformNormal(&m_Vertex, m_CurrentVat->g0.NormalElements, outVertex);
	}

	TransformUnit::TransformColor(&m_Vertex, outVertex);

	TransformUnit::TransformTexCoord(&m_Vertex, outVertex, m_TexGenSpecialCase);

	m_SetupUnit->SetupVertex();

	INCSTAT(swstats.thisFrame.numVerticesLoaded)
}

void SWVertexLoader::AddAttributeLoader(AttributeLoader loader, u8 index)
{
	_assert_msg_(VIDEO, m_NumAttributeLoaders < 21, "Too many attribute loaders");
	m_AttributeLoaders[m_NumAttributeLoaders].loader = loader;
	m_AttributeLoaders[m_NumAttributeLoaders++].index = index;
}

void SWVertexLoader::LoadPosMtx(SWVertexLoader *vertexLoader, InputVertexData *vertex, u8 unused)
{
	vertex->posMtx = DataReadU8() & 0x3f;
}

void SWVertexLoader::LoadTexMtx(SWVertexLoader *vertexLoader, InputVertexData *vertex, u8 index)
{
	vertex->texMtx[index] = DataReadU8() & 0x3f;
}

void SWVertexLoader::LoadPosition(SWVertexLoader *vertexLoader, InputVertexData *vertex, u8 unused)
{
	VertexManager::s_pCurBufferPointer = (u8*)&vertex->position;
	vertexLoader->m_positionLoader();
}

void SWVertexLoader::LoadNormal(SWVertexLoader *vertexLoader, InputVertexData *vertex, u8 unused)
{
	VertexManager::s_pCurBufferPointer = (u8*)&vertex->normal;
	vertexLoader->m_normalLoader();
}

void SWVertexLoader::LoadColor(SWVertexLoader *vertexLoader, InputVertexData *vertex, u8 index)
{
	u32 color;
	VertexManager::s_pCurBufferPointer = (u8*)&color;
	colIndex = index;
	vertexLoader->m_colorLoader[index]();

	// rgba -> abgr
	*(u32*)vertex->color[index] = Common::swap32(color);
}

void SWVertexLoader::LoadTexCoord(SWVertexLoader *vertexLoader, InputVertexData *vertex, u8 index)
{
	VertexManager::s_pCurBufferPointer = (u8*)&vertex->texCoords[index];
	tcIndex = index;
	vertexLoader->m_texCoordLoader[index]();
}

void SWVertexLoader::DoState(PointerWrap &p)
{
	p.DoArray(m_AttributeLoaders, sizeof m_AttributeLoaders);
	p.Do(m_VertexSize);
	p.Do(*m_CurrentVat);
	p.Do(m_positionLoader);
	p.Do(m_normalLoader);
	p.DoArray(m_colorLoader, sizeof m_colorLoader);
	p.Do(m_NumAttributeLoaders);
	m_SetupUnit->DoState(p);
	p.Do(m_TexGenSpecialCase);
}
