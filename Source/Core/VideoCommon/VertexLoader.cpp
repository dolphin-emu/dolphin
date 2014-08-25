// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonTypes.h"
#include "Common/MemoryUtil.h"
#include "Common/StringUtil.h"
#include "Common/x64ABI.h"
#include "Common/x64Emitter.h"

#include "Core/Host.h"

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

//BBox
#include "VideoCommon/XFMemory.h"

#define COMPILED_CODE_SIZE 4096

#ifndef _WIN32
	#undef inline
	#define inline
#endif

// Matrix components are first in GC format but later in PC format - we need to store it temporarily
// when decoding each vertex.
static u8 s_curposmtx = MatrixIndexA.PosNormalMtxIdx;
static u8 s_curtexmtx[8];
static int s_texmtxwrite = 0;
static int s_texmtxread = 0;

static int loop_counter;


// Vertex loaders read these. Although the scale ones should be baked into the shader.
int tcIndex;
int colIndex;
int colElements[2];
float posScale;
float tcScale[8];

// bbox variables
// bbox must read vertex position, so convert it to this buffer
static float s_bbox_vertex_buffer[3];
static u8 *s_bbox_pCurBufferPointer_orig;
static int s_bbox_primitive;
static struct Point
{
	s32 x;
	s32 y;
	float z;
} s_bbox_points[3];
static u8 s_bbox_currPoint;
static u8 s_bbox_loadedPoints;
static const u8 s_bbox_primitivePoints[8] = { 3, 0, 3, 3, 3, 2, 2, 1 };

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

static void LOADERDECL PosMtx_ReadDirect_UByte()
{
	s_curposmtx = DataReadU8() & 0x3f;
	PRIM_LOG("posmtx: %d, ", s_curposmtx);
}

static void LOADERDECL PosMtx_Write()
{
	DataWrite<u8>(s_curposmtx);
	DataWrite<u8>(0);
	DataWrite<u8>(0);
	DataWrite<u8>(0);

	// Resetting current position matrix to default is needed for bbox to behave
	s_curposmtx = (u8) MatrixIndexA.PosNormalMtxIdx;
}

static void LOADERDECL UpdateBoundingBoxPrepare()
{
	if (!PixelEngine::bbox_active)
		return;

	// set our buffer as videodata buffer, so we will get a copy of the vertex positions
	// this is a big hack, but so we can use the same converting function then without bbox
	s_bbox_pCurBufferPointer_orig = VertexManager::s_pCurBufferPointer;
	VertexManager::s_pCurBufferPointer = (u8*)s_bbox_vertex_buffer;
}

static inline bool UpdateBoundingBoxVars()
{
	switch (s_bbox_primitive)
	{
		// Quads: fill 0,1,2 (check),1 (check, clear, repeat)
	case 0:
		++s_bbox_loadedPoints;
		if (s_bbox_loadedPoints == 3)
		{
			s_bbox_currPoint = 1;
			return true;
		}
		if (s_bbox_loadedPoints == 4)
		{
			s_bbox_loadedPoints = 0;
			s_bbox_currPoint = 0;
			return true;
		}
		++s_bbox_currPoint;
		return false;

		// Triangles: 0,1,2 (check, clear, repeat)
	case 2:
		++s_bbox_loadedPoints;
		if (s_bbox_loadedPoints == 3)
		{
			s_bbox_loadedPoints = 0;
			s_bbox_currPoint = 0;
			return true;
		}
		++s_bbox_currPoint;
		return false;

		// Triangle strip: 0, 1, 2 (check), 0 (check), 1, (check), 2 (check, repeat checking 0, 1, 2)
	case 3:
		if (++s_bbox_currPoint == 3)
			s_bbox_currPoint = 0;

		if (s_bbox_loadedPoints == 2)
			return true;

		++s_bbox_loadedPoints;
		return false;

		// Triangle fan: 0,1,2 (check), 1 (check), 2 (check, repeat checking 1,2)
	case 4:
		s_bbox_currPoint ^= s_bbox_currPoint ? 3 : 1;

		if (s_bbox_loadedPoints == 2)
			return true;

		++s_bbox_loadedPoints;
		return false;

		// Lines: 0,1 (check, clear, repeat)
	case 5:
		++s_bbox_loadedPoints;
		if (s_bbox_loadedPoints == 2)
		{
			s_bbox_loadedPoints = 0;
			s_bbox_currPoint = 0;
			return true;
		}
		++s_bbox_currPoint;
		return false;

		// Line strip: 0,1 (check), 0 (check), 1 (check, repeat checking 0,1)
	case 6:
		s_bbox_currPoint ^= 1;

		if (s_bbox_loadedPoints == 1)
			return true;

		++s_bbox_loadedPoints;
		return false;

		// Points: 0 (check, clear, repeat)
	case 7:
		return true;

		// This should not happen!
	default:
		return false;
	}
}

