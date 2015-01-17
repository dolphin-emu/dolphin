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

	struct TCacheEntryConfig
	{
		TCacheEntryConfig() : width(0), height(0), levels(1), layers(1), rendertarget(false) {}

		u32 width, height;
		u32 levels, layers;
		bool rendertarget;
	};

	struct TCacheEntryBase
	{
		const TCacheEntryConfig config;

		// common members
		u32 addr;
		u32 size_in_bytes;
		u64 hash;
		u32 format;

		enum TexCacheEntryType type;

		unsigned int native_width, native_height; // Texture dimensions from the GameCube's point of view
		unsigned int native_levels;

		// used to delete textures which haven't been used for TEXTURE_KILL_THRESHOLD frames
		int frameCount;


		void SetGeneralParameters(u32 _addr, u32 _size, u32 _format)
		{
			addr = _addr;
			size_in_bytes = _size;
			format = _format;
		}

		void SetDimensions(unsigned int _native_width, unsigned int _native_height, unsigned int _native_levels)
		{
			native_width = _native_width;
			native_height = _native_height;
			native_levels = _native_levels;
		}

		void SetHashes(u64 _hash)
		{
			hash = _hash;
		}

		TCacheEntryBase(const TCacheEntryConfig& c) : config(c) {}
		virtual ~TCacheEntryBase();

		virtual void Bind(unsigned int stage) = 0;
		virtual bool Save(const std::string& filename, unsigned int level) = 0;

		virtual void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int level) = 0;
		virtual void FromRenderTarget(u32 dstAddr, unsigned int dstFormat,
			PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
			bool isIntensity, bool scaleByHalf, unsigned int cbufid,
			const float *colmat) = 0;

		bool OverlapsMemoryRange(u32 range_address, u32 range_size) const;

		bool IsEfbCopy() { return (type == TCET_EC_VRAM || type == TCET_EC_DYNAMIC); }
	};

	virtual ~TextureCache(); // needs virtual for DX11 dtor

	static void OnConfigChanged(VideoConfig& config);

	// Removes textures which aren't used for more than TEXTURE_KILL_THRESHOLD frames,
	// frameCount is the current frame number.
	static void Cleanup(int frameCount);

	static void Invalidate();
	static void InvalidateRange(u32 start_address, u32 size);
	static void MakeRangeDynamic(u32 start_address, u32 size);
	static void ClearRenderTargets(); // currently only used by OGL
	static bool Find(u32 start_address, u64 hash);

	virtual TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) = 0;

	virtual void CompileShaders() = 0; // currently only implemented by OGL
	virtual void DeleteShaders() = 0; // currently only implemented by OGL

	static TCacheEntryBase* Load(const u32 stage);
	static void CopyRenderTargetToTexture(u32 dstAddr, unsigned int dstFormat, PEControl::PixelFormat srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	static void RequestInvalidateTextureCache();

protected:
	TextureCache();

	static GC_ALIGNED16(u8 *temp);
	static size_t temp_size;

private:
	static void DumpTexture(TCacheEntryBase* entry, std::string basename, unsigned int level);
	static void CheckTempSize(size_t required_size);

	static TCacheEntryBase* AllocateRenderTarget(unsigned int width, unsigned int height, unsigned int layers);
	static void FreeRenderTarget(TCacheEntryBase* entry);

	typedef std::map<u32, TCacheEntryBase*> TexCache;
	typedef std::vector<TCacheEntryBase*> RenderTargetPool;

	static TexCache textures;
	static RenderTargetPool render_target_pool;

	// Backup configuration values
	static struct BackupConfig
	{
		int s_colorsamples;
		bool s_texfmt_overlay;
		bool s_texfmt_overlay_center;
		bool s_hires_textures;
		bool s_copy_cache_enable;
		bool s_stereo_3d;
		bool s_efb_mono_depth;
	} backup_config;
};

extern TextureCache *g_texture_cache;
