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

#ifndef _FBMANAGER_D3D_H_
#define _FBMANAGER_D3D_H_

#include <list>
#include "D3DBase.h"

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
const int MAX_VIRTUAL_XFB = 8;

inline bool addrRangesOverlap(u32 aLower, u32 aUpper, u32 bLower, u32 bUpper)
{
	return !((aLower >= bUpper) || (bLower >= aUpper));
}

struct XFBSource
{
	XFBSource()
	{
		this->srcAddr = 0;
		this->srcWidth = 0;
		this->srcHeight = 0;
		this->tex = NULL;
		this->texWidth = 0;
		this->texHeight = 0;
	}

	u32 srcAddr;
	u32 srcWidth;
	u32 srcHeight;

	D3DTexture2D* tex;
	unsigned int texWidth;
	unsigned int texHeight;
};

class FramebufferManager
{
public:
	FramebufferManager()
	{
		m_efb.color_tex = NULL;
		m_efb.color_staging_buf = NULL;
		m_efb.depth_tex = NULL;
		m_efb.depth_staging_buf = NULL;
		m_efb.depth_read_texture = NULL;

		m_realXFBSource.tex = NULL;
	}

	void Create();
	void Destroy();

	void CopyToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);
	const XFBSource** GetXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount);

	D3DTexture2D* &GetEFBColorTexture();
	ID3D11Texture2D* &GetEFBColorStagingBuffer();

	D3DTexture2D* &GetEFBDepthTexture();
	D3DTexture2D* &GetEFBDepthReadTexture();
	ID3D11Texture2D* &GetEFBDepthStagingBuffer();

private:

	struct VirtualXFB
	{
		// Address and size in GameCube RAM
		u32 xfbAddr;
		u32 xfbWidth;
		u32 xfbHeight;

		XFBSource xfbSource;
	};

	typedef std::list<VirtualXFB> VirtualXFBListType;

	VirtualXFBListType::iterator findVirtualXFB(u32 xfbAddr, u32 width, u32 height);

	void replaceVirtualXFB();

	void copyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);
	void copyToVirtualXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);
	const XFBSource** getRealXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount);
	const XFBSource** getVirtualXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount);

	XFBSource m_realXFBSource; // Only used in Real XFB mode
	VirtualXFBListType m_virtualXFBList; // Only used in Virtual XFB mode

	const XFBSource* m_overlappingXFBArray[MAX_VIRTUAL_XFB];

	struct
	{
		D3DTexture2D* color_tex;
		ID3D11Texture2D* color_staging_buf;

		D3DTexture2D* depth_tex;
		ID3D11Texture2D* depth_staging_buf;
		D3DTexture2D* depth_read_texture;
	} m_efb;
};

extern FramebufferManager g_framebufferManager;

#endif
