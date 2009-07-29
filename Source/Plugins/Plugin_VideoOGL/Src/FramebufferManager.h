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

#ifndef _FRAMEBUFFERMANAGER_H_
#define _FRAMEBUFFERMANAGER_H_

#include <list>
#include "GLUtil.h"

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
const int MAX_VIRTUAL_XFB = 4;

inline bool addrRangesOverlap(u32 aLower, u32 aUpper, u32 bLower, u32 bUpper)
{
	return !((aLower >= bUpper) || (bLower >= aUpper));
}

struct XFBSource
{
	XFBSource() :
		texture(0)
	{}

	GLuint texture;
	int texWidth;
	int texHeight;

	TargetRectangle sourceRc;
};

class FramebufferManager
{

public:

	FramebufferManager() :
		m_efbFramebuffer(0),
		m_efbColor(0),
		m_efbDepth(0),
		m_resolvedFramebuffer(0),
		m_resolvedColorTexture(0),
		m_resolvedDepthTexture(0),
		m_xfbFramebuffer(0)
	{}

	void Init(int targetWidth, int targetHeight, int msaaSamples, int msaaCoverageSamples);
	void Shutdown();

	void CopyToXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);

	const XFBSource* GetXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight);

	// To get the EFB in texture form, these functions may have to transfer
	// the EFB to a resolved texture first.
	GLuint GetEFBColorTexture(const EFBRectangle& sourceRc) const;
	GLuint GetEFBDepthTexture(const EFBRectangle& sourceRc) const;

	GLuint GetEFBFramebuffer() const { return m_efbFramebuffer; }

	// Resolved framebuffer is only used in MSAA mode.
	GLuint GetResolvedFramebuffer() const { return m_resolvedFramebuffer; }

	TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) const;

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

	void copyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);
	void copyToVirtualXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc);
	const XFBSource* getRealXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight);
	const XFBSource* getVirtualXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight);

	int m_targetWidth;
	int m_targetHeight;
	int m_msaaSamples;
	int m_msaaCoverageSamples;

	GLuint m_efbFramebuffer;
	GLuint m_efbColor; // Renderbuffer in MSAA mode; Texture otherwise
	GLuint m_efbDepth; // Renderbuffer in MSAA mode; Texture otherwise

	// Only used in MSAA mode.
	GLuint m_resolvedFramebuffer;
	GLuint m_resolvedColorTexture;
	GLuint m_resolvedDepthTexture;

	GLuint m_xfbFramebuffer; // Only used in MSAA mode
	XFBSource m_realXFBSource; // Only used in Real XFB mode
	VirtualXFBListType m_virtualXFBList; // Only used in Virtual XFB mode

};

#endif