static void LOADERDECL UpdateBoundingBox()
{
	if (!PixelEngine::bbox_active)
		return;

	// Reset videodata pointer
	VertexManager::s_pCurBufferPointer = s_bbox_pCurBufferPointer_orig;

	// Copy vertex pointers
	memcpy(VertexManager::s_pCurBufferPointer, s_bbox_vertex_buffer, 12);
	VertexManager::s_pCurBufferPointer += 12;

	// We must transform the just loaded point by the current world and projection matrix - in software
	float transformed[3];
	float screenPoint[3];

	// We need to get the raw projection values for the bounding box calculation
	// to work properly. That means, no projection hacks!
	const float * const orig_point = s_bbox_vertex_buffer;
	const float * const world_matrix = (float*)xfmem.posMatrices + s_curposmtx * 4;
	const float * const proj_matrix = xfmem.projection.rawProjection;

	// Transform by world matrix
	// Only calculate what we need, discard the rest
	transformed[0] = orig_point[0] * world_matrix[0] + orig_point[1] * world_matrix[1] + orig_point[2] * world_matrix[2] + world_matrix[3];
	transformed[1] = orig_point[0] * world_matrix[4] + orig_point[1] * world_matrix[5] + orig_point[2] * world_matrix[6] + world_matrix[7];

	// Transform by projection matrix
	switch (xfmem.projection.type)
	{
		// Perspective projection, we must divide by w
	case GX_PERSPECTIVE:
		transformed[2] = orig_point[0] * world_matrix[8] + orig_point[1] * world_matrix[9] + orig_point[2] * world_matrix[10] + world_matrix[11];
		screenPoint[0] = (transformed[0] * proj_matrix[0] + transformed[2] * proj_matrix[1]) / (-transformed[2]);
		screenPoint[1] = (transformed[1] * proj_matrix[2] + transformed[2] * proj_matrix[3]) / (-transformed[2]);
		screenPoint[2] = ((transformed[2] * proj_matrix[4] + proj_matrix[5]) * (1.0f - (float) 1e-7)) / (-transformed[2]);
		break;

		// Orthographic projection
	case GX_ORTHOGRAPHIC:
		screenPoint[0] = transformed[0] * proj_matrix[0] + proj_matrix[1];
		screenPoint[1] = transformed[1] * proj_matrix[2] + proj_matrix[3];

		// We don't really have to care about z here
		screenPoint[2] = -0.2f;
		break;

	default:
		ERROR_LOG(VIDEO, "Unknown projection type: %d", xfmem.projection.type);
		screenPoint[0] = screenPoint[1] = screenPoint[2] = 1;
	}

	// Convert to screen space and add the point to the list - round like the real hardware
	s_bbox_points[s_bbox_currPoint].x = (((s32) (0.5f + (16.0f * (screenPoint[0] * xfmem.viewport.wd + (xfmem.viewport.xOrig - 342.0f))))) + 3) >> 4;
	s_bbox_points[s_bbox_currPoint].y = (((s32) (0.5f + (16.0f * (screenPoint[1] * xfmem.viewport.ht + (xfmem.viewport.yOrig - 342.0f))))) + 3) >> 4;
	s_bbox_points[s_bbox_currPoint].z = screenPoint[2];

	// Update point list for primitive
	bool check_bbox = UpdateBoundingBoxVars();

	// If we do not have enough points to check the bounding box yet, we are done for now
	if (!check_bbox)
		return;

	// How many points does our primitive have?
	const u8 numPoints = s_bbox_primitivePoints[s_bbox_primitive];

	// If the primitive is a point, update the bounding box now
	if (numPoints == 1)
	{
		Point & p = s_bbox_points[0];

		// Point is out of bounds
		if (p.x < 0 || p.x > 607 || p.y < 0 || p.y > 479 || p.z >= 0.0f)
			return;

		// Point is in bounds. Update bounding box if necessary and return
		PixelEngine::bbox[0] = (p.x < PixelEngine::bbox[0]) ? p.x : PixelEngine::bbox[0];
		PixelEngine::bbox[1] = (p.x > PixelEngine::bbox[1]) ? p.x : PixelEngine::bbox[1];
		PixelEngine::bbox[2] = (p.y < PixelEngine::bbox[2]) ? p.y : PixelEngine::bbox[2];
		PixelEngine::bbox[3] = (p.y > PixelEngine::bbox[3]) ? p.y : PixelEngine::bbox[3];

		return;
	}

	// Now comes the fun part. We must clip the triangles/lines to the viewport - also in software
	Point & p0 = s_bbox_points[0], &p1 = s_bbox_points[1], &p2 = s_bbox_points[2];

	// Check for z-clip. This crude method is required for Mickey's Magical Mirror, at least
	if ((p0.z > 0.0f) || (p1.z > 0.0f) || ((numPoints == 3) && (p2.z > 0.0f)))
		return;

	// Check points for bounds
	u8 b0 = ((p0.x > 0) ? 1 : 0) | ((p0.y > 0) ? 2 : 0) | ((p0.x > 607) ? 4 : 0) | ((p0.y > 479) ? 8 : 0);
	u8 b1 = ((p1.x > 0) ? 1 : 0) | ((p1.y > 0) ? 2 : 0) | ((p1.x > 607) ? 4 : 0) | ((p1.y > 479) ? 8 : 0);

	// Let's be practical... If we only have a line, setting b2 to 3 saves an "if"-clause later on
	u8 b2 = 3;

	// Otherwise if we have a triangle, we need to check the third point
	if (numPoints == 3)
		b2 = ((p2.x > 0) ? 1 : 0) | ((p2.y > 0) ? 2 : 0) | ((p2.x > 607) ? 4 : 0) | ((p2.y > 479) ? 8 : 0);

	// These are the internal bbox vars
	s32 left = 608, right = -1, top = 480, bottom = -1;

	// If the polygon is inside viewport, let's update the bounding box and be done with it
	if ((b0 == 3) && (b0 == b1) && (b0 == b2))
	{
		left = std::min(p0.x, p1.x);
		top = std::min(p0.y, p1.y);
		right = std::max(p0.x, p1.x);
		bottom = std::max(p0.y, p1.y);

		// Triangle
		if (numPoints == 3)
		{
			left = std::min(left, p2.x);
			top = std::min(top, p2.y);
			right = std::max(right, p2.x);
			bottom = std::max(bottom, p2.y);
		}

		// Update bounding box
		PixelEngine::bbox[0] = (left   < PixelEngine::bbox[0]) ? left : PixelEngine::bbox[0];
		PixelEngine::bbox[1] = (right  > PixelEngine::bbox[1]) ? right : PixelEngine::bbox[1];
		PixelEngine::bbox[2] = (top    < PixelEngine::bbox[2]) ? top : PixelEngine::bbox[2];
		PixelEngine::bbox[3] = (bottom > PixelEngine::bbox[3]) ? bottom : PixelEngine::bbox[3];

		return;
	}

	// If it is not inside, then either it is completely outside, or it needs clipping.
	// Check the primitive's lines
	u8 i0 = b0 ^ b1;
	u8 i1 = (numPoints == 3) ? (b1 ^ b2) : i0;
	u8 i2 = (numPoints == 3) ? (b0 ^ b2) : i0;

	// Primitive out of bounds - return
	if (!(i0 | i1 | i2))
		return;

	// First point inside viewport - update internal bbox
	if (b0 == 3)
	{
		left = p0.x;
		top = p0.y;
		right = p0.x;
		bottom = p0.y;
	}

	// Second point inside
	if (b1 == 3)
	{
		left = std::min(p1.x, left);
		top = std::min(p1.y, top);
		right = std::max(p1.x, right);
		bottom = std::max(p1.y, bottom);
	}

	// Third point inside
	if ((b2 == 3) && (numPoints == 3))
	{
		left = std::min(p2.x, left);
		top = std::min(p2.y, top);
		right = std::max(p2.x, right);
		bottom = std::max(p2.y, bottom);
	}

	// Triangle equation vars
	float m, c;

	// Some definitions to help with rounding later on
	const float highNum = 89374289734.0f;
	const float roundUp = 0.001f;

	// Intersection result
	s32 s;

	// First line intersects
	if (i0)
	{
		m = (p1.x - p0.x) ? ((p1.y - p0.y) / (p1.x - p0.x)) : highNum;
		c = p0.y - (m * p0.x);
		if (i0 & 1)
		{
			s = (s32)(c + roundUp);
			if (s >= 0 && s <= 479)
				left = 0;
			top = std::min(s, top);
			bottom = std::max(s, bottom);
		}

		if (i0 & 2)
		{
			s = (s32)((-c / m) + roundUp);
			if (s >= 0 && s <= 607)
				top = 0;
			left = std::min(s, left);
			right = std::max(s, right);
		}

		if (i0 & 4)
		{
			s = (s32)((m * 607) + c + roundUp);
			if (s >= 0 && s <= 479)
				right = 607;
			top = std::min(s, top);
			bottom = std::max(s, bottom);
		}

		if (i0 & 8)
		{
			s = (s32)(((479 - c) / m) + roundUp);
			if (s >= 0 && s <= 607)
				bottom = 479;
			left = std::min(s, left);
			right = std::max(s, right);
		}
	}

	// Only check other lines if we are dealing with a triangle
	if (numPoints == 3)
	{
		// Second line intersects
		if (i1)
		{
			m = (p2.x - p1.x) ? ((p2.y - p1.y) / (p2.x - p1.x)) : highNum;
			c = p1.y - (m * p1.x);
			if (i1 & 1)
			{
				s = (s32)(c + roundUp);
				if (s >= 0 && s <= 479)
					left = 0;
				top = std::min(s, top);
				bottom = std::max(s, bottom);
			}

			if (i1 & 2)
			{
				s = (s32)((-c / m) + roundUp);
				if (s >= 0 && s <= 607)
					top = 0;
				left = std::min(s, left);
				right = std::max(s, right);
			}

			if (i1 & 4)
			{
				s = (s32)((m * 607) + c + roundUp);
				if (s >= 0 && s <= 479)
					right = 607;
				top = std::min(s, top);
				bottom = std::max(s, bottom);
			}

			if (i1 & 8)
			{
				s = (s32)(((479 - c) / m) + roundUp);
				if (s >= 0 && s <= 607)
					bottom = 479;
				left = std::min(s, left);
				right = std::max(s, right);
			}
		}

		// Third line intersects
		if (i2)
		{
			m = (p2.x - p0.x) ? ((p2.y - p0.y) / (p2.x - p0.x)) : highNum;
			c = p0.y - (m * p0.x);
			if (i2 & 1)
			{
				s = (s32)(c + roundUp);
				if (s >= 0 && s <= 479)
					left = 0;
				top = std::min(s, top);
				bottom = std::max(s, bottom);
			}

			if (i2 & 2)
			{
				s = (s32)((-c / m) + roundUp);
				if (s >= 0 && s <= 607)
					top = 0;
				left = std::min(s, left);
				right = std::max(s, right);
			}

			if (i2 & 4)
			{
				s = (s32)((m * 607) + c + roundUp);
				if (s >= 0 && s <= 479)
					right = 607;
				top = std::min(s, top);
				bottom = std::max(s, bottom);
			}

			if (i2 & 8)
			{
				s = (s32)(((479 - c) / m) + roundUp);
				if (s >= 0 && s <= 607)
					bottom = 479;
				left = std::min(s, left);
				right = std::max(s, right);
			}
		}
	}

	// Wrong bounding box values, discard this polygon (it is outside)
	if (left > 607 || top > 479 || right < 0 || bottom < 0)
		return;

	// Trim bounding box to viewport
	left = (left   < 0) ? 0 : left;
	top = (top    < 0) ? 0 : top;
	right = (right  > 607) ? 607 : right;
	bottom = (bottom > 479) ? 479 : bottom;

	// Update bounding box
	PixelEngine::bbox[0] = (left   < PixelEngine::bbox[0]) ? left : PixelEngine::bbox[0];
	PixelEngine::bbox[1] = (right  > PixelEngine::bbox[1]) ? right : PixelEngine::bbox[1];
	PixelEngine::bbox[2] = (top    < PixelEngine::bbox[2]) ? top : PixelEngine::bbox[2];
	PixelEngine::bbox[3] = (bottom > PixelEngine::bbox[3]) ? bottom : PixelEngine::bbox[3];
}

