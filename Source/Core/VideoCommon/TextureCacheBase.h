// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "Common/CommonTypes.h"
#include "Common/Thread.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

struct VideoConfig;

class TextureCache
{
public:
	enum TexCacheEntryType
	{
		TCET_NORMAL,
		TCET_EC_VRAM,    // EFB copy which sits in VRAM and is ready to be used
		TCET_EC_DYNAMIC, // EFB copy which sits in RAM and needs to be decoded before being used
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
		unsigned int num_layers;
		unsigned int native_width, native_height; // Texture dimensions from the GameCube's point of view
		unsigned int virtual_width, virtual_height; // Texture dimensions from OUR point of view - for hires textures or scaled EFB copies

		// used to delete textures which haven't been used for TEXTURE_KILL_THRESHOLD frames
		int frameCount;


		void SetGeneralParameters(u32 _addr, u32 _size, u32 _format, unsigned int _num_mipmaps, unsigned int _num_layers)
		{
			addr = _addr;
			size_in_bytes = _size;
			format = _format;
			num_mipmaps = _num_mipmaps;
			num_layers = _num_layers;
		}

		void SetDimensions(unsigned int _native_width, unsigned int _native_height, unsigned int _virtual_width, unsigned int _virtual_height)
		{
			native_width = _native_width;
			native_height = _native_height;
			virtual_width = _virtual_width;
			virtual_height = _virtual_height;
		}

		void SetHashes(u64 _hash/*, u32 _pal_hash*/)
		{
			hash = _hash;
			//pal_hash = _pal_hash;
		}


		virtual ~TCacheEntryBase();

		virtual void Bind(unsigned int stage) = 0;
		virtual bool Save(const std::string& filename, unsigned int level) = 0;

		virtual void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int level) = 0;
		virtual void FromRenderTarget(u32 dstAddr, unsigned int dstFormat,
			PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
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
	static void ClearRenderTargets(); // currently only used by OGL
	static bool Find(u32 start_address, u64 hash);

	virtual TCacheEntryBase* CreateTexture(unsigned int width, unsigned int height,
		unsigned int expanded_width, unsigned int tex_levels, PC_TexFormat pcfmt) = 0;
	virtual TCacheEntryBase* CreateRenderTargetTexture(unsigned int scaled_tex_w, unsigned int scaled_tex_h) = 0;

	virtual void CompileShaders() = 0; // currently only implemented by OGL
	virtual void DeleteShaders() = 0; // currently only implemented by OGL

	static TCacheEntryBase* Load(unsigned int stage, u32 address, unsigned int width, unsigned int height,
		int format, unsigned int tlutaddr, int tlutfmt, bool use_mipmaps, unsigned int maxlevel, bool from_tmem);
	static void CopyRenderTargetToTexture(u32 dstAddr, unsigned int dstFormat, PEControl::PixelFormat srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	static void RequestInvalidateTextureCache();

protected:
	TextureCache();

	static  GC_ALIGNED16(u8 *temp);
	static unsigned int temp_size;

private:
	static bool CheckForCustomTextureLODs(u64 tex_hash, int texformat, unsigned int levels);
	static PC_TexFormat LoadCustomTexture(u64 tex_hash, int texformat, unsigned int level, unsigned int* width, unsigned int* height);
	static void DumpTexture(TCacheEntryBase* entry, unsigned int level);

	static TCacheEntryBase* AllocateRenderTarget(unsigned int width, unsigned int height);
	static void FreeRenderTarget(TCacheEntryBase* entry);

	typedef std::map<u32, TCacheEntryBase*> TexCache;
	typedef std::vector<TCacheEntryBase*> RenderTargetPool;

	static TexCache textures;
	static RenderTargetPool render_target_pool;

	// Backup configuration values
	static struct BackupConfig
	{
		int s_colorsamples;
		bool s_copy_efb_to_texture;
		bool s_copy_efb_scaled;
		bool s_copy_efb;
		int s_efb_scale;
		bool s_texfmt_overlay;
		bool s_texfmt_overlay_center;
		bool s_hires_textures;
		bool s_copy_cache_enable;
		bool s_stereo_3d;
		bool s_mono_efb_depth;
	} backup_config;
};

extern TextureCache *g_texture_cache;
