
#include "Common.h"

#include "Statistics.h"
#include "OpcodeDecoding.h"
#include "IndexGenerator.h"

#include "VertexManagerBase.h"

VertexManager *g_vertex_manager;

u8 *VertexManager::s_pCurBufferPointer;
u8 *VertexManager::s_pBaseBufferPointer;

u8 *VertexManager::LocalVBuffer;
u16 *VertexManager::TIBuffer;
u16 *VertexManager::LIBuffer;
u16 *VertexManager::PIBuffer;

bool VertexManager::Flushed;

VertexManager::VertexManager()
{
	Flushed = false;

	LocalVBuffer = new u8[MAXVBUFFERSIZE];
	s_pCurBufferPointer = s_pBaseBufferPointer = LocalVBuffer;

	TIBuffer = new u16[MAXIBUFFERSIZE];
	LIBuffer = new u16[MAXIBUFFERSIZE];
	PIBuffer = new u16[MAXIBUFFERSIZE];

	IndexGenerator::Start(TIBuffer, LIBuffer, PIBuffer);
}

void VertexManager::ResetBuffer()
{
	s_pCurBufferPointer = LocalVBuffer;
}

VertexManager::~VertexManager()
{
	delete[] LocalVBuffer;

	delete[] TIBuffer;
	delete[] LIBuffer;
	delete[] PIBuffer;

	// TODO: necessary??
	ResetBuffer();
}

void VertexManager::AddIndices(int primitive, int numVertices)
{
	//switch (primitive)
	//{
	//case GX_DRAW_QUADS:          IndexGenerator::AddQuads(numVertices);		break;
	//case GX_DRAW_TRIANGLES:      IndexGenerator::AddList(numVertices);		break;
	//case GX_DRAW_TRIANGLE_STRIP: IndexGenerator::AddStrip(numVertices);		break;
	//case GX_DRAW_TRIANGLE_FAN:   IndexGenerator::AddFan(numVertices);		break;
	//case GX_DRAW_LINES:		   IndexGenerator::AddLineList(numVertices);	break;
	//case GX_DRAW_LINE_STRIP:     IndexGenerator::AddLineStrip(numVertices);	break;
	//case GX_DRAW_POINTS:         IndexGenerator::AddPoints(numVertices);	break;
	//}

	static void (*const primitive_table[])(int) =
	{
		IndexGenerator::AddQuads,
		NULL,
		IndexGenerator::AddList,
		IndexGenerator::AddStrip,
		IndexGenerator::AddFan,
		IndexGenerator::AddLineList,
		IndexGenerator::AddLineStrip,
		IndexGenerator::AddPoints,
	};

	primitive_table[primitive](numVertices);
}

int VertexManager::GetRemainingSize()
{
	return MAXVBUFFERSIZE - (int)(s_pCurBufferPointer - LocalVBuffer);
}

int VertexManager::GetRemainingVertices(int primitive)
{
	switch (primitive)
	{
	case GX_DRAW_QUADS:
	case GX_DRAW_TRIANGLES:
	case GX_DRAW_TRIANGLE_STRIP:
	case GX_DRAW_TRIANGLE_FAN:
		return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 3;
		break;

	case GX_DRAW_LINES:
	case GX_DRAW_LINE_STRIP:
		return (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen()) / 2;
		break;

	case GX_DRAW_POINTS:
		return (MAXIBUFFERSIZE - IndexGenerator::GetPointindexLen());
		break;

	default:
		return 0;
		break;
	}
}

void VertexManager::AddVertices(int primitive, int numVertices)
{
	if (numVertices <= 0)
		return;

	switch (primitive)
	{
	case GX_DRAW_QUADS:
	case GX_DRAW_TRIANGLES:
	case GX_DRAW_TRIANGLE_STRIP:
	case GX_DRAW_TRIANGLE_FAN:
		if (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen() < 3 * numVertices)
			Flush();
		break;

	case GX_DRAW_LINES:
	case GX_DRAW_LINE_STRIP:
		if (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen() < 2 * numVertices)
			Flush();
		break;

	case GX_DRAW_POINTS:
		if (MAXIBUFFERSIZE - IndexGenerator::GetPointindexLen() < numVertices)
			Flush();
		break;

	default:
		return;
		break;
	}

	if (Flushed)
	{
		IndexGenerator::Start(TIBuffer, LIBuffer, PIBuffer);
		Flushed = false;
	}

	ADDSTAT(stats.thisFrame.numPrims, numVertices);
	INCSTAT(stats.thisFrame.numPrimitiveJoins);
	AddIndices(primitive, numVertices);
}

// TODO: merge this func, (need to merge TextureCache)
void VertexManager::Flush()
{
	g_vertex_manager->vFlush();
}