static void LOADERDECL TexMtx_ReadDirect_UByte()
{
	s_curtexmtx[s_texmtxread] = DataReadU8() & 0x3f;
	PRIM_LOG("texmtx%d: %d, ", s_texmtxread, s_curtexmtx[s_texmtxread]);
	s_texmtxread++;
}

static void LOADERDECL TexMtx_Write_Float()
{
	DataWrite(float(s_curtexmtx[s_texmtxwrite++]));
}

static void LOADERDECL TexMtx_Write_Float2()
{
	DataWrite(0.f);
	DataWrite(float(s_curtexmtx[s_texmtxwrite++]));
}

static void LOADERDECL TexMtx_Write_Float4()
{
	DataWrite(0.f);
	DataWrite(0.f);
	DataWrite(float(s_curtexmtx[s_texmtxwrite++]));
	// Just to fill out with 0.
	DataWrite(0.f);
}

VertexLoader::VertexLoader(const TVtxDesc &vtx_desc, const VAT &vtx_attr)
{
	m_compiledCode = nullptr;
	m_numLoadedVertices = 0;
	m_VertexSize = 0;
	m_native_vertex_format = nullptr;
	loop_counter = 0;
	VertexLoader_Normal::Init();
	VertexLoader_Position::Init();
	VertexLoader_TextCoord::Init();

	m_VtxDesc = vtx_desc;
	SetVAT(vtx_attr);

	#ifdef USE_VERTEX_LOADER_JIT
	AllocCodeSpace(COMPILED_CODE_SIZE);
	CompileVertexTranslator();
	WriteProtect();
	#else
	m_numPipelineStages = 0;
	CompileVertexTranslator();
	#endif

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
	// We don't use any callee saved registers or anything but RAX.
	ABI_PushRegistersAndAdjustStack(0, 8);

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
#else
	// Reset pipeline
	m_numPipelineStages = 0;
#endif

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
	if (g_ActiveConfig.bUseBBox)
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
			Host_SysMessage(
				StringFromFormat("VertexLoader_Normal::GetFunction(%i %i %i %i) returned zero!",
				(u32)m_VtxDesc.Normal, m_VtxAttr.NormalFormat,
				m_VtxAttr.NormalElements, m_VtxAttr.NormalIndex3).c_str());
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

	m_native_components = components;
	m_native_vtx_decl.stride = nat_offset;

#ifdef USE_VERTEX_LOADER_JIT
	// End loop here
	MOV(64, R(RAX), Imm64((u64)&loop_counter));
	SUB(32, MatR(RAX), Imm8(1));

	J_CC(CC_NZ, loop_start);
	ABI_PopRegistersAndAdjustStack(0, 8);
	RET();
#endif
}

