// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Hash.h"
#include "VideoCommon/TextureDecoderHelpers.h"

enum
{
	TMEM_SIZE = 1024*1024,
	TMEM_LINE_SIZE = 32,
};
extern  GC_ALIGNED16(u8 texMem[TMEM_SIZE]);

class TextureDecoder
{
	private:
	bool m_TexFmt_Overlay_Enable;
	bool m_TexFmt_Overlay_Center;

	// Private texture decoders
	// Generic CPU based TextureDecoding
	virtual void Decode_C4(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt);
	virtual void Decode_C8(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt);
	virtual void Decode_I4(u32 *dst, const u8 *src, int width, int height);
	virtual void Decode_I8(u32 *dst, const u8 *src, int width, int height);
	virtual void Decode_IA4(u32 *dst, const u8 *src, int width, int height);
	virtual void Decode_IA8(u32 *dst, const u8 *src, int width, int height);
	virtual void Decode_C14X2(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt);
	virtual void Decode_RGB565(u32 *dst, const u8 *src, int width, int height);
	virtual void Decode_RGB5A3(u32 *dst, const u8 *src, int width, int height);
	virtual void Decode_RGBA8(u32 *dst, const u8 *src, int width, int height);
	virtual void Decode_CMPR(u32 *dst, const u8 *src, int width, int height);

	public:
	virtual ~TextureDecoder() {}
	TextureDecoder() : m_TexFmt_Overlay_Enable(false), m_TexFmt_Overlay_Center(false) {}	

	// Texture format overlay option setting
	virtual void SetTextureFormatOverlay(bool enable, bool center) final;
	
	// Calls our internal texture decoders
	// Also puts the texture overlay upon the final texture image
	virtual void Decode(u8 *dst, const u8 *src, int width, int height, int texformat, int tlutaddr, int tlutfmt) final;
};

#ifdef _M_X86
// JSD 01/06/11:
// TODO: we really should ensure BOTH the source and destination addresses are aligned to 16-byte boundaries to
// squeeze out a little more performance. _mm_loadu_si128/_mm_storeu_si128 is slower than _mm_load_si128/_mm_store_si128
// because they work on unaligned addresses. The processor is free to make the assumption that addresses are multiples
// of 16 in the aligned case.
// TODO: complete SSE2 optimization of less often used texture formats.
// TODO: refactor algorithms using _mm_loadl_epi64 unaligned loads to prefer 128-bit aligned loads.

class TextureDecoderX86 : public TextureDecoder
{
	private:
	// Private texture decoders
	// x86 accelerated texture decoding
	void Decode_C4(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt);
	void Decode_C8(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt);
	void Decode_I4(u32 *dst, const u8 *src, int width, int height);
	void Decode_I8(u32 *dst, const u8 *src, int width, int height);
	void Decode_IA4(u32 *dst, const u8 *src, int width, int height);
	void Decode_IA8(u32 *dst, const u8 *src, int width, int height);
	void Decode_C14X2(u32 *dst, const u8 *src, int width, int height, int tlutaddr, int tlutfmt);
	void Decode_RGB565(u32 *dst, const u8 *src, int width, int height);
	void Decode_RGB5A3(u32 *dst, const u8 *src, int width, int height);
	void Decode_RGBA8(u32 *dst, const u8 *src, int width, int height);
	void Decode_CMPR(u32 *dst, const u8 *src, int width, int height);
};
#endif

