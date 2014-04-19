// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/Common.h"

#include "VideoBackends/Software/CPMemLoader.h"
#include "VideoBackends/Software/SetupUnit.h"
#include "VideoBackends/Software/SWStatistics.h"
#include "VideoBackends/Software/SWVertexLoader.h"
#include "VideoBackends/Software/TransformUnit.h"
#include "VideoBackends/Software/XFMemLoader.h"

#include "VideoCommon/DataReader.h"
#include "VideoCommon/VertexLoader_Color.h"
#include "VideoCommon/VertexLoader_Normal.h"
#include "VideoCommon/VertexLoader_Position.h"
#include "VideoCommon/VertexLoader_TextCoord.h"
#include "VideoCommon/VertexManagerBase.h"

// Vertex loaders read these
extern int tcIndex;
extern int colIndex;
extern int colElements[2];
extern float posScale;
extern float tcScale[8];


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
	m_CurrentVat = &g_VtxAttr[attributeIndex];

	posScale = 1.0f / float(1 << m_CurrentVat->PosFrac);
	for (int i = 0; i < 8; ++i)
		tcScale[0] = 1.0f / float(1 << m_CurrentVat->GetTexCoordFracs()[i]);

	const auto tmDesc = g_VtxDesc.TexMtxIdx();

	// Colors
	const auto colDesc = g_VtxDesc.Color();
	const auto colComp = m_CurrentVat->GetColorComponents();
	colElements[0] = m_CurrentVat->Color0Elements;
	colElements[1] = m_CurrentVat->Color1Elements;

	// TextureCoord
	const auto tcDesc = g_VtxDesc.TexCoord();

	m_VertexSize = 0;

	// Reset pipeline
	m_positionLoader = nullptr;
	m_normalLoader = nullptr;
	m_NumAttributeLoaders = 0;

	// Reset vertex
	// matrix index from xf regs or cp memory?
	if (swxfregs.MatrixIndexA.PosNormalMtxIdx != MatrixIndexA.PosNormalMtxIdx ||
		swxfregs.MatrixIndexA.Tex0MtxIdx != MatrixIndexA.Tex0MtxIdx ||
		swxfregs.MatrixIndexA.Tex1MtxIdx != MatrixIndexA.Tex1MtxIdx ||
		swxfregs.MatrixIndexA.Tex2MtxIdx != MatrixIndexA.Tex2MtxIdx ||
		swxfregs.MatrixIndexA.Tex3MtxIdx != MatrixIndexA.Tex3MtxIdx ||
		swxfregs.MatrixIndexB.Tex4MtxIdx != MatrixIndexB.Tex4MtxIdx ||
		swxfregs.MatrixIndexB.Tex5MtxIdx != MatrixIndexB.Tex5MtxIdx ||
		swxfregs.MatrixIndexB.Tex6MtxIdx != MatrixIndexB.Tex6MtxIdx ||
		swxfregs.MatrixIndexB.Tex7MtxIdx != MatrixIndexB.Tex7MtxIdx)
	{
		WARN_LOG(VIDEO, "Matrix indices don't match");

		// Just show the assert once
		static bool showedAlert = false;
		_assert_msg_(VIDEO, showedAlert, "Matrix indices don't match");
		showedAlert = true;
	}

#if(1)
	m_Vertex.posMtx = swxfregs.MatrixIndexA.PosNormalMtxIdx;
	m_Vertex.texMtx[0] = swxfregs.MatrixIndexA.Tex0MtxIdx;
	m_Vertex.texMtx[1] = swxfregs.MatrixIndexA.Tex1MtxIdx;
	m_Vertex.texMtx[2] = swxfregs.MatrixIndexA.Tex2MtxIdx;
	m_Vertex.texMtx[3] = swxfregs.MatrixIndexA.Tex3MtxIdx;
	m_Vertex.texMtx[4] = swxfregs.MatrixIndexB.Tex4MtxIdx;
	m_Vertex.texMtx[5] = swxfregs.MatrixIndexB.Tex5MtxIdx;
	m_Vertex.texMtx[6] = swxfregs.MatrixIndexB.Tex6MtxIdx;
	m_Vertex.texMtx[7] = swxfregs.MatrixIndexB.Tex7MtxIdx;
