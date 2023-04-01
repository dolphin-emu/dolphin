// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d11.h>
#include <memory>

#include "VideoBackends/D3D/D3DTexture.h"
#include "VideoCommon/FramebufferManagerBase.h"

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
	XFBSource(D3DTexture2D *_tex, int slices) : tex(_tex), m_slices(slices) {}
	~XFBSource() { tex->Release(); }

	void DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight) override;
	void CopyEFB(float Gamma) override;

	D3DTexture2D* const tex;
	const int m_slices;
};

class FramebufferManager : public FramebufferManagerBase
{
public:
	FramebufferManager();
	~FramebufferManager();

	static D3DTexture2D* &GetEFBColorTexture();
	static D3DTexture2D* &GetEFBColorReadTexture();
	static ID3D11Texture2D* &GetEFBColorStagingBuffer();

	static D3DTexture2D* &GetEFBDepthTexture();
	static D3DTexture2D* &GetEFBDepthReadTexture();
	static ID3D11Texture2D* &GetEFBDepthStagingBuffer();

	static D3DTexture2D* &GetResolvedEFBColorTexture();
	static D3DTexture2D* &GetResolvedEFBDepthTexture();

	static D3DTexture2D* &GetEFBColorTempTexture() { return m_efb.color_temp_tex; }
	static void SwapReinterpretTexture()
	{
		D3DTexture2D* swaptex = GetEFBColorTempTexture();
		m_efb.color_temp_tex = GetEFBColorTexture();
		m_efb.color_tex = swaptex;
	}

private:
	std::unique_ptr<XFBSourceBase> CreateXFBSource(unsigned int target_width, unsigned int target_height, unsigned int layers) override;
	void GetTargetSize(unsigned int *width, unsigned int *height) override;

	void CopyToRealXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc,float Gamma) override;

	static struct Efb
	{
		D3DTexture2D* color_tex;
		ID3D11Texture2D* color_staging_buf;
		D3DTexture2D* color_read_texture;

		D3DTexture2D* depth_tex;
		ID3D11Texture2D* depth_staging_buf;
		D3DTexture2D* depth_read_texture;

		D3DTexture2D* color_temp_tex;

		D3DTexture2D* resolved_color_tex;
		D3DTexture2D* resolved_depth_tex;

		int slices;
	} m_efb;

	static unsigned int m_target_width;
	static unsigned int m_target_height;
};

}  // namespace DX11
