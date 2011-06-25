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

#include "FramebufferManagerBase.h"

#include "D3DTexture.h"

namespace DX11 {

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
	XFBSource(D3DTexture2D *_tex) : tex(_tex) {}
	~XFBSource() { tex->Release(); }

	void Draw(const MathUtil::Rectangle<float> &sourcerc,
		const MathUtil::Rectangle<float> &drawrc, int width, int height) const;
	void DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight);
	void CopyEFB(float Gamma);

	D3DTexture2D* const tex;
};

class FramebufferManager : public FramebufferManagerBase
{
public:
	FramebufferManager();
	~FramebufferManager();

	static D3DTexture2D* GetEFBColorTexture();
	static D3DTexture2D* GetResolvedEFBColorTexture();
	static D3DTexture2D* GetRealEFBColorTexture();

	static D3DTexture2D* GetEFBDepthTexture();
	static D3DTexture2D* GetResolvedEFBDepthTexture();
	static D3DTexture2D* GetRealEFBDepthTexture();

	static D3DTexture2D* GetEFBColorTempTexture();
	static void SwapReinterpretTexture();

	void SetColorDirty() { ++m_colorTime; }
	void SetDepthDirty() { ++m_depthTime; }

	// Read color at EFB pixel x,y
	u32 ReadColorAt(unsigned int x, unsigned int y);
	// Read depth at EFB pixel x,y
	float ReadDepthAt(unsigned int x, unsigned int y);

private:
	XFBSourceBase* CreateXFBSource(unsigned int target_width, unsigned int target_height);
	void GetTargetSize(unsigned int *width, unsigned int *height, const EFBRectangle& sourceRc);

	void CopyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc,float Gamma);

	D3DTexture2D* m_colorTex;
	long m_colorTime;
	D3DTexture2D* m_resolvedColorTex;
	long m_resolvedColorTime;
	D3DTexture2D* m_realColorTex;
	long m_realColorTime;
	ID3D11Texture2D* m_colorAccessStage;
	long m_colorAccessTime;

	D3DTexture2D* m_depthTex;
	long m_depthTime;
	D3DTexture2D* m_resolvedDepthTex;
	long m_resolvedDepthTime;
	D3DTexture2D* m_realDepthTex;
	long m_realDepthTime;
	ID3D11Texture2D* m_depthAccessStage;
	long m_depthAccessTime;

	D3DTexture2D* m_colorTempTex;
	
	ID3D11PixelShader* m_realColorShader;
	ID3D11PixelShader* m_realDepthShader;
};

}  // namespace DX11

#endif
