// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "D3DBase.h"
#include "FramebufferManagerBase.h"

// On the GameCube, the game sends a request for the graphics processor to
// transfer its internal EFB (Embedded Framebuffer) to an area in GameCube RAM
// called the XFB (External Framebuffer). The size and location of the XFB is
// decided at the time of the copy, and the format is always YUYV. The video
// interface is given a pointer to the XFB, which will be decoded and
// displayed on the TV.
//
// There are two ways for Dolphin to emulate this:
//
// Real XFB mode:
//
// Dolphin will behave like the GameCube and encode the EFB to
// a portion of GameCube RAM. The emulated video interface will decode the data
// for output to the screen.
//
// Advantages: Behaves exactly like the GameCube.
// Disadvantages: Resolution will be limited.
//
// Virtual XFB mode:
//
// When a request is made to copy the EFB to an XFB, Dolphin
// will remember the RAM location and size of the XFB in a Virtual XFB list.
// The video interface will look up the XFB in the list and use the enhanced
// data stored there, if available.
//
// Advantages: Enables high resolution graphics, better than real hardware.
// Disadvantages: If the GameCube CPU writes directly to the XFB (which is
// possible but uncommon), the Virtual XFB will not capture this information.

namespace DX9
{

struct XFBSource : public XFBSourceBase
{
	XFBSource(LPDIRECT3DTEXTURE9 tex) : texture(tex) {}
	~XFBSource() { texture->Release(); }

	void Draw(const MathUtil::Rectangle<float> &sourcerc,
		const MathUtil::Rectangle<float> &drawrc, int width, int height) const;
	void DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight);
	void CopyEFB(float Gamma);

	LPDIRECT3DTEXTURE9 const texture;
};

class FramebufferManager : public FramebufferManagerBase
{
public:
	FramebufferManager();
	~FramebufferManager();

	static LPDIRECT3DTEXTURE9 GetEFBColorTexture() { return s_efb.color_texture; }
	static LPDIRECT3DTEXTURE9 GetEFBDepthTexture() { return s_efb.depth_texture; }

	static LPDIRECT3DSURFACE9 GetEFBColorRTSurface() { return s_efb.color_surface; }
	static LPDIRECT3DSURFACE9 GetEFBDepthRTSurface() { return s_efb.depth_surface; }

	static LPDIRECT3DSURFACE9 GetEFBColorOffScreenRTSurface() { return s_efb.color_OffScreenReadBuffer; }
	static LPDIRECT3DSURFACE9 GetEFBDepthOffScreenRTSurface() { return s_efb.depth_OffScreenReadBuffer; }

	static D3DFORMAT GetEFBDepthRTSurfaceFormat() { return s_efb.depth_surface_Format; }
	static D3DFORMAT GetEFBColorRTSurfaceFormat() { return s_efb.color_surface_Format; }
	static D3DFORMAT GetEFBDepthReadSurfaceFormat() { return s_efb.depth_ReadBuffer_Format; }

	static LPDIRECT3DSURFACE9 GetEFBColorReadSurface() { return s_efb.color_ReadBuffer; }
	static LPDIRECT3DSURFACE9 GetEFBDepthReadSurface() { return s_efb.depth_ReadBuffer; }

	static LPDIRECT3DTEXTURE9 GetEFBColorReinterpretTexture() { return s_efb.color_reinterpret_texture; }
	static LPDIRECT3DSURFACE9 GetEFBColorReinterpretSurface() { return s_efb.color_reinterpret_surface; }
	static void SwapReinterpretTexture()
	{
		LPDIRECT3DSURFACE9 swapsurf = GetEFBColorReinterpretSurface();
		LPDIRECT3DTEXTURE9 swaptex = GetEFBColorReinterpretTexture();
		s_efb.color_reinterpret_surface = GetEFBColorRTSurface();
		s_efb.color_reinterpret_texture = GetEFBColorTexture();
		s_efb.color_surface = swapsurf;
		s_efb.color_texture = swaptex;
	}

private:
	XFBSourceBase* CreateXFBSource(unsigned int target_width, unsigned int target_height);
	void GetTargetSize(unsigned int *width, unsigned int *height, const EFBRectangle& sourceRc);

	void CopyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc,float Gamma);

	static struct Efb
	{
		Efb() : color_texture(NULL), colorRead_texture(NULL), depth_texture(NULL), depthRead_texture(NULL),
				color_reinterpret_texture(NULL), color_reinterpret_surface(NULL),
				depth_surface(NULL), color_surface(NULL), color_ReadBuffer(NULL), depth_ReadBuffer(NULL),
				color_OffScreenReadBuffer(NULL), depth_OffScreenReadBuffer(NULL),
				color_surface_Format(D3DFMT_UNKNOWN), depth_surface_Format(D3DFMT_UNKNOWN),
				depth_ReadBuffer_Format(D3DFMT_UNKNOWN) {}

		LPDIRECT3DTEXTURE9 color_texture;//Texture that contains the color data of the render target
		LPDIRECT3DTEXTURE9 colorRead_texture;//1 pixel texture for temporal data store
		LPDIRECT3DTEXTURE9 depth_texture;//Texture that contains the depth data of the render target
		LPDIRECT3DTEXTURE9 depthRead_texture;//4 pixel texture for temporal data store

		LPDIRECT3DTEXTURE9 color_reinterpret_texture;//buffer used for ReinterpretPixelData
		LPDIRECT3DSURFACE9 color_reinterpret_surface;//corresponding surface

		LPDIRECT3DSURFACE9 depth_surface;//Depth Surface
		LPDIRECT3DSURFACE9 color_surface;//Color Surface
		LPDIRECT3DSURFACE9 color_ReadBuffer;//Surface 0 of colorRead_texture
		LPDIRECT3DSURFACE9 depth_ReadBuffer;//Surface 0 of depthRead_texture
		LPDIRECT3DSURFACE9 color_OffScreenReadBuffer;//System memory Surface that can be locked to retrieve the data
		LPDIRECT3DSURFACE9 depth_OffScreenReadBuffer;//System memory Surface that can be locked to retrieve the data

		D3DFORMAT color_surface_Format;//Format of the color Surface
		D3DFORMAT depth_surface_Format;//Format of the Depth Surface
		D3DFORMAT depth_ReadBuffer_Format;//Format of the Depth color Read Surface
	} s_efb;
};

}  // namespace DX9