
#ifndef _TEXTURECACHEBASE_H
#define _TEXTURECACHEBASE_H

#include <map>

#include "VideoCommon.h"
#include "TextureDecoder.h"
#include "BPMemory.h"
#include "Thread.h"

#include "CommonTypes.h"

class TextureCache
{
public:
	struct TCacheEntryBase
	{
		// TODO: organize
		u32 addr;
		u32 size_in_bytes;
		u64 hash;
		//u32 paletteHash;
		u32 oldpixel;
		u32 format;
		
		int frameCount;

		unsigned int realW, realH; // Texture dimensions from the GameCube's point of view
		unsigned int virtualW, virtualH; // Texture dimensions from OUR point of view
		// Real and virtual dimensions are usually the same, but may be
		// different if e.g. we use high-res textures. Then, realW,realH will
		// be the dimensions of the original GameCube texture and
		// virtualW,virtualH will be the dimensions of the high-res texture.

		unsigned int mipLevels;

		bool isRenderTarget; // copied from EFB
		bool isDynamic; // Used for hybrid EFB copies to enable checks for CPU modifications
		bool isNonPow2;	// doesn't seem to be used anywhere

		//TCacheEntryBase()
		//{
		//	// TODO: remove these
		//	isRenderTarget = 0;
		//	hash = 0;
		//	//paletteHash = 0;
		//	oldpixel = 0;
		//	addr = 0;
		//	size_in_bytes = 0;
		//	frameCount = 0;
		//	isNonPow2 = true;
		//	w = 0;
		//	h = 0;
		//	scaledW = 0;
		//	scaledH = 0;
		//}

		virtual ~TCacheEntryBase();

		virtual void Bind(unsigned int stage) = 0;
		virtual bool Save(const char filename[]) = 0;

		virtual void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int level, bool autogen_mips = false) = 0;
		virtual void FromRenderTarget(bool bFromZBuffer, bool bScaleByHalf,
			unsigned int cbufid, const float *colmat, const EFBRectangle &source_rect,
			bool bIsIntensityFmt, u32 copyfmt) = 0;

		int IntersectsMemoryRange(u32 range_address, u32 range_size) const;
	};

	virtual ~TextureCache(); // needs virtual for DX11 dtor

	static void Init();
	static void Shutdown();
	static void Cleanup();

	static void Invalidate(bool shutdown);
	static void InvalidateRange(u32 start_address, u32 size);
	static void MakeRangeDynamic(u32 start_address, u32 size);
	static void ClearRenderTargets();	// currently only used by OGL
	static bool Find(u32 start_address, u64 hash);

	virtual TCacheEntryBase* CreateTexture(unsigned int width, unsigned int height,
		unsigned int expanded_width, unsigned int tex_levels, PC_TexFormat pcfmt) = 0;
	virtual TCacheEntryBase* CreateRenderTargetTexture(unsigned int scaled_tex_w, unsigned int scaled_tex_h) = 0;

	static TCacheEntryBase* Load(unsigned int stage, u32 address, unsigned int width, unsigned int height,
		int format, unsigned int tlutaddr, int tlutfmt, bool UseNativeMips, unsigned int maxlevel);
	static void CopyRenderTargetToTexture(u32 address, bool bFromZBuffer, bool bIsIntensityFmt,
		u32 copyfmt, bool bScaleByHalf, const EFBRectangle &source_rect);

	static Common::CriticalSection texMutex;

protected:
	TextureCache();

	static u8 *temp;

private:
	typedef std::map<u32, TCacheEntryBase*> TexCache;

	static TexCache textures;
};

extern TextureCache *g_texture_cache;

#endif
