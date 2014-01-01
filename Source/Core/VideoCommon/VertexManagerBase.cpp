
#include "Common.h"

#include "Statistics.h"
#include "OpcodeDecoding.h"
#include "IndexGenerator.h"
#include "VertexShaderManager.h"
#include "PixelShaderManager.h"
#include "NativeVertexFormat.h"
#include "TextureCacheBase.h"
#include "RenderBase.h"
#include "BPStructs.h"

#include "VertexManagerBase.h"
#include "MainBase.h"
#include "VideoConfig.h"

VertexManager *g_vertex_manager;

u8 *VertexManager::s_pCurBufferPointer;
u8 *VertexManager::s_pBaseBufferPointer;
u8 *VertexManager::s_pEndBufferPointer;

VertexManager::VertexManager()
{
	LocalVBuffer.resize(MAXVBUFFERSIZE);
	s_pCurBufferPointer = s_pBaseBufferPointer = &LocalVBuffer[0];
	s_pEndBufferPointer = s_pBaseBufferPointer + LocalVBuffer.size();

	TIBuffer.resize(MAXIBUFFERSIZE);
	LIBuffer.resize(MAXIBUFFERSIZE);
	PIBuffer.resize(MAXIBUFFERSIZE);

	ResetBuffer();
}

VertexManager::~VertexManager()
{
}

void VertexManager::ResetBuffer()
{
	s_pCurBufferPointer = s_pBaseBufferPointer;
	IndexGenerator::Start(GetTriangleIndexBuffer(), GetLineIndexBuffer(), GetPointIndexBuffer());
}

u32 VertexManager::GetRemainingSize()
{
	return (u32)(s_pEndBufferPointer - s_pCurBufferPointer);
}

void VertexManager::PrepareForAdditionalData(int primitive, u32 count, u32 stride)
{
	u32 const needed_vertex_bytes = count * stride;

	if (count > IndexGenerator::GetRemainingIndices() || count > GetRemainingIndices(primitive) || needed_vertex_bytes > GetRemainingSize())
	{
		Flush();

		if(count > IndexGenerator::GetRemainingIndices())
			ERROR_LOG(VIDEO, "Too little remaining index values. Use 32-bit or reset them on flush.");
		if (count > GetRemainingIndices(primitive))
			ERROR_LOG(VIDEO, "VertexManager: Buffer not large enough for all indices! "
				"Increase MAXIBUFFERSIZE or we need primitive breaking after all.");
		if (needed_vertex_bytes > GetRemainingSize())
			ERROR_LOG(VIDEO, "VertexManager: Buffer not large enough for all vertices! "
				"Increase MAXVBUFFERSIZE or we need primitive breaking after all.");
	}
}

bool VertexManager::IsFlushed() const
{
	return s_pBaseBufferPointer == s_pCurBufferPointer;
}

u32 VertexManager::GetRemainingIndices(int primitive)
{

	if(g_Config.backend_info.bSupportsPrimitiveRestart)
	{
		switch (primitive)
		{
		case GX_DRAW_QUADS:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 5 * 4;
		case GX_DRAW_TRIANGLES:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 4 * 3;
		case GX_DRAW_TRIANGLE_STRIP:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 1 - 1;
		case GX_DRAW_TRIANGLE_FAN:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 6 * 4 + 1;

		case GX_DRAW_LINES:
			return (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen());
		case GX_DRAW_LINE_STRIP:
			return (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen()) / 2 + 1;

		case GX_DRAW_POINTS:
			return (MAXIBUFFERSIZE - IndexGenerator::GetPointindexLen());

		default:
			return 0;
		}
	}
	else
	{
		switch (primitive)
		{
		case GX_DRAW_QUADS:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 6 * 4;
		case GX_DRAW_TRIANGLES:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen());
		case GX_DRAW_TRIANGLE_STRIP:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 3 + 2;
		case GX_DRAW_TRIANGLE_FAN:
			return (MAXIBUFFERSIZE - IndexGenerator::GetTriangleindexLen()) / 3 + 2;

		case GX_DRAW_LINES:
			return (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen());
		case GX_DRAW_LINE_STRIP:
			return (MAXIBUFFERSIZE - IndexGenerator::GetLineindexLen()) / 2 + 1;

		case GX_DRAW_POINTS:
			return (MAXIBUFFERSIZE - IndexGenerator::GetPointindexLen());

		default:
			return 0;
		}
	}
}

void VertexManager::AddVertices(int primitive, u32 numVertices)
{
	if (numVertices <= 0)
		return;

	ADDSTAT(stats.thisFrame.numPrims, numVertices);
	INCSTAT(stats.thisFrame.numPrimitiveJoins);

	IndexGenerator::AddIndices(primitive, numVertices);
}

void VertexManager::Flush()
{
	if (g_vertex_manager->IsFlushed())
		return;

	// loading a state will invalidate BP, so check for it
	g_video_backend->CheckInvalidState();

	VideoFifo_CheckEFBAccess();

	// TODO: need to merge more stuff into VideoCommon
	g_vertex_manager->vFlush();

	g_vertex_manager->ResetBuffer();
}

void VertexManager::DoState(PointerWrap& p)
{
	g_vertex_manager->vDoState(p);
}

void VertexManager::DoStateShared(PointerWrap& p)
{
	// It seems we half-assume to be flushed here
	// We update s_pCurBufferPointer yet don't worry about IndexGenerator's outdated pointers
	// and maybe other things are overlooked

	p.Do(LocalVBuffer);
	p.Do(TIBuffer);
	p.Do(LIBuffer);
	p.Do(PIBuffer);

	s_pBaseBufferPointer = &LocalVBuffer[0];
	s_pEndBufferPointer = s_pBaseBufferPointer + LocalVBuffer.size();
	p.DoPointer(s_pCurBufferPointer, s_pBaseBufferPointer);
}
