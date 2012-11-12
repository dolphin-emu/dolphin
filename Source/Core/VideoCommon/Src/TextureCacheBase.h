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

#ifndef _TEXTURECACHEBASE_H
#define _TEXTURECACHEBASE_H

#include <map>

#include "VideoCommon.h"
#include "TextureDecoder.h"
#include "BPMemory.h"
#include "Thread.h"

#include "CommonTypes.h"

struct VideoConfig;

class TextureCache
{
public:
	enum TexCacheEntryType
	{
		TCET_NORMAL,
		TCET_EC_VRAM,		// EFB copy which sits in VRAM and is ready to be used
		TCET_EC_DYNAMIC,	// EFB copy which sits in RAM and needs to be decoded before being used
	};

	struct TCacheEntryBase
	{
#define TEXHASH_INVALID 0

		// common members
		u32 addr;
		u32 size_in_bytes;
		u64 hash;
		//u32 pal_hash;
		u32 format;

		enum TexCacheEntryType type;

		unsigned int num_mipmaps;
		unsigned int native_width, native_height; // Texture dimensions from the GameCube's point of view
		unsigned int virtual_width, virtual_height; // Texture dimensions from OUR point of view - for hires textures or scaled EFB copies

		// used to delete textures which haven't been used for TEXTURE_KILL_THRESHOLD frames
		int frameCount;


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


		virtual ~TCacheEntryBase();

		virtual void Bind(unsigned int stage) = 0;
		virtual bool Save(const char filename[], unsigned int level) = 0;

		virtual void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int level, bool autogen_mips) = 0;
		virtual void FromRenderTarget(u32 dstAddr, unsigned int dstFormat,
			unsigned int srcFormat, const EFBRectangle& srcRect,
			bool isIntensity, bool scaleByHalf, unsigned int cbufid,
			const float *colmat) = 0;

		int IntersectsMemoryRange(u32 range_address, u32 range_size) const;

		bool IsEfbCopy() { return (type == TCET_EC_VRAM || type == TCET_EC_DYNAMIC); }
	};

	virtual ~TextureCache(); // needs virtual for DX11 dtor

	static void OnConfigChanged(VideoConfig& config);
	static void Cleanup();

	static void Invalidate();
	static void InvalidateRange(u32 start_address, u32 size);
	static void MakeRangeDynamic(u32 start_address, u32 size);
	static void ClearRenderTargets();	// currently only used by OGL
	static bool Find(u32 start_address, u64 hash);

	virtual TCacheEntryBase* CreateTexture(unsigned int width, unsigned int height,
		unsigned int expanded_width, unsigned int tex_levels, PC_TexFormat pcfmt) = 0;
	virtual TCacheEntryBase* CreateRenderTargetTexture(unsigned int scaled_tex_w, unsigned int scaled_tex_h) = 0;

	static TCacheEntryBase* Load(unsigned int stage, u32 address, unsigned int width, unsigned int height,
		int format, unsigned int tlutaddr, int tlutfmt, bool UseNativeMips, unsigned int maxlevel, bool from_tmem);
	static void CopyRenderTargetToTexture(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

protected:
	TextureCache();

	static  GC_ALIGNED16(u8 *temp);
	static unsigned int temp_size;

private:
	static bool CheckForCustomTextureLODs(u64 tex_hash, int texformat, unsigned int levels);
	static PC_TexFormat LoadCustomTexture(u64 tex_hash, int texformat, unsigned int level, unsigned int& width, unsigned int& height);
	static void DumpTexture(TCacheEntryBase* entry, unsigned int level);


	typedef std::map<u32, TCacheEntryBase*> TexCache;

	static TexCache textures;

	// Backup configuration values
	static struct BackupConfig {
		int s_colorsamples;
		bool s_copy_efb_to_texture;
		bool s_copy_efb_scaled;
		bool s_copy_efb;
		int s_efb_scale;
		bool s_texfmt_overlay;
		bool s_texfmt_overlay_center;
		bool s_hires_textures;
		bool s_copy_cache_enable;
	} backup_config;
};

extern TextureCache *g_texture_cache;

#endif