void VertexLoader::WriteCall(TPipelineFunction func)
{
#ifdef USE_VERTEX_LOADER_JIT
	MOV(64, R(RAX), Imm64((u64)func));
	CALLptr(R(RAX));
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

void VertexLoader::SetupRunVertices(const VAT& vat, int primitive, int const count)
{
	m_numLoadedVertices += count;

	// Load position and texcoord scale factors.
	m_VtxAttr.PosFrac          = vat.g0.PosFrac;
	m_VtxAttr.texCoord[0].Frac = vat.g0.Tex0Frac;
	m_VtxAttr.texCoord[1].Frac = vat.g1.Tex1Frac;
	m_VtxAttr.texCoord[2].Frac = vat.g1.Tex2Frac;
	m_VtxAttr.texCoord[3].Frac = vat.g1.Tex3Frac;
	m_VtxAttr.texCoord[4].Frac = vat.g2.Tex4Frac;
	m_VtxAttr.texCoord[5].Frac = vat.g2.Tex5Frac;
	m_VtxAttr.texCoord[6].Frac = vat.g2.Tex6Frac;
	m_VtxAttr.texCoord[7].Frac = vat.g2.Tex7Frac;

	posScale = fractionTable[m_VtxAttr.PosFrac];
	if (m_native_components & VB_HAS_UVALL)
		for (int i = 0; i < 8; i++)
			tcScale[i] = fractionTable[m_VtxAttr.texCoord[i].Frac];
	for (int i = 0; i < 2; i++)
		colElements[i] = m_VtxAttr.color[i].Elements;

	// Prepare bounding box
	s_bbox_primitive = primitive;
	s_bbox_currPoint = 0;
	s_bbox_loadedPoints = 0;
}

void VertexLoader::ConvertVertices ( int count )
{
#ifdef USE_VERTEX_LOADER_JIT
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

void VertexLoader::RunVertices(const VAT& vat, int primitive, int const count)
{
	SetupRunVertices(vat, primitive, count);
	ConvertVertices(count);
}

void VertexLoader::SetVAT(const VAT& vat)
{
	m_VtxAttr.PosElements          = vat.g0.PosElements;
	m_VtxAttr.PosFormat            = vat.g0.PosFormat;
	m_VtxAttr.PosFrac              = vat.g0.PosFrac;
	m_VtxAttr.NormalElements       = vat.g0.NormalElements;
	m_VtxAttr.NormalFormat         = vat.g0.NormalFormat;
	m_VtxAttr.color[0].Elements    = vat.g0.Color0Elements;
	m_VtxAttr.color[0].Comp        = vat.g0.Color0Comp;
	m_VtxAttr.color[1].Elements    = vat.g0.Color1Elements;
	m_VtxAttr.color[1].Comp        = vat.g0.Color1Comp;
	m_VtxAttr.texCoord[0].Elements = vat.g0.Tex0CoordElements;
	m_VtxAttr.texCoord[0].Format   = vat.g0.Tex0CoordFormat;
	m_VtxAttr.texCoord[0].Frac     = vat.g0.Tex0Frac;
	m_VtxAttr.ByteDequant          = vat.g0.ByteDequant;
	m_VtxAttr.NormalIndex3         = vat.g0.NormalIndex3;

	m_VtxAttr.texCoord[1].Elements = vat.g1.Tex1CoordElements;
	m_VtxAttr.texCoord[1].Format   = vat.g1.Tex1CoordFormat;
	m_VtxAttr.texCoord[1].Frac     = vat.g1.Tex1Frac;
	m_VtxAttr.texCoord[2].Elements = vat.g1.Tex2CoordElements;
	m_VtxAttr.texCoord[2].Format   = vat.g1.Tex2CoordFormat;
	m_VtxAttr.texCoord[2].Frac     = vat.g1.Tex2Frac;
	m_VtxAttr.texCoord[3].Elements = vat.g1.Tex3CoordElements;
	m_VtxAttr.texCoord[3].Format   = vat.g1.Tex3CoordFormat;
	m_VtxAttr.texCoord[3].Frac     = vat.g1.Tex3Frac;
	m_VtxAttr.texCoord[4].Elements = vat.g1.Tex4CoordElements;
	m_VtxAttr.texCoord[4].Format   = vat.g1.Tex4CoordFormat;

	m_VtxAttr.texCoord[4].Frac     = vat.g2.Tex4Frac;
	m_VtxAttr.texCoord[5].Elements = vat.g2.Tex5CoordElements;
	m_VtxAttr.texCoord[5].Format   = vat.g2.Tex5CoordFormat;
	m_VtxAttr.texCoord[5].Frac     = vat.g2.Tex5Frac;
	m_VtxAttr.texCoord[6].Elements = vat.g2.Tex6CoordElements;
	m_VtxAttr.texCoord[6].Format   = vat.g2.Tex6CoordFormat;
	m_VtxAttr.texCoord[6].Frac     = vat.g2.Tex6Frac;
	m_VtxAttr.texCoord[7].Elements = vat.g2.Tex7CoordElements;
	m_VtxAttr.texCoord[7].Format   = vat.g2.Tex7CoordFormat;
	m_VtxAttr.texCoord[7].Frac     = vat.g2.Tex7Frac;

	if (!m_VtxAttr.ByteDequant)
	{
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
		m_VertexSize, (u32)m_VtxDesc.PosMatIdx,
		m_VtxAttr.PosElements ? 3 : 2, posMode[m_VtxDesc.Position], posFormats[m_VtxAttr.PosFormat]));

	if (m_VtxDesc.Normal)
	{
		dest->append(StringFromFormat("Nrm: %i %s-%s ",
			m_VtxAttr.NormalElements, posMode[m_VtxDesc.Normal], posFormats[m_VtxAttr.NormalFormat]));
	}

	u64 color_mode[2] = {m_VtxDesc.Color0, m_VtxDesc.Color1};
	for (int i = 0; i < 2; i++)
	{
		if (color_mode[i])
		{
			dest->append(StringFromFormat("C%i: %i %s-%s ", i, m_VtxAttr.color[i].Elements, posMode[color_mode[i]], colorFormat[m_VtxAttr.color[i].Comp]));
		}
	}
	u64 tex_mode[8] = {
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

NativeVertexFormat* VertexLoader::GetNativeVertexFormat()
{
	if (m_native_vertex_format)
		return m_native_vertex_format;
	auto& native = s_native_vertex_map[m_native_vtx_decl];
	if (!native)
	{
		auto raw_pointer = g_vertex_manager->CreateNativeVertexFormat();
		native = std::unique_ptr<NativeVertexFormat>(raw_pointer);
		native->Initialize(m_native_vtx_decl);
		native->m_components = m_native_components;
	}
	m_native_vertex_format = native.get();
	return native.get();

}

std::map<PortableVertexDeclaration, std::unique_ptr<NativeVertexFormat>> VertexLoader::s_native_vertex_map;
