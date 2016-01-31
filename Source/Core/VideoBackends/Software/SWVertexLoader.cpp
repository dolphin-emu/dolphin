// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <limits>

#include "Common/ChunkFile.h"
#include "Common/CommonTypes.h"

#include "VideoBackends/Software/Clipper.h"
#include "VideoBackends/Software/DebugUtil.h"
#include "VideoBackends/Software/NativeVertexFormat.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/SetupUnit.h"
#include "VideoBackends/Software/SWVertexLoader.h"
#include "VideoBackends/Software/Tev.h"
#include "VideoBackends/Software/TransformUnit.h"

#include "VideoCommon/DataReader.h"
#include "VideoCommon/IndexGenerator.h"
#include "VideoCommon/OpcodeDecoding.h"
#include "VideoCommon/PixelShaderManager.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/VertexLoaderBase.h"
#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/XFMemory.h"

class NullNativeVertexFormat : public NativeVertexFormat
{
public:
	NullNativeVertexFormat(const PortableVertexDeclaration& _vtx_decl) { vtx_decl = _vtx_decl; }
	void SetupVertexPointers() override {}
};

NativeVertexFormat* SWVertexLoader::CreateNativeVertexFormat(const PortableVertexDeclaration& vtx_decl)
{
	return new NullNativeVertexFormat(vtx_decl);
}

SWVertexLoader::SWVertexLoader()
{
	LocalVBuffer.resize(MAXVBUFFERSIZE);
	LocalIBuffer.resize(MAXIBUFFERSIZE);
	m_SetupUnit = new SetupUnit;
}

SWVertexLoader::~SWVertexLoader()
{
	delete m_SetupUnit;
	m_SetupUnit = nullptr;
}

void SWVertexLoader::ResetBuffer(u32 stride)
{
	s_pCurBufferPointer = s_pBaseBufferPointer = LocalVBuffer.data();
	s_pEndBufferPointer = s_pCurBufferPointer + LocalVBuffer.size();
	IndexGenerator::Start(GetIndexBuffer());
}

void SWVertexLoader::vFlush(bool useDstAlpha)
{
	DebugUtil::OnObjectBegin();

	u8 primitiveType = 0;
	switch (current_primitive_type)
	{
		case PRIMITIVE_POINTS:
			primitiveType = GX_DRAW_POINTS;
			break;
		case PRIMITIVE_LINES:
			primitiveType = GX_DRAW_LINES;
			break;
		case PRIMITIVE_TRIANGLES:
			primitiveType = g_ActiveConfig.backend_info.bSupportsPrimitiveRestart ? GX_DRAW_TRIANGLE_STRIP : GX_DRAW_TRIANGLES;
			break;
	}

	m_SetupUnit->Init(primitiveType);

	// set all states with are stored within video sw
	Clipper::SetViewOffset();
	Rasterizer::SetScissor();
	for (int i = 0; i < 4; i++)
	{
		Rasterizer::SetTevReg(i, Tev::RED_C, false, PixelShaderManager::constants.colors[i][0]);
		Rasterizer::SetTevReg(i, Tev::GRN_C, false, PixelShaderManager::constants.colors[i][1]);
		Rasterizer::SetTevReg(i, Tev::BLU_C, false, PixelShaderManager::constants.colors[i][2]);
		Rasterizer::SetTevReg(i, Tev::ALP_C, false, PixelShaderManager::constants.colors[i][3]);
		Rasterizer::SetTevReg(i, Tev::RED_C, true, PixelShaderManager::constants.kcolors[i][0]);
		Rasterizer::SetTevReg(i, Tev::GRN_C, true, PixelShaderManager::constants.kcolors[i][1]);
		Rasterizer::SetTevReg(i, Tev::BLU_C, true, PixelShaderManager::constants.kcolors[i][2]);
		Rasterizer::SetTevReg(i, Tev::ALP_C, true, PixelShaderManager::constants.kcolors[i][3]);
	}

	for (u32 i = 0; i < IndexGenerator::GetIndexLen(); i++)
	{
		u16 index = LocalIBuffer[i];

		if (index == 0xffff)
		{
			// primitive restart
			m_SetupUnit->Init(primitiveType);
			continue;
		}
		memset(&m_Vertex, 0, sizeof(m_Vertex));

		// Super Mario Sunshine requires those to be zero for those debug boxes.
		memset(&m_Vertex.color, 0, sizeof(m_Vertex.color));

		// parse the videocommon format to our own struct format (m_Vertex)
		SetFormat(g_main_cp_state.last_id, primitiveType);
		ParseVertex(VertexLoaderManager::GetCurrentVertexFormat()->GetVertexDeclaration(), index);

		// transform this vertex so that it can be used for rasterization (outVertex)
		OutputVertexData* outVertex = m_SetupUnit->GetVertex();
		TransformUnit::TransformPosition(&m_Vertex, outVertex);
		memset(&outVertex->normal, 0, sizeof(outVertex->normal));
		if (VertexLoaderManager::g_current_components & VB_HAS_NRM0)
		{
			TransformUnit::TransformNormal(&m_Vertex, (VertexLoaderManager::g_current_components & VB_HAS_NRM2) != 0, outVertex);
		}
		TransformUnit::TransformColor(&m_Vertex, outVertex);
		TransformUnit::TransformTexCoord(&m_Vertex, outVertex, m_TexGenSpecialCase);

		// assemble and rasterize the primitive
		m_SetupUnit->SetupVertex();

		INCSTAT(stats.thisFrame.numVerticesLoaded)
	}

	DebugUtil::OnObjectEnd();
}

void SWVertexLoader::SetFormat(u8 attributeIndex, u8 primitiveType)
{
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
		ERROR_LOG(VIDEO, "Matrix indices don't match");
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

void SWVertexLoader::ParseVertex(const PortableVertexDeclaration& vdec, int index)
{
	DataReader src(LocalVBuffer.data(), LocalVBuffer.data() + LocalVBuffer.size());
	src.Skip(index * vdec.stride);

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
