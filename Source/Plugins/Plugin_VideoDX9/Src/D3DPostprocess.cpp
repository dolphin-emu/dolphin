// Copyright (C) 2003 Dolphin Project.

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

#include "Common.h"

#include "D3DBase.h"
#include "D3DTexture.h"
#include "D3DUtil.h"

#include "Config.h"

#include "Render.h"

using namespace D3D;

namespace Postprocess
{
	LPDIRECT3DSURFACE9 displayColorBuffer;
	LPDIRECT3DSURFACE9 displayZStencilBuffer;

	LPDIRECT3DTEXTURE9 mainColorBufferTexture;
	LPDIRECT3DSURFACE9 mainColorBuffer;
	LPDIRECT3DSURFACE9 mainZStencilBuffer;

	const int numScratch = 2;
	LPDIRECT3DTEXTURE9 scratch[numScratch];
	LPDIRECT3DSURFACE9 scratchSurface[numScratch];

	const int mainWidth = 640, mainHeight=480;
	const int scratchWidth = 256, scratchHeight=256;

	int displayWidth, displayHeight;

	bool initialized;

	int GetWidth() {
		return initialized ? mainWidth : displayWidth;
	}

	int GetHeight() {
		return initialized ? mainHeight : displayHeight;
	}


	void CreateStuff()
	{
		mainColorBufferTexture = D3D::CreateRenderTarget(mainWidth,mainHeight);
		mainColorBufferTexture->GetSurfaceLevel(0,&mainColorBuffer);
		mainZStencilBuffer = D3D::CreateDepthStencilSurface(mainWidth,mainHeight);

		for (int i=0; i<numScratch; i++)
		{
			scratch[i]=D3D::CreateRenderTarget(scratchWidth,scratchHeight);
			scratch[i]->GetSurfaceLevel(0,&(scratchSurface[i]));
		}

		initialized=true;
	}

	void DestroyStuff()
	{
		SAFE_RELEASE(mainColorBuffer);
		SAFE_RELEASE(mainColorBufferTexture);
		SAFE_RELEASE(mainZStencilBuffer);

		for (int i=0; i<numScratch; i++)
		{
			SAFE_RELEASE(scratch[i]);
			SAFE_RELEASE(scratchSurface[i]);
		}
		initialized=false;
	}


	void Initialize()
	{
		dev->GetRenderTarget(0,&displayColorBuffer);
		dev->GetDepthStencilSurface(&displayZStencilBuffer);

		D3DSURFACE_DESC desc;
		displayColorBuffer->GetDesc(&desc);
		displayWidth = desc.Width;
		displayHeight = desc.Height;

		if (g_Config.iPostprocessEffect)
			CreateStuff();
	}

	void Cleanup()
	{
		DestroyStuff();
		SAFE_RELEASE(displayColorBuffer);
		SAFE_RELEASE(displayZStencilBuffer);
	}


	void BeginFrame()
	{
		if (g_Config.iPostprocessEffect)
		{
			if (!initialized)
				CreateStuff();
			dev->SetRenderTarget(0,mainColorBuffer);
			dev->SetDepthStencilSurface(mainZStencilBuffer);
			
			// dev->SetRenderState(D3DRS_ZENABLE,TRUE);
			Renderer::SetRenderState( D3DRS_ZENABLE, TRUE );
			
			dev->Clear(0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0,1,0);
		}
		else
		{
			if (initialized)
			{
				dev->SetRenderTarget(0,displayColorBuffer);
				dev->SetDepthStencilSurface(displayZStencilBuffer);
				DestroyStuff();
			}
			
			// dev->SetRenderState(D3DRS_ZENABLE,TRUE);
			Renderer::SetRenderState( D3DRS_ZENABLE, TRUE );
		}
	}


	int filterKernel[8] = {0x40,0x80,0xc0,0xFF,0xFF,0xc0,0x80,0x40}; //good looking almost Gaussian
	
	//int filterKernel[8] = {0xFF,0xc0,0x80,0x40,0x40,0x80,0xc0,0xFF,}; //crazy filter

	void NightGlow(bool intense, bool original)
	{
		// dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SUBTRACT);
		// dev->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
		// dev->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);
		// dev->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
		Renderer::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SUBTRACT );
		Renderer::SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
		Renderer::SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
		Renderer::SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

		dev->SetDepthStencilSurface(0);

