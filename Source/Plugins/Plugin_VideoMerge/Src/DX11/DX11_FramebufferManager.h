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

#ifndef _DX11_FBMANAGER_D3D_H_
#define _DX11_FBMANAGER_D3D_H_

#include <list>

#include "DX11_D3DBase.h"

#include "../FramebufferManager.h"

namespace DX11
{

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

struct XFBSource : public XFBSourceBase
{
	XFBSource() : tex(NULL) {}
	~XFBSource();

	void CopyEFB(const TargetRectangle& efbSource);

	D3DTexture2D* tex;
};

class FramebufferManager : public ::FramebufferManagerBase
{
	friend struct XFBSource;

public:
	FramebufferManager();
	~FramebufferManager();

	static D3DTexture2D* &GetEFBColorTexture();
	static ID3D11Texture2D* &GetEFBColorStagingBuffer();

	static D3DTexture2D* &GetEFBDepthTexture();
	static D3DTexture2D* &GetEFBDepthReadTexture();
	static ID3D11Texture2D* &GetEFBDepthStagingBuffer();

	XFBSourceBase* CreateXFBSource(unsigned int target_width, unsigned int target_height);

private:
	static void copyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);
	static const XFBSource** getRealXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount);

	static XFBSource m_realXFBSource;           // used in real XFB mode

	static struct EFB
	{
		D3DTexture2D* color_tex;
		ID3D11Texture2D* color_staging_buf;

		D3DTexture2D* depth_tex;
		ID3D11Texture2D* depth_staging_buf;
		D3DTexture2D* depth_read_texture;
	} m_efb;
};

}

#endif
