// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <limits>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

#include "VideoBackends/Software/CPMemLoader.h"
#include "VideoBackends/Software/NativeVertexFormat.h"
#include "VideoBackends/Software/SetupUnit.h"
#include "VideoBackends/Software/SWStatistics.h"
#include "VideoBackends/Software/SWVertexLoader.h"
#include "VideoBackends/Software/TransformUnit.h"
#include "VideoBackends/Software/XFMemLoader.h"

#include "VideoCommon/VertexLoaderBase.h"
#include "VideoCommon/VertexLoaderUtils.h"

SWVertexLoader::SWVertexLoader() :
	m_VertexSize(0)
{
	m_SetupUnit = new SetupUnit;
}

SWVertexLoader::~SWVertexLoader()
{
	delete m_SetupUnit;
	m_SetupUnit = nullptr;
}

void SWVertexLoader::SetFormat(u8 attributeIndex, u8 primitiveType)
{
	m_attributeIndex = attributeIndex;

	VertexLoaderUID uid(g_main_cp_state.vtx_desc, g_main_cp_state.vtx_attr[m_attributeIndex]);
	m_CurrentLoader = m_VertexLoaderMap[uid].get();

	if (!m_CurrentLoader)
	{
		m_CurrentLoader = VertexLoaderBase::CreateVertexLoader(g_main_cp_state.vtx_desc, g_main_cp_state.vtx_attr[m_attributeIndex]);
		m_VertexLoaderMap[uid] = std::unique_ptr<VertexLoaderBase>(m_CurrentLoader);
	}

	m_VertexSize = m_CurrentLoader->m_VertexSize;
	m_CurrentVat = &g_main_cp_state.vtx_attr[m_attributeIndex];


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

	m_Vertex.posMtx = xfmem.MatrixIndexA.PosNormalMtxIdx;
	m_Vertex.texMtx[0] = xfmem.MatrixIndexA.Tex0MtxIdx;
	m_Vertex.texMtx[1] = xfmem.MatrixIndexA.Tex1MtxIdx;
	m_Vertex.texMtx[2] = xfmem.MatrixIndexA.Tex2MtxIdx;
	m_Vertex.texMtx[3] = xfmem.MatrixIndexA.Tex3MtxIdx;
	m_Vertex.texMtx[4] = xfmem.MatrixIndexB.Tex4MtxIdx;
	m_Vertex.texMtx[5] = xfmem.MatrixIndexB.Tex5MtxIdx;
	m_Vertex.texMtx[6] = xfmem.MatrixIndexB.Tex6MtxIdx;
	m_Vertex.texMtx[7] = xfmem.MatrixIndexB.Tex7MtxIdx;


	// special case if only pos and tex coord 0 and tex coord input is AB11
	m_TexGenSpecialCase =
		((g_main_cp_state.vtx_desc.Hex & 0x60600L) == g_main_cp_state.vtx_desc.Hex) && // only pos and tex coord 0
		(g_main_cp_state.vtx_desc.Tex0Coord != NOT_PRESENT) &&
		(xfmem.texMtxInfo[0].projection == XF_TEXPROJ_ST);

	m_SetupUnit->Init(primitiveType);
}

template <typename T, typename I>
static T ReadNormalized(I value)
{
	T casted = (T) value;
	if (!std::numeric_limits<T>::is_integer && std::numeric_limits<I>::is_integer)
	{
		// normalize if non-float is converted to a float
		casted *= (T) (1.0 / std::numeric_limits<I>::max());
	}
	return casted;
}

