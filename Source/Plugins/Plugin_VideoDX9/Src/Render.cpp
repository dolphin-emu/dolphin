// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <d3dx9.h>

#include "Common.h"
#include "Statistics.h"

#include "Config.h"
#include "main.h"
#include "VertexManager.h"
#include "Render.h"
#include "OpcodeDecoding.h"
#include "BPStructs.h"
#include "XFStructs.h"
#include "D3DPostprocess.h"
#include "D3DUtil.h"
#include "ShaderManager.h"
#include "TextureCache.h"
#include "Utils.h"
#include "EmuWindow.h"

#include <list>

float Renderer::m_x,Renderer::m_y,Renderer::m_width, Renderer::m_height, Renderer::xScale,Renderer::yScale;
std::vector<LPDIRECT3DBASETEXTURE9> Renderer::m_Textures;
DWORD Renderer::m_RenderStates[MaxRenderStates];
DWORD Renderer::m_TextureStageStates[MaxTextureStages][MaxTextureTypes];
DWORD Renderer::m_SamplerStates[MaxSamplerSize][MaxSamplerTypes];
DWORD Renderer::m_FVF;

#define NUMWNDRES 6
extern int g_Res[NUMWNDRES][2];


struct Message
{
	Message(const std::string &msg, u32 dw) : message( msg ), dwTimeStamp( dw )
	{
	}

	std::string message;
    u32 dwTimeStamp;
};
static std::list<Message> s_listMsgs;


void Renderer::Init(SVideoInitialize &_VideoInitialize) 
{
    EmuWindow::SetSize(g_Res[g_Config.iWindowedRes][0], g_Res[g_Config.iWindowedRes][1]);

    D3D::Create(g_Config.iAdapter, EmuWindow::GetWnd(), g_Config.bFullscreen, g_Config.iFSResolution, g_Config.iMultisampleMode);

	D3DVIEWPORT9 vp;
	D3D::dev->GetViewport(&vp);

	m_x = 0;
	m_y = 0;
	m_width  = (float)vp.Width;
	m_height = (float)vp.Height;
	xScale = 640.0f / (float)vp.Width;
	yScale = 480.0f / (float)vp.Height;

	D3D::font.Init();
	Initialize();
}

void Renderer::Shutdown(void)
{
	D3D::font.Shutdown();
	D3D::EndFrame();
	D3D::Close();
}

void Renderer::Initialize(void)
{
	m_FVF = 0;

	m_Textures.reserve( MaxTextureStages );
	for ( int i = 0; i < MaxTextureStages; i++ )
	{
		m_Textures.push_back( NULL );
	}

	for (int i=0; i<8; i++)
	{
		D3D::dev->SetSamplerState(i, D3DSAMP_MAXANISOTROPY, 16);
	}
	ReinitView();

	Postprocess::Initialize();
	Postprocess::BeginFrame();
	D3D::BeginFrame(true, 0);
	VertexManager::BeginFrame();
}

void Renderer::AddMessage(const std::string &message, u32 ms)
{
    s_listMsgs.push_back(Message(message, timeGetTime()+ms));
}

void Renderer::ProcessMessages()
{
	if (s_listMsgs.size() > 0) {
        int left = 25, top = 15;
		std::list<Message>::iterator it = s_listMsgs.begin();
        
        while( it != s_listMsgs.end() ) 
		{
			int time_left = (int)(it->dwTimeStamp - timeGetTime());
			int alpha = 255;

			if(time_left<1024)
			{
				alpha=time_left>>2;
				if(time_left<0) alpha=0;
			}

			alpha <<= 24;

            RenderText(it->message, left+1, top+1, 0x000000|alpha);
            RenderText(it->message, left, top, 0xffff30|alpha);
            top += 15;

            if (time_left <= 0)
                it = s_listMsgs.erase(it);
            else ++it;
        }
    }
}

void Renderer::RenderText(const std::string &text, int left, int top, u32 color)
{
	D3D::font.DrawTextScaled((float)left, (float)top, 20, 20, 0.0f, color, text.c_str(), false);
}

void dumpMatrix(D3DXMATRIX &mtx)
{
	for (int y=0; y<4; y++)
	{
		char temp[256];
		sprintf(temp,"%4.4f %4.4f %4.4f %4.4f",mtx.m[y][0],mtx.m[y][1],mtx.m[y][2],mtx.m[y][3]);
		g_VideoInitialize.pLog(temp, FALSE);
	}
}

void Renderer::ReinitView()
{
	D3DXMATRIX mtx;
	D3DXMatrixIdentity(&mtx);
	D3D::dev->SetTransform(D3DTS_VIEW,&mtx);
	D3D::dev->SetTransform(D3DTS_WORLD,&mtx);

	float width =  (float)D3D::GetDisplayWidth();
	float height = (float)D3D::GetDisplayHeight();

	xScale = width/640.0f;
	yScale = height/480.0f;

	RECT rc = {
		(LONG)(m_x*xScale), (LONG)(m_y*yScale), (LONG)(m_width*xScale), (LONG)(m_height*yScale)
	};
}

