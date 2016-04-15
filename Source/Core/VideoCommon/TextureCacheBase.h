// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>

#include "Common/CommonTypes.h"
#include "VideoCommon/BPMemory.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"

struct VideoConfig;

class TextureCacheBase
{
public:
	struct TCacheEntryConfig
	{
		constexpr TCacheEntryConfig() = default;

		bool operator==(const TCacheEntryConfig& o) const
		{
			return std::tie(width, height, levels, layers, rendertarget) ==
			       std::tie(o.width, o.height, o.levels, o.layers, o.rendertarget);
		}

		struct Hasher : std::hash<u64>
		{
			size_t operator()(const TCacheEntryConfig& c) const
			{
				u64 id = (u64)c.rendertarget << 63 | (u64)c.layers << 48 | (u64)c.levels << 32 | (u64)c.height << 16 | (u64)c.width;
				return std::hash<u64>::operator()(id);
			}
		};

		u32 width  = 0;
		u32 height = 0;
		u32 levels = 1;
		u32 layers = 1;
		bool rendertarget = false;
	};

	struct TCacheEntryBase
	{
		const TCacheEntryConfig config;

		// common members
		u32 addr;
		u32 size_in_bytes;
		u64 base_hash;
		u64 hash; // for paletted textures, hash = base_hash ^ palette_hash
		u32 format; // bits 0-3 will contain the in-memory format.
		bool is_efb_copy;
		bool is_custom_tex;
		u32 memory_stride;

		unsigned int native_width, native_height; // Texture dimensions from the GameCube's point of view
		unsigned int native_levels;

		// used to delete textures which haven't been used for TEXTURE_KILL_THRESHOLD frames
		int frameCount;

		// Keep an iterator to the entry in textures_by_hash, so it does not need to be searched when removing the cache entry
		std::multimap<u64, TCacheEntryBase*>::iterator textures_by_hash_iter;

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
			memory_stride = _native_width;
		}

		void SetHashes(u64 _base_hash, u64 _hash)
		{
			base_hash = _base_hash;
			hash = _hash;
		}

		void SetEfbCopy(u32 stride);

		TCacheEntryBase(const TCacheEntryConfig& c) : config(c) {}
		virtual ~TCacheEntryBase();

		virtual void Bind(unsigned int stage) = 0;
		virtual bool Save(const std::string& filename, unsigned int level) = 0;

		virtual void CopyRectangleFromTexture(
			const TCacheEntryBase* source,
			const MathUtil::Rectangle<int> &srcrect,
			const MathUtil::Rectangle<int> &dstrect) = 0;

		virtual void Load(unsigned int width, unsigned int height,
			unsigned int expanded_width, unsigned int level) = 0;
		virtual void FromRenderTarget(u8* dst, PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
			bool scaleByHalf, unsigned int cbufid, const float *colmat) = 0;

		bool OverlapsMemoryRange(u32 range_address, u32 range_size) const;

		TextureCacheBase::TCacheEntryBase* ApplyPalette(u8* palette, u32 tlutfmt);

		bool IsEfbCopy() const { return is_efb_copy; }

		u32 NumBlocksY() const;
		u32 BytesPerRow() const;

		u64 CalculateHash() const;
	};

	virtual ~TextureCacheBase(); // needs virtual for DX11 dtor

	static void OnConfigChanged(VideoConfig& config);

	// Removes textures which aren't used for more than TEXTURE_KILL_THRESHOLD frames,
	// frameCount is the current frame number.
	static void Cleanup(int _frameCount);

	static void Invalidate();

	virtual TCacheEntryBase* CreateTexture(const TCacheEntryConfig& config) = 0;

	virtual void CopyEFB(u8* dst, u32 format, u32 native_width, u32 bytes_per_row, u32 num_blocks_y, u32 memory_stride,
		PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect,
		bool isIntensity, bool scaleByHalf) = 0;

	virtual void CompileShaders() = 0; // currently only implemented by OGL
	virtual void DeleteShaders() = 0; // currently only implemented by OGL

	static TCacheEntryBase* Load(const u32 stage);
	static void UnbindTextures();
	virtual void BindTextures();
	static void CopyRenderTargetToTexture(u32 dstAddr, unsigned int dstFormat, u32 dstStride,
		PEControl::PixelFormat srcFormat, const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);

	virtual void ConvertTexture(TCacheEntryBase* entry, TCacheEntryBase* unconverted, void* palette, TlutFormat format) = 0;

protected:
	TextureCacheBase();

	alignas(16) static u8* temp;
	static size_t temp_size;

	static TCacheEntryBase* bound_textures[8];

private:
	typedef std::multimap<u64, TCacheEntryBase*> TexCache;
	typedef std::unordered_multimap<TCacheEntryConfig, TCacheEntryBase*, TCacheEntryConfig::Hasher> TexPool;
	static void ScaleTextureCacheEntryTo(TCacheEntryBase** entry, u32 new_width, u32 new_height);
	static TCacheEntryBase* DoPartialTextureUpdates(TexCache::iterator iter, u8* palette, u32 tlutfmt);
	static void DumpTexture(TCacheEntryBase* entry, std::string basename, unsigned int level);
	static void CheckTempSize(size_t required_size);

	static TCacheEntryBase* AllocateTexture(const TCacheEntryConfig& config);
	static TexCache::iterator GetTexCacheIter(TCacheEntryBase* entry);
	static TexCache::iterator FreeTexture(TexCache::iterator t_iter);

	static TCacheEntryBase* ReturnEntry(unsigned int stage, TCacheEntryBase* entry);

	static TexCache textures_by_address;
	static TexCache textures_by_hash;
	static TexPool texture_pool;

	// Backup configuration values
	static struct BackupConfig
	{
		int s_colorsamples;
		bool s_texfmt_overlay;
		bool s_texfmt_overlay_center;
		bool s_hires_textures;
		bool s_cache_hires_textures;
		bool s_copy_cache_enable;
		bool s_stereo_3d;
		bool s_efb_mono_depth;
	} backup_config;
};

extern std::unique_ptr<TextureCacheBase> g_texture_cache;
