#include <d3dx9.h>

#include "Common.h"

#include "D3DBase.h"

#include "Globals.h"
#include "main.h"
#include "VertexHandler.h"
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

float Renderer::m_x,Renderer::m_y,Renderer::m_width, Renderer::m_height, Renderer::xScale,Renderer::yScale;

#define NUMWNDRES 6
extern int g_Res[NUMWNDRES][2];

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
	D3D::dev->SetRenderState(D3DRS_LIGHTING,FALSE);
	D3D::dev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	D3D::dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	D3D::dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
	D3D::dev->SetRenderState(D3DRS_FILLMODE, g_Config.bWireFrame?D3DFILL_WIREFRAME:D3DFILL_SOLID);
	for (int i=0; i<8; i++)
	{
		D3D::dev->SetSamplerState(i, D3DSAMP_MAXANISOTROPY, 16);
	}
	ReinitView();

	Postprocess::Initialize();
	Postprocess::BeginFrame();
	D3D::BeginFrame(true, 0);
	CVertexHandler::BeginFrame();
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
		p+=sprintf(p,"Num strip joins: %i\n",stats.numJoins);
		p+=sprintf(p,"Num primitives: %i\n",stats.thisFrame.numPrims);
		p+=sprintf(p,"Num primitives (DL): %i\n",stats.thisFrame.numDLPrims);
		p+=sprintf(p,"Num bad commands: %i%s\n",stats.thisFrame.numBadCommands,stats.thisFrame.numBadCommands?"!!!":"");
		p+=sprintf(p,"Num XF loads: %i\n",stats.thisFrame.numXFLoads);
		p+=sprintf(p,"Num XF loads (DL): %i\n",stats.thisFrame.numXFLoadsInDL);
		p+=sprintf(p,"Num CP loads: %i\n",stats.thisFrame.numCPLoads);
		p+=sprintf(p,"Num CP loads (DL): %i\n",stats.thisFrame.numCPLoadsInDL);
		p+=sprintf(p,"Num BP loads: %i\n",stats.thisFrame.numBPLoads);
		p+=sprintf(p,"Num BP loads (DL): %i\n",stats.thisFrame.numBPLoadsInDL);

		D3D::font.DrawTextScaled(0,30,20,20,0.0f,0xFF00FFFF,st,false);

		//end frame
	}

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
	CVertexHandler::BeginFrame();

	if (g_Config.bOldCard)
		D3D::font.SetRenderStates(); //compatibility with lowend cards
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