void Renderer::SwapBuffers(void)
{
	// center window again
	if (EmuWindow::GetParentWnd())
	{
		RECT rcWindow;
		GetWindowRect(EmuWindow::GetParentWnd(), &rcWindow);

		int width = rcWindow.right - rcWindow.left;
		int height = rcWindow.bottom - rcWindow.top;

		::MoveWindow(EmuWindow::GetWnd(), 0,0,width,height, FALSE);
	//	nBackbufferWidth = width;
	//	nBackbufferHeight = height;
	}

	//Finish up the current frame, print some stats
	Postprocess::FinalizeFrame();
	if (g_Config.bOverlayStats)
	{
		char st[2048];
		char *p = st;
		p+=sprintf(p,"Num textures created: %i\n",stats.numTexturesCreated);
		p+=sprintf(p,"Num textures alive: %i\n",stats.numTexturesAlive);
		p+=sprintf(p,"Num pshaders created: %i\n",stats.numPixelShadersCreated);
		p+=sprintf(p,"Num pshaders alive: %i\n",stats.numPixelShadersAlive);
		p+=sprintf(p,"Num vshaders created: %i\n",stats.numVertexShadersCreated);
		p+=sprintf(p,"Num vshaders alive: %i\n",stats.numVertexShadersAlive);
		p+=sprintf(p,"Num dlists called: %i\n",stats.numDListsCalled);
		p+=sprintf(p,"Num dlists created: %i\n",stats.numDListsCreated);
		p+=sprintf(p,"Num dlists alive: %i\n",stats.numDListsAlive);
		p+=sprintf(p,"Num primitives: %i\n",stats.thisFrame.numPrims);
		p+=sprintf(p,"Num primitive joins: %i\n",stats.thisFrame.numPrimitiveJoins);
		p+=sprintf(p,"Num primitives (DL): %i\n",stats.thisFrame.numDLPrims);
		p+=sprintf(p,"Num XF loads: %i\n",stats.thisFrame.numXFLoads);
		p+=sprintf(p,"Num XF loads (DL): %i\n",stats.thisFrame.numXFLoadsInDL);
		p+=sprintf(p,"Num CP loads: %i\n",stats.thisFrame.numCPLoads);
		p+=sprintf(p,"Num CP loads (DL): %i\n",stats.thisFrame.numCPLoadsInDL);
		p+=sprintf(p,"Num BP loads: %i\n",stats.thisFrame.numBPLoads);
		p+=sprintf(p,"Num BP loads (DL): %i\n",stats.thisFrame.numBPLoadsInDL);

		D3D::font.DrawTextScaled(0,30,20,20,0.0f,0xFF00FFFF,st,false);

		//end frame
	}

	ProcessMessages();

#if defined(DVPROFILE)
    if( g_bWriteProfile ) {
        //g_bWriteProfile = 0;
        static int framenum = 0;
        const int UPDATE_FRAMES = 8;
        if( ++framenum >= UPDATE_FRAMES ) {
            DVProfWrite("prof.txt", UPDATE_FRAMES);
            DVProfClear();
            framenum = 0;
        }
    }
#endif

	D3D::EndFrame();
	//D3D frame is now over
	//////////////////////////////////////////////////////////////////////////
	
	//clean out old stuff from caches
	frameCount++;
	PShaderCache::Cleanup();
	VShaderCache::Cleanup();
	TextureCache::Cleanup();
	//DListCache::Cleanup();

	//////////////////////////////////////////////////////////////////////////
	//Begin new frame
	//Set default viewport and scissor, for the clear to work correctly
	stats.ResetFrame();
	D3DVIEWPORT9 vp;
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = (DWORD)m_width;
	vp.Height = (DWORD)m_height;
	vp.MinZ = 0;
	vp.MaxZ = 0;
	D3D::dev->SetViewport(&vp);
	RECT rc;
	rc.left   = 0; 
	rc.top    = 0;
	rc.right  = (LONG)m_width;
	rc.bottom = (LONG)m_height;
	D3D::dev->SetScissorRect(&rc);

	u32 clearColor = (bpmem.clearcolorAR<<16)|bpmem.clearcolorGB;
//	clearColor |= 0x003F003F;
//	D3D::BeginFrame(true,clearColor,1.0f);
	D3D::BeginFrame(false,clearColor,1.0f);
	// D3D::EnableAlphaToCoverage();

	Postprocess::BeginFrame();
	VertexManager::BeginFrame();

	if (g_Config.bOldCard)
		D3D::font.SetRenderStates(); //compatibility with low end cards
}

void Renderer::Flush(void)
{
	// render the rest of the vertex buffer
	//only to be used for debugging purposes
	//D3D::EndFrame();

	//D3D::BeginFrame(false,0);
}