template <typename T, bool swap = false>
static void ReadVertexAttribute(T* dst, DataReader src, const AttributeFormat& format, int base_component, int components, bool reverse)
{
	if (format.enable)
	{
		src.Skip(format.offset);
		src.Skip(base_component * (1<<(format.type>>1)));

		int i;
		for (i = 0; i < std::min(format.components - base_component, components); i++)
		{
			int i_dst = reverse ? components - i - 1 : i;
			switch (format.type)
			{
				case VAR_UNSIGNED_BYTE:
					dst[i_dst] = ReadNormalized<T, u8>(src.Read<u8, swap>());
					break;
				case VAR_BYTE:
					dst[i_dst] = ReadNormalized<T, s8>(src.Read<s8, swap>());
					break;
				case VAR_UNSIGNED_SHORT:
					dst[i_dst] = ReadNormalized<T, u16>(src.Read<u16, swap>());
					break;
				case VAR_SHORT:
					dst[i_dst] = ReadNormalized<T, s16>(src.Read<s16, swap>());
					break;
				case VAR_FLOAT:
					dst[i_dst] = ReadNormalized<T, float>(src.Read<float, swap>());
					break;
			}

			_assert_msg_(VIDEO, !format.integer || format.type != VAR_FLOAT, "only non-float values are allowed to be streamed as integer");
		}
		for (; i < components; i++)
		{
			int i_dst = reverse ? components - i - 1 : i;
			dst[i_dst] = i == 3;
		}
	}
}

void SWVertexLoader::ParseVertex(const PortableVertexDeclaration& vdec)
{
	DataReader src(m_LoadedVertices.data(), m_LoadedVertices.data() + m_LoadedVertices.size());

	ReadVertexAttribute<float>(&m_Vertex.position[0], src, vdec.position, 0, 3, false);

	for (int i = 0; i < 3; i++)
	{
		ReadVertexAttribute<float>(&m_Vertex.normal[i][0], src, vdec.normals[i], 0, 3, false);
	}

	for (int i = 0; i < 2; i++)
	{
		ReadVertexAttribute<u8>(m_Vertex.color[i], src, vdec.colors[i], 0, 4, true);
	}

	for (int i = 0; i < 8; i++)
	{
		ReadVertexAttribute<float>(m_Vertex.texCoords[i], src, vdec.texcoords[i], 0, 2, false);

		// the texmtr is stored as third component of the texCoord
		if (vdec.texcoords[i].components >= 3)
		{
			ReadVertexAttribute<u8>(&m_Vertex.texMtx[i], src, vdec.texcoords[i], 2, 1, false);
		}
	}

	ReadVertexAttribute<u8>(&m_Vertex.posMtx, src, vdec.posmtx, 0, 1, false);
}

void SWVertexLoader::LoadVertex()
{
	const PortableVertexDeclaration& vdec = m_CurrentLoader->m_native_vtx_decl;

	// reserve memory for the destination of the vertex loader
	m_LoadedVertices.resize(vdec.stride + 4);

	// convert the vertex from the gc format to the videocommon (hardware optimized) format
	u8* old = g_video_buffer_read_ptr;
	int converted_vertices = m_CurrentLoader->RunVertices(
		DataReader(g_video_buffer_read_ptr, nullptr), // src
		DataReader(m_LoadedVertices.data(), m_LoadedVertices.data() + m_LoadedVertices.size()), // dst
		1 // vertices
	);
	g_video_buffer_read_ptr = old + m_CurrentLoader->m_VertexSize;

	if (converted_vertices == 0)
		return;

	// parse the videocommon format to our own struct format (m_Vertex)
	ParseVertex(vdec);

	// transform this vertex so that it can be used for rasterization (outVertex)
	OutputVertexData* outVertex = m_SetupUnit->GetVertex();
	TransformUnit::TransformPosition(&m_Vertex, outVertex);
	if (g_main_cp_state.vtx_desc.Normal != NOT_PRESENT)
	{
		TransformUnit::TransformNormal(&m_Vertex, m_CurrentVat->g0.NormalElements, outVertex);
	}
	TransformUnit::TransformColor(&m_Vertex, outVertex);
	TransformUnit::TransformTexCoord(&m_Vertex, outVertex, m_TexGenSpecialCase);

	// assemble and rasterize the primitive
	m_SetupUnit->SetupVertex();

	INCSTAT(swstats.thisFrame.numVerticesLoaded)
}

void SWVertexLoader::DoState(PointerWrap &p)
{
	p.Do(m_VertexSize);
	p.Do(*m_CurrentVat);
	m_SetupUnit->DoState(p);
	p.Do(m_TexGenSpecialCase);
}
