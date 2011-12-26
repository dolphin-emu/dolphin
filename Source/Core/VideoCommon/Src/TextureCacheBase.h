
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
#define TEXHASH_INVALID 0

		// common members
		u32 addr;
		u32 size_in_bytes;
		u64 hash;
		//u32 pal_hash;
		u32 format;

		//bool is_preloaded;

		unsigned int num_mipmaps;
		unsigned int native_width, native_height; // Texture dimensions from the GameCube's point of view
		unsigned int virtual_width, virtual_height; // Texture dimensions from OUR point of view - for hires textures or scaled EFB copies

		void SetGeneralParameters(u32 addr, u32 size, u32 format, unsigned int num_mipmaps)
		{
			this->addr = addr;
			this->size_in_bytes = size;
			this->format = format;
			this->num_mipmaps = num_mipmaps;
		}

		void SetDimensions(unsigned int native_width, unsigned int native_height, unsigned int virtual_width, unsigned int virtual_height)
		{
			this->native_width = native_width;
			this->native_height = native_height;
			this->virtual_width = virtual_width;
			this->virtual_height = virtual_height;
		}

		void SetHashes(u64 hash/*, u32 pal_hash*/)
		{
			this->hash = hash;
			//this->pal_hash = pal_hash;
		}

		void SetEFBCopyParameters(bool is_efb_copy, bool is_dynamic)
		{
			isRenderTarget = is_efb_copy;
			isDynamic = is_dynamic;
		}

		// EFB copies
		bool isRenderTarget; // copied from EFB
		bool isDynamic; // Used for hybrid EFB copies to enable checks for CPU modifications

		// deprecated members
		u32 oldpixel;
		int frameCount;
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
		virtual void FromRenderTarget(u32 dstAddr, unsigned int dstFormat,
			unsigned int srcFormat, const EFBRectangle& srcRect,
			bool isIntensity, bool scaleByHalf, unsigned int cbufid,
			const float *colmat) = 0;

		int IntersectsMemoryRange(u32 range_address, u32 range_size) const;
	};

	virtual ~TextureCache(); // needs virtual for DX11 dtor

	static void Cleanup();

	static void Invalidate(bool shutdown);
	static void InvalidateDefer();
	static void InvalidateRange(u32 start_address, u32 size);
	static void MakeRangeDynamic(u32 start_address, u32 size);
	static void ClearRenderTargets();	// currently only used by OGL
	static bool Find(u32 start_address, u64 hash);

	virtual TCacheEntryBase* CreateTexture(unsigned int width, unsigned int height,
		unsigned int expanded_width, unsigned int tex_levels, PC_TexFormat pcfmt) = 0;
	virtual TCacheEntryBase* CreateRenderTargetTexture(unsigned int scaled_tex_w, unsigned int scaled_tex_h) = 0;

	static TCacheEntryBase* Load(unsigned int stage, u32 address, unsigned int width, unsigned int height,
		int format, unsigned int tlutaddr, int tlutfmt, bool UseNativeMips, unsigned int maxlevel);
	static void CopyRenderTargetToTexture(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	static bool DeferredInvalidate;

protected:
	TextureCache();

	static  GC_ALIGNED16(u8 *temp);

private:
	typedef std::map<u32, TCacheEntryBase*> TexCache;

	static TexCache textures;
};

extern TextureCache *g_texture_cache;

#endif