#else
	m_Vertex.posMtx = MatrixIndexA.PosNormalMtxIdx;
	m_Vertex.texMtx[0] = MatrixIndexA.Tex0MtxIdx;
	m_Vertex.texMtx[1] = MatrixIndexA.Tex1MtxIdx;
	m_Vertex.texMtx[2] = MatrixIndexA.Tex2MtxIdx;
	m_Vertex.texMtx[3] = MatrixIndexA.Tex3MtxIdx;
	m_Vertex.texMtx[4] = MatrixIndexB.Tex4MtxIdx;
	m_Vertex.texMtx[5] = MatrixIndexB.Tex5MtxIdx;
	m_Vertex.texMtx[6] = MatrixIndexB.Tex6MtxIdx;
	m_Vertex.texMtx[7] = MatrixIndexB.Tex7MtxIdx;
#endif

	if (g_VtxDesc.PosMatIdx != TVtxDesc::NOT_PRESENT)
	{
		AddAttributeLoader(LoadPosMtx);
		m_VertexSize++;
	}

	for (int i = 0; i < 8; ++i)
	{
		if (tmDesc[i] != TVtxDesc::NOT_PRESENT)
		{
			AddAttributeLoader(LoadTexMtx, i);
			m_VertexSize++;
		}
	}

	// Write vertex position loader
	m_positionLoader = VertexLoader_Position::GetFunction(g_VtxDesc.Position, m_CurrentVat->PosFormat, m_CurrentVat->PosElements);
	m_VertexSize += VertexLoader_Position::GetSize(g_VtxDesc.Position, m_CurrentVat->PosFormat, m_CurrentVat->PosElements);
	AddAttributeLoader(LoadPosition);

	// Normals
	if (g_VtxDesc.Normal != TVtxDesc::NOT_PRESENT)
	{
		m_VertexSize += VertexLoader_Normal::GetSize(g_VtxDesc.Normal,
			m_CurrentVat->NormalFormat, m_CurrentVat->NormalElements, m_CurrentVat->NormalIndex3);

		m_normalLoader = VertexLoader_Normal::GetFunction(g_VtxDesc.Normal,
			m_CurrentVat->NormalFormat, m_CurrentVat->NormalElements, m_CurrentVat->NormalIndex3);

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
		case TVtxDesc::NOT_PRESENT:
			m_colorLoader[i] = nullptr;
			break;
		case TVtxDesc::DIRECT:
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
		case TVtxDesc::INDEX8:
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
		case TVtxDesc::INDEX16:
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
		const TVtxDesc::VertexComponentType desc = tcDesc[i];
		const VAT::VertexComponentFormat format = m_CurrentVat->GetTexCoordFormats()[i];
		const int elements = m_CurrentVat->GetTexCoordElements()[i];
		_assert_msg_(VIDEO, TVtxDesc::NOT_PRESENT <= desc && desc <= TVtxDesc::INDEX16, "Invalid texture coordinates description!\n(desc = %d)", (int)desc);
		_assert_msg_(VIDEO, VAT::UBYTE <= format && format <= VAT::FLOAT, "Invalid texture coordinates format!\n(format = %d)", format);
		_assert_msg_(VIDEO, 0 <= elements && elements <= 1, "Invalid number of texture coordinates elements!\n(elements = %d)", elements);

		m_texCoordLoader[i] = VertexLoader_TextCoord::GetFunction(desc, format, elements);
		m_VertexSize += VertexLoader_TextCoord::GetSize(desc, format, elements);
		if (m_texCoordLoader[i])
			AddAttributeLoader(LoadTexCoord, i);
	}

	// special case if only pos and tex coord 0 and tex coord input is AB11
	m_TexGenSpecialCase =
		((g_VtxDesc.Hex & 0x60600L) == g_VtxDesc.Hex) && // only pos and tex coord 0
		(g_VtxDesc.Tex0Coord != TVtxDesc::NOT_PRESENT) &&
		(swxfregs.texMtxInfo[0].projection == XF_TEXPROJ_ST);

	m_SetupUnit->Init(primitiveType);
}


void SWVertexLoader::LoadVertex()
{
	for (int i = 0; i < m_NumAttributeLoaders; i++)
		m_AttributeLoaders[i].loader(this, &m_Vertex, m_AttributeLoaders[i].index);

	OutputVertexData* outVertex = m_SetupUnit->GetVertex();

	// transform input data
	TransformUnit::TransformPosition(&m_Vertex, outVertex);

	if (g_VtxDesc.Normal != TVtxDesc::NOT_PRESENT)
	{
		TransformUnit::TransformNormal(&m_Vertex, m_CurrentVat->NormalElements, outVertex);
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