		//dev->SetTexture(0,mainColorBufferTexture);
		Renderer::SetTexture( 0, mainColorBufferTexture );

		dev->SetRenderTarget(0,scratchSurface[0]);
		dev->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
		dev->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
		dev->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
		dev->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
		dev->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);
		dev->Clear(0,0,D3DCLEAR_TARGET,0,0,0);

		POINT pt;
		GetCursorPos(&pt);

		//dev->SetSamplerState(0,D3DSAMP_MIPMAPLODBIAS,1.0f);
#define QOFF(xoff,yoff,col) quad2d(-0.0f,-0.0f,scratchWidth-0.0f,scratchHeight-0.0f,col,0+xoff,0+yoff,1+xoff,1+yoff);
		float f=0.008f;
		QOFF(0,0,0xa0a0a0a0);

		// dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);
		//dev->SetTexture(0,scratch[0]);
		Renderer::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
		Renderer::SetTexture( 0, scratch[0] );

		dev->SetRenderTarget(0,scratchSurface[1]);
		dev->Clear(0,0,D3DCLEAR_TARGET,0,0,0);

		float yMul = 1.33333333333f;
		for (int i=0; i<8; i++)
		{
			DWORD c=filterKernel[i]/2;
			c|=c<<8;
			c|=c<<16;
			QOFF(0,(i-3.5f) * f * yMul,c);
		}

		//dev->SetTexture(0,scratch[1]);
		Renderer::SetTexture( 0, scratch[1] );

		dev->SetRenderTarget(0,scratchSurface[0]);
		dev->Clear(0,0,D3DCLEAR_TARGET,0,0,0);
		for (int i=0; i<8; i++)
		{
			DWORD c=filterKernel[i]/(intense?3:2);
			c|=c<<8;
			c|=c<<16;
			QOFF((i-3.5f) * f,0,c);
		}

		// dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);
		Renderer::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );

		if (intense)
		{
			// dev->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);
			// dev->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_SRCALPHA);
			Renderer::SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
			Renderer::SetRenderState( D3DRS_DESTBLEND, D3DBLEND_SRCALPHA );
		}
		else
		{
			// dev->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_INVDESTCOLOR);
			// dev->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
			Renderer::SetRenderState( D3DRS_SRCBLEND, D3DBLEND_INVDESTCOLOR );
			Renderer::SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
		}

		// dev->SetTexture(0,scratch[0]);
		Renderer::SetTexture( 0, scratch[0] );

		dev->SetRenderTarget(0,mainColorBuffer);
		quad2d(0,0,(float)mainWidth,(float)mainHeight,original?0xCFFFFFFF:0xFFFFFFFF,0,0,1,1);
		
		// dev->SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
		Renderer::SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

		dev->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP);
		dev->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
		//dev->SetSamplerState(0,D3DSAMP_MIPMAPLODBIAS,0);
	}

	const char **GetPostprocessingNames()
	{
		static const char *names[] = {
			"None",
			"Night Glow 1",
			"Night Glow 2",
			"Night Glow 3",
			0,
		};
		return names;
	}

	void FinalizeFrame()
	{	
		if (initialized)
		{
			// dev->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
			// dev->SetRenderState(D3DRS_ZENABLE,FALSE);
			// dev->SetRenderState(D3DRS_FOGENABLE,FALSE);
			// dev->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
			// dev->SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
			// dev->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);
			// dev->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
			// dev->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
			// dev->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);

			Renderer::SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
			Renderer::SetRenderState( D3DRS_ZENABLE, FALSE );
			Renderer::SetRenderState( D3DRS_FOGENABLE, FALSE );
			Renderer::SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
			Renderer::SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
			Renderer::SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);
			Renderer::SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
			Renderer::SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
			Renderer::SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);

			switch(g_Config.iPostprocessEffect) {
			case 1:
				NightGlow(true,true);
			case 2:
				NightGlow(false,true);
				break;
			case 3:
				NightGlow(false,false);
				break;
			}

			dev->SetRenderTarget(0,displayColorBuffer);
			dev->SetDepthStencilSurface(displayZStencilBuffer);
			
			// dev->SetTexture(0,mainColorBufferTexture);
			Renderer::SetTexture( 0, mainColorBufferTexture );

			quad2d(0, 0, (float)displayWidth, (float)displayHeight, 0xFFFFFFFF);
		}
	}
}