void Renderer::SetViewport(float* _Viewport)
{
	Viewport* pViewport = (Viewport*)_Viewport;
	D3DVIEWPORT9 vp;
	float x=(pViewport->xOrig-662)*2;
	float y=(pViewport->yOrig-582)*2; //something is wrong, but what??
	y-=16;

	float w=pViewport->wd*2;  //multiply up to real size 
	float h=pViewport->ht*-2; //why is this negative? oh well..

	if (x < 0.0f) x = 0.0f;
	if (y < 0.0f) y = 0.0f;
	if (x > 640.0f) x = 639.0f;
	if (y > 480.0f) y = 479.0f;
	if (w < 0) w=1;
	if (h < 0) h=1;
	if (x+w > 640.0f) w=640-x;
	if (y+h > 480.0f) h=480-y;
	//x=y=0;
	//if(w>0.0f) w=0.0f;
	//if(h<0.0f) h=0.0f;

	vp.X = (DWORD)(x*xScale);
	vp.Y = (DWORD)(y*yScale);
	vp.Width = (DWORD)(w*xScale);
	vp.Height = (DWORD)(h*yScale);
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;

//	char temp[256];
//	sprintf(temp,"Viewport: %i %i %i %i %f %f",vp.X,vp.Y,vp.Width,vp.Height,vp.MinZ,vp.MaxZ);
//	g_VideoInitialize.pLog(temp, FALSE);

	D3D::dev->SetViewport(&vp);
}

void Renderer::SetScissorBox(RECT &rc)
{
	rc.left   = (int)(rc.left   * xScale);
	rc.top    = (int)(rc.top    * yScale);
	rc.right  = (int)(rc.right  * xScale);
	rc.bottom = (int)(rc.bottom * yScale);
	if (rc.right >= rc.left && rc.bottom >= rc.top)
		D3D::dev->SetScissorRect(&rc);
	else
		g_VideoInitialize.pLog("SCISSOR ERROR", FALSE);
}

void Renderer::SetProjection(float* pMatrix, int constantIndex)
{
	D3DXMATRIX mtx;
	if (pMatrix[6] == 0)
	{
		mtx.m[0][0] = pMatrix[0];
		mtx.m[1][0] = 0.0f;
		mtx.m[2][0] = pMatrix[1];
		mtx.m[3][0] = -0.5f/m_width;	
			 		
		mtx.m[0][1] = 0.0f;
		mtx.m[1][1] = pMatrix[2];
		mtx.m[2][1] = pMatrix[3];
		mtx.m[3][1] = +0.5f/m_height;
			 		
		mtx.m[0][2] = 0.0f;
		mtx.m[1][2] = 0.0f;
		mtx.m[2][2] = -(1-pMatrix[4]);
		mtx.m[3][2] = pMatrix[5]; 
			 		
		mtx.m[0][3] = 0.0f;
		mtx.m[1][3] = 0.0f;
		mtx.m[2][3] = -1.0f;
		mtx.m[3][3] = 0.0f;
	}
	else
	{
		mtx.m[0][0] = pMatrix[0];
		mtx.m[1][0] = 0.0f;
		mtx.m[2][0] = 0.0f;
		mtx.m[3][0] = pMatrix[1]-0.5f/m_width;  // fix d3d pixel center

		mtx.m[0][1] = 0.0f;
		mtx.m[1][1] = pMatrix[2];
		mtx.m[2][1] = 0.0f;
		mtx.m[3][1] = pMatrix[3]+0.5f/m_height; // fix d3d pixel center

		mtx.m[0][2] = 0.0f;
		mtx.m[1][2] = 0.0f;
		mtx.m[2][2] = pMatrix[4];
		mtx.m[3][2] = -(-1 - pMatrix[5]);
			 		
		mtx.m[0][3] = 0;
		mtx.m[1][3] = 0;
		mtx.m[2][3] = 0.0f;
		mtx.m[3][3] = 1.0f;
	}
	D3D::dev->SetVertexShaderConstantF(constantIndex, mtx, 4);
}


void Renderer::SetTexture( DWORD Stage, LPDIRECT3DBASETEXTURE9 pTexture )
{
	if ( m_Textures[Stage] != pTexture )
	{
		m_Textures[Stage] = pTexture;
		D3D::dev->SetTexture( Stage, pTexture );
	}
}


void Renderer::SetFVF( DWORD FVF )
{
	if ( m_FVF != FVF )
	{
		m_FVF = FVF;
		D3D::dev->SetFVF( FVF );
	}
}


void Renderer::SetRenderState( D3DRENDERSTATETYPE State, DWORD Value )
{
	if ( m_RenderStates[State] != Value )
	{
		m_RenderStates[State] = Value;
		D3D::dev->SetRenderState( State, Value );
	}
}


void Renderer::SetTextureStageState( DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value )
{
	if ( m_TextureStageStates[Stage][Type] != Value )
	{
		m_TextureStageStates[Stage][Type] = Value;
		D3D::dev->SetTextureStageState( Stage, Type, Value );
	}
}


void Renderer::SetSamplerState( DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value )
{
	if ( m_SamplerStates[Sampler][Type] != Value )
	{
		m_SamplerStates[Sampler][Type] = Value;
		D3D::dev->SetSamplerState( Sampler, Type, Value );
	}
}


void Renderer::DrawPrimitiveUP( D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride )
{
	D3D::dev->DrawPrimitiveUP( PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride );
}


void Renderer::DrawPrimitive( D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount )
{
	D3D::dev->DrawPrimitive( PrimitiveType, StartVertex, PrimitiveCount );
}
