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

#ifndef _FRAMEBUFFERMANAGER_D3D_H_
#define _FRAMEBUFFERMANAGER_D3D_H_

#include <list>
#include "DX9_D3DBase.h"

#include "../FramebufferManager.h"

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

// There may be multiple XFBs in GameCube RAM. This is the maximum number to
// virtualize.

namespace DX9
{

const int MAX_VIRTUAL_XFB = 8;

inline bool addrRangesOverlap(u32 aLower, u32 aUpper, u32 bLower, u32 bUpper)
{
	return !((aLower >= bUpper) || (bLower >= aUpper));
}

struct XFBSource : public XFBSourceBase
{
	XFBSource() : texture(NULL) {}
	~XFBSource();

	void CopyEFB(const TargetRectangle& efbSource);

	LPDIRECT3DTEXTURE9 texture;
};

class FramebufferManager : public ::FramebufferManagerBase
{
public:
	FramebufferManager();
	~FramebufferManager();

	void CopyToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);

	XFBSourceBase *CreateXFBSource(unsigned int target_width, unsigned int target_height);

	static LPDIRECT3DTEXTURE9 GetEFBColorTexture(const EFBRectangle& sourceRc);
	static LPDIRECT3DTEXTURE9 GetEFBDepthTexture(const EFBRectangle& sourceRc);

	static LPDIRECT3DSURFACE9 GetEFBColorRTSurface();
	static LPDIRECT3DSURFACE9 GetEFBDepthRTSurface();
	static LPDIRECT3DSURFACE9 GetEFBColorOffScreenRTSurface();
	static LPDIRECT3DSURFACE9 GetEFBDepthOffScreenRTSurface();
	static D3DFORMAT GetEFBDepthRTSurfaceFormat();
	static D3DFORMAT GetEFBColorRTSurfaceFormat();
	static D3DFORMAT GetEFBDepthReadSurfaceFormat();
	static LPDIRECT3DSURFACE9 GetEFBColorReadSurface();
	static LPDIRECT3DSURFACE9 GetEFBDepthReadSurface();

private:

	void copyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);
	void copyToVirtualXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);
	const XFBSource** getRealXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount);

	static XFBSource m_realXFBSource; // Only used in Real XFB mode

	const XFBSource* m_overlappingXFBArray[MAX_VIRTUAL_XFB];

	static LPDIRECT3DTEXTURE9 s_efb_color_texture;//Texture thats contains the color data of the render target
	static LPDIRECT3DTEXTURE9 s_efb_colorRead_texture;//1 pixel texture for temporal data store
	static LPDIRECT3DTEXTURE9 s_efb_depth_texture;//Texture thats contains the depth data of the render target
	static LPDIRECT3DTEXTURE9 s_efb_depthRead_texture;//4 pixel texture for temporal data store

	static LPDIRECT3DSURFACE9 s_efb_depth_surface;//Depth Surface
	static LPDIRECT3DSURFACE9 s_efb_color_surface;//Color Surface
	static LPDIRECT3DSURFACE9 s_efb_color_ReadBuffer;//Surface 0 of s_efb_colorRead_texture
	static LPDIRECT3DSURFACE9 s_efb_depth_ReadBuffer;//Surface 0 of s_efb_depthRead_texture
	static LPDIRECT3DSURFACE9 s_efb_color_OffScreenReadBuffer;//System memory Surface that can be locked to retriebe the data
	static LPDIRECT3DSURFACE9 s_efb_depth_OffScreenReadBuffer;//System memory Surface that can be locked to retriebe the data

	static D3DFORMAT s_efb_color_surface_Format;//Format of the color Surface
	static D3DFORMAT s_efb_depth_surface_Format;//Format of the Depth Surface
	static D3DFORMAT s_efb_depth_ReadBuffer_Format;//Format of the Depth color Read Surface
};

}

#endif
