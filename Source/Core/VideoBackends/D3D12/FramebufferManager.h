// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoCommon/FramebufferManagerBase.h"

namespace DX12
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

struct XFBSource final : public XFBSourceBase
{
	XFBSource(D3DTexture2D* tex, int slices) : m_tex(tex), m_slices(slices) {}
	~XFBSource() { m_tex->Release(); }

	void DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight) override;
	void CopyEFB(float gamma) override;

	D3DTexture2D* m_tex;
	const int m_slices;
};

class FramebufferManager final : public FramebufferManagerBase
{
public:
	FramebufferManager();
	~FramebufferManager();

	static D3DTexture2D*& GetEFBColorTexture();
	static D3DTexture2D*& GetEFBDepthTexture();
	static D3DTexture2D*& GetResolvedEFBColorTexture();
	static D3DTexture2D*& GetResolvedEFBDepthTexture();

	static D3DTexture2D*& GetEFBColorTempTexture();
	static void SwapReinterpretTexture();

	static void ResolveDepthTexture();

	static void RestoreEFBRenderTargets();

	// Access EFB from CPU
	static u32 ReadEFBColorAccessCopy(u32 x, u32 y);
	static float ReadEFBDepthAccessCopy(u32 x, u32 y);
	static void UpdateEFBColorAccessCopy(u32 x, u32 y, u32 color);
	static void UpdateEFBDepthAccessCopy(u32 x, u32 y, float depth);
	static void InitializeEFBAccessCopies();
	static void MapEFBColorAccessCopy();
	static void MapEFBDepthAccessCopy();
	static void InvalidateEFBAccessCopies();
	static void DestroyEFBAccessCopies();

private:
	std::unique_ptr<XFBSourceBase> CreateXFBSource(unsigned int target_width, unsigned int target_height, unsigned int layers) override;
	void GetTargetSize(unsigned int* width, unsigned int* height) override;

	void CopyToRealXFB(u32 xfbAddr, u32 fbStride, u32 fbHeight, const EFBRectangle& sourceRc, float gamma) override;

	static struct Efb
	{
		D3DTexture2D* color_tex;

		D3DTexture2D* depth_tex;

		D3DTexture2D* color_temp_tex;

		D3DTexture2D* resolved_color_tex;
		D3DTexture2D* resolved_depth_tex;

		D3DTexture2D* color_access_resize_tex;
		ID3D12Resource* color_access_readback_buffer;
		u8* color_access_readback_map;
		u32 color_access_readback_pitch;

		D3DTexture2D* depth_access_resize_tex;
		ID3D12Resource* depth_access_readback_buffer;
		u8* depth_access_readback_map;
		u32 depth_access_readback_pitch;

		int slices;
	} m_efb;

	static unsigned int m_target_width;
	static unsigned int m_target_height;
};

}  // namespace DX12
