// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <d3d12.h>
#include <memory>

#include "VideoBackends/D3D12/D3DStreamBuffer.h"
#include "VideoBackends/D3D12/D3DTexture.h"
#include "VideoCommon/VideoCommon.h"

namespace DX12
{

class D3DTexture2D;

class XFBEncoder
{
public:
	XFBEncoder();
	~XFBEncoder();

	void EncodeTextureToRam(u8* dst, u32 dst_pitch, u32 dst_height,
	                        D3DTexture2D* src_texture, const TargetRectangle& src_rect,
	                        u32 src_width, u32 src_height, float gamma);

	void DecodeToTexture(D3DTexture2D* dst_texture, const u8* src, u32 src_width, u32 src_height);

private:
	D3DTexture2D* m_yuyv_texture;

	ID3D12Resource* m_readback_buffer;

	std::unique_ptr<D3DStreamBuffer> m_upload_buffer;
	std::unique_ptr<D3DStreamBuffer> m_encode_params_buffer;
};

extern std::unique_ptr<XFBEncoder> g_xfb_encoder;

}
