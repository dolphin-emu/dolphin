// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common.h"
#include "FileUtil.h"

#include "D3DBase.h"
#include "Fifo.h"
#include "Statistics.h"
#include "VertexManager.h"
#include "OpcodeDecoding.h"
#include "IndexGenerator.h"
#include "VertexShaderManager.h"
#include "VertexShaderCache.h"
#include "PixelShaderManager.h"
#include "PixelShaderCache.h"
#include "NativeVertexFormat.h"
#include "TextureCache.h"
#include "main.h"

#include "BPStructs.h"
#include "XFStructs.h"
#include "Debugger.h"
#include "VideoConfig.h"

// internal state for loading vertices
extern NativeVertexFormat *g_nativeVertexFmt;
namespace DX9
{
//This are the initially requeted size for the buffers expresed in elements
const u32 IBUFFER_SIZE = VertexManager::MAXIBUFFERSIZE * sizeof(u16) * 8;
const u32 VBUFFER_SIZE = VertexManager::MAXVBUFFERSIZE;
const u32 MAX_VBUFFER_COUNT = 2;

inline void DumpBadShaders()
{
#if defined(_DEBUG) || defined(DEBUGFAST)
	// TODO: Reimplement!
/*	std::string error_shaders;
	error_shaders.append(VertexShaderCache::GetCurrentShaderCode());
	error_shaders.append(PixelShaderCache::GetCurrentShaderCode());
	char filename[512] = "bad_shader_combo_0.txt";
	int which = 0;
	while (File::Exists(filename))
	{
		which++;
		sprintf(filename, "bad_shader_combo_%i.txt", which);
	}
	File::WriteStringToFile(true, error_shaders, filename);
	PanicAlert("DrawIndexedPrimitiveUP failed. Shaders written to %s", filename);*/
#endif
}

void VertexManager::CreateDeviceObjects()
{
	m_buffers_count = 0;
	m_vertex_buffers = NULL;
	m_index_buffers = NULL;
	D3DCAPS9 DeviceCaps = D3D::GetCaps();
	u32 devicevMaxBufferSize =  DeviceCaps.MaxPrimitiveCount * 3 * DeviceCaps.MaxStreamStride;
	//Calculate Device Dependant size
	m_vertex_buffer_size = (VBUFFER_SIZE > devicevMaxBufferSize) ? devicevMaxBufferSize : VBUFFER_SIZE;
	m_index_buffer_size = (IBUFFER_SIZE > DeviceCaps.MaxVertexIndex) ? DeviceCaps.MaxVertexIndex : IBUFFER_SIZE;
	//if device caps are not enough for Vbuffer fall back to vertex arrays
	if (m_index_buffer_size < MAXIBUFFERSIZE || m_vertex_buffer_size < MAXVBUFFERSIZE) return;
	
	m_vertex_buffers = new LPDIRECT3DVERTEXBUFFER9[MAX_VBUFFER_COUNT];
	m_index_buffers = new LPDIRECT3DINDEXBUFFER9[MAX_VBUFFER_COUNT];

	bool Fail = false;
	for (m_current_vertex_buffer = 0; m_current_vertex_buffer < MAX_VBUFFER_COUNT; m_current_vertex_buffer++)
	{
		m_vertex_buffers[m_current_vertex_buffer] = NULL;
		m_index_buffers[m_current_vertex_buffer] = NULL;
	}
	for (m_current_vertex_buffer = 0; m_current_vertex_buffer < MAX_VBUFFER_COUNT; m_current_vertex_buffer++)
	{
		if(FAILED( D3D::dev->CreateVertexBuffer( m_vertex_buffer_size,  D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &m_vertex_buffers[m_current_vertex_buffer], NULL ) ) )
		{
			Fail = true;
			break;
		}
		if( FAILED( D3D::dev->CreateIndexBuffer( m_index_buffer_size * sizeof(u16),  D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &m_index_buffers[m_current_vertex_buffer], NULL ) ) )
		{
			Fail = true;
			return;
		}
	}
	m_buffers_count = m_current_vertex_buffer;
	m_current_vertex_buffer = 0;
	m_current_index_buffer = 0;
	m_index_buffer_cursor = m_index_buffer_size;
	m_vertex_buffer_cursor = m_vertex_buffer_size;
	m_current_stride = 0;
	if (Fail)
	{
		m_buffers_count--;
		if (m_buffers_count < 2)
		{
			//Error creating Vertex buffers. clean and fall to Vertex arrays
			m_buffers_count = MAX_VBUFFER_COUNT;
			DestroyDeviceObjects();
		}		
	}	
}
void VertexManager::DestroyDeviceObjects()
{
	D3D::SetStreamSource( 0, NULL, 0, 0);
	D3D::SetIndices(NULL);
	for (int i = 0; i < MAX_VBUFFER_COUNT; i++)
	{
		if(m_vertex_buffers)
		{
			if (m_vertex_buffers[i])
			{
				m_vertex_buffers[i]->Release();
				m_vertex_buffers[i] = NULL;
			}
		}

		if (m_index_buffers[i])
		{
			m_index_buffers[i]->Release();
			m_index_buffers[i] = NULL;
		}
	}
	if(m_vertex_buffers)
		delete [] m_vertex_buffers;
	if(m_index_buffers)
		delete [] m_index_buffers;
	m_vertex_buffers = NULL;
	m_index_buffers = NULL;
}

void VertexManager::PrepareDrawBuffers(u32 stride)
{
	if (!m_buffers_count)
	{
		return;
	}
	u8* pVertices;
	u16* pIndices;
	int datasize = IndexGenerator::GetNumVerts() * stride;
	int TdataSize = IndexGenerator::GetTriangleindexLen();
	int LDataSize = IndexGenerator::GetLineindexLen();
	int PDataSize = IndexGenerator::GetPointindexLen();
	int IndexDataSize = TdataSize + LDataSize;
	DWORD LockMode = D3DLOCK_NOOVERWRITE;
	m_vertex_buffer_cursor--;
	m_vertex_buffer_cursor = m_vertex_buffer_cursor - (m_vertex_buffer_cursor % stride) + stride;
	if (m_vertex_buffer_cursor > m_vertex_buffer_size - datasize)
	{
		LockMode = D3DLOCK_DISCARD;
		m_vertex_buffer_cursor = 0;
		m_current_vertex_buffer = (m_current_vertex_buffer + 1) % m_buffers_count;		
	}	
	if(FAILED(m_vertex_buffers[m_current_vertex_buffer]->Lock(m_vertex_buffer_cursor, datasize,(VOID**)(&pVertices), LockMode))) 
	{
		DestroyDeviceObjects();
		return;
	}
	memcpy(pVertices, s_pBaseBufferPointer, datasize);
	m_vertex_buffers[m_current_vertex_buffer]->Unlock();

	LockMode = D3DLOCK_NOOVERWRITE;
	if (m_index_buffer_cursor > m_index_buffer_size - IndexDataSize)
	{
		LockMode = D3DLOCK_DISCARD;
		m_index_buffer_cursor = 0;
		m_current_index_buffer = (m_current_index_buffer + 1) % m_buffers_count;		
	}	
	
	if(FAILED(m_index_buffers[m_current_index_buffer]->Lock(m_index_buffer_cursor * sizeof(u16), IndexDataSize * sizeof(u16), (VOID**)(&pIndices), LockMode ))) 
	{
		DestroyDeviceObjects();
		return;
	}
	if(TdataSize)
	{		
		memcpy(pIndices, GetTriangleIndexBuffer(), TdataSize * sizeof(u16));
		pIndices += TdataSize;
	}
	if(LDataSize)
	{		
		memcpy(pIndices, GetLineIndexBuffer(), LDataSize * sizeof(u16));
		pIndices += LDataSize;
	}
	m_index_buffers[m_current_index_buffer]->Unlock();
	if(m_current_stride != stride || m_vertex_buffer_cursor == 0)
	{
		m_current_stride = stride;
		D3D::SetStreamSource( 0, m_vertex_buffers[m_current_vertex_buffer], 0, stride);
	}
	if (m_index_buffer_cursor == 0)
	{
		D3D::SetIndices(m_index_buffers[m_current_index_buffer]);
	}
	
	ADDSTAT(stats.thisFrame.bytesVertexStreamed, datasize);
	ADDSTAT(stats.thisFrame.bytesIndexStreamed, IndexDataSize);
}

void VertexManager::DrawVertexBuffer(int stride)
{
	int triangles = IndexGenerator::GetNumTriangles();
	int lines = IndexGenerator::GetNumLines();
	int points = IndexGenerator::GetNumPoints();
	int numverts = IndexGenerator::GetNumVerts();
	int StartIndex = m_index_buffer_cursor;
	int basevertex = m_vertex_buffer_cursor / stride;
	if (triangles > 0)
	{
		if (FAILED(D3D::dev->DrawIndexedPrimitive(
			D3DPT_TRIANGLELIST,
			basevertex,
			0, 
			numverts,
			StartIndex, 
			triangles)))
		{
			DumpBadShaders();
		}
		StartIndex += IndexGenerator::GetTriangleindexLen();
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (lines > 0)
	{
		if (FAILED(D3D::dev->DrawIndexedPrimitive(
			D3DPT_LINELIST, 
			basevertex,
			0, 
			numverts,
			StartIndex, 
			IndexGenerator::GetNumLines())))
		{
			DumpBadShaders();
		}
		StartIndex += IndexGenerator::GetLineindexLen();
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (points > 0)
	{
		//DrawIndexedPrimitive does not support point list so we have to draw the points one by one
		for (int i = 0; i < points; i++)
		{
			if (FAILED(D3D::dev->DrawPrimitive(
			D3DPT_POINTLIST, 
			basevertex + GetPointIndexBuffer()[i],
			1)))
			{
				DumpBadShaders();
			}
			INCSTAT(stats.thisFrame.numDrawCalls);
		}
		
		
	}
	
}

void VertexManager::DrawVertexArray(int stride)
{
	int triangles = IndexGenerator::GetNumTriangles();
	int lines = IndexGenerator::GetNumLines();
	int points = IndexGenerator::GetNumPoints();
	int numverts = IndexGenerator::GetNumVerts();
	if (triangles > 0)
	{
		if (FAILED(D3D::dev->DrawIndexedPrimitiveUP(
			D3DPT_TRIANGLELIST, 
			0, numverts, triangles, 
			GetTriangleIndexBuffer(), 
			D3DFMT_INDEX16, 
			s_pBaseBufferPointer, 
			stride)))
		{
			DumpBadShaders();
		}
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (lines > 0)
	{
		if (FAILED(D3D::dev->DrawIndexedPrimitiveUP(
			D3DPT_LINELIST, 
			0, numverts, lines, 
			GetLineIndexBuffer(), 
			D3DFMT_INDEX16, 
			s_pBaseBufferPointer, 
			stride)))
		{
			DumpBadShaders();
		}
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}
	if (points > 0)
	{
		if (FAILED(D3D::dev->DrawIndexedPrimitiveUP(
			D3DPT_POINTLIST, 
			0, numverts, points, 
			GetPointIndexBuffer(), 
			D3DFMT_INDEX16, 
			s_pBaseBufferPointer, 
			stride)))
		{
			DumpBadShaders();
		}
		INCSTAT(stats.thisFrame.numIndexedDrawCalls);
	}	
}

void VertexManager::vFlush()
{
	u32 usedtextures = 0;
	for (u32 i = 0; i < (u32)bpmem.genMode.numtevstages + 1; ++i)
		if (bpmem.tevorders[i / 2].getEnable(i & 1))
			usedtextures |= 1 << bpmem.tevorders[i/2].getTexMap(i & 1);

	if (bpmem.genMode.numindstages > 0)
		for (unsigned int i = 0; i < bpmem.genMode.numtevstages + 1; ++i)
			if (bpmem.tevind[i].IsActive() && bpmem.tevind[i].bt < bpmem.genMode.numindstages)
				usedtextures |= 1 << bpmem.tevindref.getTexMap(bpmem.tevind[i].bt);

	for (unsigned int i = 0; i < 8; i++)
	{
		if (usedtextures & (1 << i))
		{
			g_renderer->SetSamplerState(i & 3, i >> 2);
			FourTexUnits &tex = bpmem.tex[i >> 2];
			TextureCache::TCacheEntryBase* tentry = TextureCache::Load(i, 
				(tex.texImage3[i&3].image_base/* & 0x1FFFFF*/) << 5,
				tex.texImage0[i&3].width + 1, tex.texImage0[i&3].height + 1,
				tex.texImage0[i&3].format, tex.texTlut[i&3].tmem_offset<<9, 
				tex.texTlut[i&3].tlut_format,
				(tex.texMode0[i&3].min_filter & 3),
				(tex.texMode1[i&3].max_lod + 0xf) / 0x10,
				tex.texImage1[i&3].image_type);

			if (tentry)
			{
				// 0s are probably for no manual wrapping needed.
				PixelShaderManager::SetTexDims(i, tentry->native_width, tentry->native_height, 0, 0);
			}
			else
				ERROR_LOG(VIDEO, "Error loading texture");
		}
	}

	// set global constants
	VertexShaderManager::SetConstants();
	PixelShaderManager::SetConstants(g_nativeVertexFmt->m_components);
	u32 stride = g_nativeVertexFmt->GetVertexStride();
	bool useDstAlpha = !g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate &&
						bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24;
	bool useDualSource = useDstAlpha && g_ActiveConfig.backend_info.bSupportsDualSourceBlend;
	DSTALPHA_MODE AlphaMode = useDualSource ? DSTALPHA_DUAL_SOURCE_BLEND : DSTALPHA_NONE;	

	if (!PixelShaderCache::SetShader(AlphaMode ,g_nativeVertexFmt->m_components))
	{
		GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set pixel shader\n");});
		goto shader_fail;
	}
	if (!VertexShaderCache::SetShader(g_nativeVertexFmt->m_components))
	{
		GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set vertex shader\n");});
		goto shader_fail;

	}
	PrepareDrawBuffers(stride);
	g_nativeVertexFmt->SetupVertexPointers();
	g_perf_query->EnableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);
	if(m_buffers_count)
	{
		DrawVertexBuffer(stride);
	} 
	else 
	{ 
		DrawVertexArray(stride);
	}
	g_perf_query->DisableQuery(bpmem.zcontrol.early_ztest ? PQG_ZCOMP_ZCOMPLOC : PQG_ZCOMP);
	if (useDstAlpha && !useDualSource)
	{
		if (!PixelShaderCache::SetShader(DSTALPHA_ALPHA_PASS, g_nativeVertexFmt->m_components))
		{
			GFX_DEBUGGER_PAUSE_LOG_AT(NEXT_ERROR,true,{printf("Fail to set pixel shader\n");});
			goto shader_fail;
		}
		// update alpha only
		g_renderer->ApplyState(true);
		if(m_buffers_count)
		{ 
			DrawVertexBuffer(stride);
		} 
		else 
		{ 
			DrawVertexArray(stride);
		}
		g_renderer->RestoreState();
	}
	GFX_DEBUGGER_PAUSE_AT(NEXT_FLUSH, true);

shader_fail:
	if(m_buffers_count)
	{
		m_index_buffer_cursor += IndexGenerator::GetTriangleindexLen() + IndexGenerator::GetLineindexLen() + IndexGenerator::GetPointindexLen();
		m_vertex_buffer_cursor += IndexGenerator::GetNumVerts() * stride;
	}
}

}
