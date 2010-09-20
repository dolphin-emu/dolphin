
#ifndef _TEXTURECACHE_H_
#define _TEXTURECACHE_H_

#include <map>

#include "CommonTypes.h"
#include "VideoCommon.h"
#include "TextureDecoder.h"

unsigned int GetPow2(unsigned int val);

class TextureCacheBase
{
public:
	struct TCacheEntryBase
	{
		u32 addr;
		u32 size_in_bytes;
		u64 hash;
		u32 paletteHash;
		u32 oldpixel;
		
		int frameCount;
		unsigned int w, h, fmt, MipLevels;
		int Scaledw, Scaledh;

		bool isRenderTarget;
		bool isNonPow2;

		TCacheEntryBase() : addr(0), size_in_bytes(0), hash(0), paletteHash(0), oldpixel(0),
						frameCount(0), w(0), h(0), fmt(0), MipLevels(0), Scaledw(0), Scaledh(0),
						isRenderTarget(false), isNonPow2(true) {}

		virtual ~TCacheEntryBase();

		virtual void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int levels) = 0;

		virtual void FromRenderTarget(bool bFromZBuffer, bool bScaleByHalf,
			unsigned int cbufid, const float colmat[], const EFBRectangle &source_rect) = 0;

		virtual void Bind(unsigned int stage) = 0;

		bool IntersectsMemoryRange(u32 range_address, u32 range_size) const;
	};

	friend struct TCacheEntryBase;

	TextureCacheBase();
	virtual ~TextureCacheBase();

	static void Cleanup();

	static void Invalidate(bool shutdown);
	static void InvalidateRange(u32 start_address, u32 size);

	// TODO: make these 2 static?
	TCacheEntryBase* Load(unsigned int stage, u32 address, unsigned int width,
		unsigned int height, unsigned int tex_format, unsigned int tlutaddr,
		unsigned int tlutfmt, bool UseNativeMips, unsigned int maxlevel);

	void CopyRenderTargetToTexture(u32 address, bool bFromZBuffer, bool bIsIntensityFmt,
		u32 copyfmt, bool bScaleByHalf, const EFBRectangle &source_rect);

	virtual TCacheEntryBase* CreateTexture(unsigned int width, unsigned int height,
		unsigned int expanded_width, unsigned int tex_levels, PC_TexFormat pcfmt) = 0;

	virtual TCacheEntryBase* CreateRenderTargetTexture(int scaled_tex_w, int scaled_tex_h) = 0;

	static void ClearRenderTargets();
	static void MakeRangeDynamic(u32 start_address, u32 size);

protected:
	typedef std::map<u32, TCacheEntryBase*> TexCache;
	static TexCache textures;
	static u8 *temp;
};

#endif
