// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "Common/Assert.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Hash.h"
#include "Common/MemoryUtil.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"

#include "Core/ConfigManager.h"
#include "Core/FifoPlayer/FifoPlayer.h"
#include "Core/FifoPlayer/FifoRecorder.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/BPMemory.h"
#include "VideoCommon/Debugger.h"
#include "VideoCommon/FramebufferManagerBase.h"
#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/TextureDecoder.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

static const u64 TEXHASH_INVALID = 0;
static const int TEXTURE_KILL_THRESHOLD = 64; // Sonic the Fighters (inside Sonic Gems Collection) loops a 64 frames animation
static const int TEXTURE_POOL_KILL_THRESHOLD = 3;
static const int FRAMECOUNT_INVALID = 0;

std::unique_ptr<TextureCacheBase> g_texture_cache;

alignas(16) u8* TextureCacheBase::temp = nullptr;
size_t TextureCacheBase::temp_size;

TextureCacheBase::TexCache TextureCacheBase::textures_by_address;
TextureCacheBase::TexCache TextureCacheBase::textures_by_hash;
TextureCacheBase::TexPool TextureCacheBase::texture_pool;
TextureCacheBase::TCacheEntryBase* TextureCacheBase::bound_textures[8];

TextureCacheBase::BackupConfig TextureCacheBase::backup_config;

TextureCacheBase::TCacheEntryBase::~TCacheEntryBase()
{
}

void TextureCacheBase::CheckTempSize(size_t required_size)
{
	if (required_size <= temp_size)
		return;

	temp_size = required_size;
	FreeAlignedMemory(temp);
	temp = (u8*)AllocateAlignedMemory(temp_size, 16);
}

TextureCacheBase::TextureCacheBase()
{
	temp_size = 2048 * 2048 * 4;
	if (!temp)
		temp = (u8*)AllocateAlignedMemory(temp_size, 16);

	TexDecoder_SetTexFmtOverlayOptions(g_ActiveConfig.bTexFmtOverlayEnable, g_ActiveConfig.bTexFmtOverlayCenter);

	HiresTexture::Init();

	SetHash64Function();
}

void TextureCacheBase::Invalidate()
{
	UnbindTextures();

	for (auto& tex : textures_by_address)
	{
		delete tex.second;
	}
	textures_by_address.clear();
	textures_by_hash.clear();

	for (auto& rt : texture_pool)
	{
		delete rt.second;
	}
	texture_pool.clear();
}

TextureCacheBase::~TextureCacheBase()
{
	HiresTexture::Shutdown();
	Invalidate();
	FreeAlignedMemory(temp);
	temp = nullptr;
}

void TextureCacheBase::OnConfigChanged(VideoConfig& config)
{
	if (g_texture_cache)
	{
		if (config.bHiresTextures != backup_config.s_hires_textures ||
			config.bCacheHiresTextures != backup_config.s_cache_hires_textures)
		{
			HiresTexture::Update();
		}

		// TODO: Invalidating texcache is really stupid in some of these cases
		if (config.iSafeTextureCache_ColorSamples != backup_config.s_colorsamples ||
			config.bTexFmtOverlayEnable != backup_config.s_texfmt_overlay ||
			config.bTexFmtOverlayCenter != backup_config.s_texfmt_overlay_center ||
			config.bHiresTextures != backup_config.s_hires_textures)
		{
			g_texture_cache->Invalidate();

			TexDecoder_SetTexFmtOverlayOptions(g_ActiveConfig.bTexFmtOverlayEnable, g_ActiveConfig.bTexFmtOverlayCenter);
		}

		if ((config.iStereoMode > 0) != backup_config.s_stereo_3d ||
			config.bStereoEFBMonoDepth != backup_config.s_efb_mono_depth)
		{
			g_texture_cache->DeleteShaders();
			g_texture_cache->CompileShaders();
		}
	}

	backup_config.s_colorsamples = config.iSafeTextureCache_ColorSamples;
	backup_config.s_texfmt_overlay = config.bTexFmtOverlayEnable;
	backup_config.s_texfmt_overlay_center = config.bTexFmtOverlayCenter;
	backup_config.s_hires_textures = config.bHiresTextures;
	backup_config.s_cache_hires_textures = config.bCacheHiresTextures;
	backup_config.s_stereo_3d = config.iStereoMode > 0;
	backup_config.s_efb_mono_depth = config.bStereoEFBMonoDepth;
}

void TextureCacheBase::Cleanup(int _frameCount)
{
	TexCache::iterator iter = textures_by_address.begin();
	TexCache::iterator tcend = textures_by_address.end();
	while (iter != tcend)
	{
		if (iter->second->frameCount == FRAMECOUNT_INVALID)
		{
			iter->second->frameCount = _frameCount;
			++iter;
		}
		else if (_frameCount > TEXTURE_KILL_THRESHOLD + iter->second->frameCount)
		{
			if (iter->second->IsEfbCopy())
			{
				// Only remove EFB copies when they wouldn't be used anymore(changed hash), because EFB copies living on the
				// host GPU are unrecoverable. Perform this check only every TEXTURE_KILL_THRESHOLD for performance reasons
				if ((_frameCount - iter->second->frameCount) % TEXTURE_KILL_THRESHOLD == 1 &&
					iter->second->hash != iter->second->CalculateHash())
				{
					iter = FreeTexture(iter);
				}
				else
				{
					++iter;
				}
			}
			else
			{
				iter = FreeTexture(iter);
			}
		}
		else
		{
			++iter;
		}
	}

	TexPool::iterator iter2 = texture_pool.begin();
	TexPool::iterator tcend2 = texture_pool.end();
	while (iter2 != tcend2)
	{
		if (iter2->second->frameCount == FRAMECOUNT_INVALID)
		{
			iter2->second->frameCount = _frameCount;
		}
		if (_frameCount > TEXTURE_POOL_KILL_THRESHOLD + iter2->second->frameCount)
		{
			delete iter2->second;
			iter2 = texture_pool.erase(iter2);
		}
		else
		{
			++iter2;
		}
	}
}

bool TextureCacheBase::TCacheEntryBase::OverlapsMemoryRange(u32 range_address, u32 range_size) const
{
	if (addr + size_in_bytes <= range_address)
		return false;

	if (addr >= range_address + range_size)
		return false;

	return true;
}

void TextureCacheBase::ScaleTextureCacheEntryTo(TextureCacheBase::TCacheEntryBase** entry, u32 new_width, u32 new_height)
{
	if ((*entry)->config.width == new_width && (*entry)->config.height == new_height)
	{
		return;
	}

	u32 max = g_renderer->GetMaxTextureSize();
	if (max < new_width || max < new_height)
	{
		ERROR_LOG(VIDEO, "Texture too big, width = %d, height = %d", new_width, new_height);
		return;
	}

	TextureCacheBase::TCacheEntryConfig newconfig;
	newconfig.width = new_width;
	newconfig.height = new_height;
	newconfig.layers = (*entry)->config.layers;
	newconfig.rendertarget = true;

	TCacheEntryBase* newentry = AllocateTexture(newconfig);
	if (newentry)
	{
		newentry->SetGeneralParameters((*entry)->addr, (*entry)->size_in_bytes, (*entry)->format);
		newentry->SetDimensions((*entry)->native_width, (*entry)->native_height, 1);
		newentry->SetHashes((*entry)->base_hash, (*entry)->hash);
		newentry->frameCount = frameCount;
		newentry->is_efb_copy = false;
		MathUtil::Rectangle<int> srcrect, dstrect;
		srcrect.left = 0;
		srcrect.top = 0;
		srcrect.right = (*entry)->config.width;
		srcrect.bottom = (*entry)->config.height;
		dstrect.left = 0;
		dstrect.top = 0;
		dstrect.right = new_width;
		dstrect.bottom = new_height;
		newentry->CopyRectangleFromTexture(*entry, srcrect, dstrect);

		// Keep track of the pointer for textures_by_hash
		if ((*entry)->textures_by_hash_iter != textures_by_hash.end())
		{
			newentry->textures_by_hash_iter = textures_by_hash.emplace((*entry)->hash, newentry);
		}

		// Remove the old texture
		std::pair<TexCache::iterator, TexCache::iterator>iter_range = textures_by_address.equal_range((*entry)->addr);
		TexCache::iterator iter = iter_range.first;
		while (iter != iter_range.second)
		{
			if (iter->second == *entry)
			{
				FreeTexture(iter);
				iter = iter_range.second;
			}
			else
			{
				iter++;
			}
		}

		*entry = newentry;
		textures_by_address.emplace((*entry)->addr, *entry);
	}
	else
	{
		ERROR_LOG(VIDEO, "Scaling failed");
	}
}

TextureCacheBase::TCacheEntryBase* TextureCacheBase::DoPartialTextureUpdates(TexCache::iterator iter_t)
{
	TCacheEntryBase* entry_to_update = iter_t->second;
	const bool isPaletteTexture = (entry_to_update->format == GX_TF_C4
		|| entry_to_update->format == GX_TF_C8
		|| entry_to_update->format == GX_TF_C14X2
		|| entry_to_update->format >= 0x10000);

	// Efb copies and paletted textures are excluded from these updates, until there's an example where a game would
	// benefit from this. Both would require more work to be done.
	// TODO: Implement upscaling support for normal textures, and then remove the efb to ram and the scaled efb restrictions
	if (entry_to_update->IsEfbCopy()
		|| isPaletteTexture)
		return entry_to_update;

	u32 block_width = TexDecoder_GetBlockWidthInTexels(entry_to_update->format & 0xf);
	u32 block_height = TexDecoder_GetBlockHeightInTexels(entry_to_update->format & 0xf);
	u32 block_size = block_width * block_height * TexDecoder_GetTexelSizeInNibbles(entry_to_update->format & 0xf) / 2;

	u32 numBlocksX = (entry_to_update->native_width + block_width - 1) / block_width;

	TexCache::iterator iter = textures_by_address.lower_bound(entry_to_update->addr);
	TexCache::iterator iterend = textures_by_address.upper_bound(entry_to_update->addr + entry_to_update->size_in_bytes);
	while (iter != iterend)
	{
		TCacheEntryBase* entry = iter->second;
		if (entry != entry_to_update
			&& entry->IsEfbCopy()
			&& entry_to_update->addr <= entry->addr
			&& entry->addr + entry->size_in_bytes <= entry_to_update->addr + entry_to_update->size_in_bytes
			&& entry->frameCount == FRAMECOUNT_INVALID
			&& entry->memory_stride == numBlocksX * block_size)
		{
			if (entry->hash == entry->CalculateHash())
			{
				u32 block_offset = (entry->addr - entry_to_update->addr) / block_size;
				u32 block_x = block_offset % numBlocksX;
				u32 block_y = block_offset / numBlocksX;

				u32 dst_x = block_x * block_width;
				u32 dst_y = block_y * block_height;
				u32 src_x = 0;
				u32 src_y = 0;

				// If the EFB copy doesn't fully fit, cancel the copy process
				if (entry->native_width - src_x > entry_to_update->native_width - dst_x
					|| entry->native_height - src_y > entry_to_update->native_height - dst_y)
				{
					iter++;
					continue;
				}

				u32 copy_width = std::min(entry->native_width - src_x, entry_to_update->native_width - dst_x);
				u32 copy_height = std::min(entry->native_height - src_y, entry_to_update->native_height - dst_y);

				// If one of the textures is scaled, scale both with the current efb scaling factor
				if (entry_to_update->native_width != entry_to_update->config.width
					|| entry_to_update->native_height != entry_to_update->config.height
					|| entry->native_width != entry->config.width || entry->native_height != entry->config.height)
				{
					ScaleTextureCacheEntryTo(&entry_to_update, Renderer::EFBToScaledX(entry_to_update->native_width), Renderer::EFBToScaledY(entry_to_update->native_height));
					ScaleTextureCacheEntryTo(&entry, Renderer::EFBToScaledX(entry->native_width), Renderer::EFBToScaledY(entry->native_height));

					src_x = Renderer::EFBToScaledX(src_x);
					src_y = Renderer::EFBToScaledY(src_y);
					dst_x = Renderer::EFBToScaledX(dst_x);
					dst_y = Renderer::EFBToScaledY(dst_y);
					copy_width = Renderer::EFBToScaledX(copy_width);
					copy_height = Renderer::EFBToScaledY(copy_height);
				}

				MathUtil::Rectangle<int> srcrect, dstrect;
				srcrect.left = src_x;
				srcrect.top = src_y;
				srcrect.right = (src_x + copy_width);
				srcrect.bottom = (src_y + copy_height);
				dstrect.left = dst_x;
				dstrect.top = dst_y;
				dstrect.right = (dst_x + copy_width);
				dstrect.bottom = (dst_y + copy_height);
				entry_to_update->CopyRectangleFromTexture(entry, srcrect, dstrect);
				// Mark the texture update as used, so it isn't applied more than once
				entry->frameCount = frameCount;
			}
			else
			{
				// If the hash does not match, this EFB copy will not be used for anything, so remove it
				iter = FreeTexture(iter);
				continue;
			}
		}
		++iter;
	}
	return entry_to_update;
}

void TextureCacheBase::DumpTexture(TCacheEntryBase* entry, std::string basename, unsigned int level)
{
	std::string szDir = File::GetUserPath(D_DUMPTEXTURES_IDX) +
		SConfig::GetInstance().m_strUniqueID;

	// make sure that the directory exists
	if (!File::Exists(szDir) || !File::IsDirectory(szDir))
		File::CreateDir(szDir);

	if (level > 0)
	{
		basename += StringFromFormat("_mip%i", level);
	}
	std::string filename = szDir + "/" + basename + ".png";

	if (!File::Exists(filename))
		entry->Save(filename, level);
}

static u32 CalculateLevelSize(u32 level_0_size, u32 level)
{
	return std::max(level_0_size >> level, 1u);
}

// Used by TextureCacheBase::Load
TextureCacheBase::TCacheEntryBase* TextureCacheBase::ReturnEntry(unsigned int stage, TCacheEntryBase* entry)
{
	entry->frameCount = FRAMECOUNT_INVALID;
	bound_textures[stage] = entry;

	GFX_DEBUGGER_PAUSE_AT(NEXT_TEXTURE_CHANGE, true);

	return entry;
}

void TextureCacheBase::BindTextures()
{
	for (int i = 0; i < 8; ++i)
	{
		if (bound_textures[i])
			bound_textures[i]->Bind(i);
	}
}

void TextureCacheBase::UnbindTextures()
{
	std::fill(std::begin(bound_textures), std::end(bound_textures), nullptr);
}

TextureCacheBase::TCacheEntryBase* TextureCacheBase::Load(const u32 stage)
{
	const FourTexUnits &tex = bpmem.tex[stage >> 2];
	const u32 id = stage & 3;
	const u32 address = (tex.texImage3[id].image_base/* & 0x1FFFFF*/) << 5;
	u32 width = tex.texImage0[id].width + 1;
	u32 height = tex.texImage0[id].height + 1;
	const int texformat = tex.texImage0[id].format;
	const u32 tlutaddr = tex.texTlut[id].tmem_offset << 9;
	const u32 tlutfmt = tex.texTlut[id].tlut_format;
	const bool use_mipmaps = (tex.texMode0[id].min_filter & 3) != 0;
	u32 tex_levels = use_mipmaps ? ((tex.texMode1[id].max_lod + 0xf) / 0x10 + 1) : 1;
	const bool from_tmem = tex.texImage1[id].image_type != 0;

	if (0 == address)
		return nullptr;

	// TexelSizeInNibbles(format) * width * height / 16;
	const unsigned int bsw = TexDecoder_GetBlockWidthInTexels(texformat);
	const unsigned int bsh = TexDecoder_GetBlockHeightInTexels(texformat);

	unsigned int expandedWidth = ROUND_UP(width, bsw);
	unsigned int expandedHeight = ROUND_UP(height, bsh);
	const unsigned int nativeW = width;
	const unsigned int nativeH = height;

	// Hash assigned to texcache entry (also used to generate filenames used for texture dumping and custom texture lookup)
	u64 base_hash = TEXHASH_INVALID;
	u64 full_hash = TEXHASH_INVALID;

	u32 full_format = texformat;

	const bool isPaletteTexture = (texformat == GX_TF_C4 || texformat == GX_TF_C8 || texformat == GX_TF_C14X2);

	// Reject invalid tlut format.
	if (isPaletteTexture && tlutfmt > GX_TL_RGB5A3)
		return nullptr;

	if (isPaletteTexture)
		full_format = texformat | (tlutfmt << 16);

	const u32 texture_size = TexDecoder_GetTextureSizeInBytes(expandedWidth, expandedHeight, texformat);
	u32 additional_mips_size = 0; // not including level 0, which is texture_size

	// GPUs don't like when the specified mipmap count would require more than one 1x1-sized LOD in the mipmap chain
	// e.g. 64x64 with 7 LODs would have the mipmap chain 64x64,32x32,16x16,8x8,4x4,2x2,1x1,0x0, so we limit the mipmap count to 6 there
	tex_levels = std::min<u32>(IntLog2(std::max(width, height)) + 1, tex_levels);

	for (u32 level = 1; level != tex_levels; ++level)
	{
		// We still need to calculate the original size of the mips
		const u32 expanded_mip_width = ROUND_UP(CalculateLevelSize(width, level), bsw);
		const u32 expanded_mip_height = ROUND_UP(CalculateLevelSize(height, level), bsh);

		additional_mips_size += TexDecoder_GetTextureSizeInBytes(expanded_mip_width, expanded_mip_height, texformat);
	}

	const u8* src_data;
	if (from_tmem)
		src_data = &texMem[bpmem.tex[stage / 4].texImage1[stage % 4].tmem_even * TMEM_LINE_SIZE];
	else
		src_data = Memory::GetPointer(address);

	if (!src_data)
	{
		ERROR_LOG(VIDEO, "Trying to use an invalid texture address 0x%8x", address);
		return nullptr;
	}

	// If we are recording a FifoLog, keep track of what memory we read.
	// FifiRecorder does it's own memory modification tracking independant of the texture hashing below.
	if (g_bRecordFifoData && !from_tmem)
		FifoRecorder::GetInstance().UseMemory(address, texture_size + additional_mips_size, MemoryUpdate::TEXTURE_MAP);

	// TODO: This doesn't hash GB tiles for preloaded RGBA8 textures (instead, it's hashing more data from the low tmem bank than it should)
	base_hash = GetHash64(src_data, texture_size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
	u32 palette_size = 0;
	if (isPaletteTexture)
	{
		palette_size = TexDecoder_GetPaletteSize(texformat);
		full_hash = base_hash ^ GetHash64(&texMem[tlutaddr], palette_size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
	}
	else
	{
		full_hash = base_hash;
	}

	// Search the texture cache for textures by address
	//
	// Find all texture cache entries for the current texture address, and decide whether to use one of
	// them, or to create a new one
	//
	// In most cases, the fastest way is to use only one texture cache entry for the same address. Usually,
	// when a texture changes, the old version of the texture is unlikely to be used again. If there were
	// new cache entries created for normal texture updates, there would be a slowdown due to a huge amount
	// of unused cache entries. Also thanks to texture pooling, overwriting an existing cache entry is
	// faster than creating a new one from scratch.
	//
	// Some games use the same address for different textures though. If the same cache entry was used in
	// this case, it would be constantly overwritten, and effectively there wouldn't be any caching for
	// those textures. Examples for this are Metroid Prime and Castlevania 3. Metroid Prime has multiple
	// sets of fonts on each other stored in a single texture and uses the palette to make different
	// characters visible or invisible. In Castlevania 3 some textures are used for 2 different things or
	// at least in 2 different ways(size 1024x1024 vs 1024x256).
	//
	// To determine whether to use multiple cache entries or a single entry, use the following heuristic:
	// If the same texture address is used several times during the same frame, assume the address is used
	// for different purposes and allow creating an additional cache entry. If there's at least one entry
	// that hasn't been used for the same frame, then overwrite it, in order to keep the cache as small as
	// possible. If the current texture is found in the cache, use that entry.
	//
	// For efb copies, the entry created in CopyRenderTargetToTexture always has to be used, or else it was
	// done in vain.
	std::pair<TexCache::iterator, TexCache::iterator> iter_range = textures_by_address.equal_range((u64)address);
	TexCache::iterator iter = iter_range.first;
	TexCache::iterator oldest_entry = iter;
	int temp_frameCount = 0x7fffffff;
	TexCache::iterator unconverted_copy = textures_by_address.end();

	while (iter != iter_range.second)
	{
		TCacheEntryBase* entry = iter->second;
		// Do not load strided EFB copies, they are not meant to be used directly
		if (entry->IsEfbCopy() && entry->native_width == nativeW && entry->native_height == nativeH &&
			entry->memory_stride == entry->BytesPerRow())
		{
			// EFB copies have slightly different rules as EFB copy formats have different
			// meanings from texture formats.
			if ((base_hash == entry->hash && (!isPaletteTexture || g_Config.backend_info.bSupportsPaletteConversion)) ||
				IsPlayingBackFifologWithBrokenEFBCopies)
			{
				// TODO: We should check format/width/height/levels for EFB copies. Checking
				// format is complicated because EFB copy formats don't exactly match
				// texture formats. I'm not sure what effect checking width/height/levels
				// would have.
				if (!isPaletteTexture || !g_Config.backend_info.bSupportsPaletteConversion)
					return ReturnEntry(stage, entry);

				// Note that we found an unconverted EFB copy, then continue.  We'll
				// perform the conversion later.  Currently, we only convert EFB copies to
				// palette textures; we could do other conversions if it proved to be
				// beneficial.
				unconverted_copy = iter;
			}
			else
			{
				// Aggressively prune EFB copies: if it isn't useful here, it will probably
				// never be useful again.  It's theoretically possible for a game to do
				// something weird where the copy could become useful in the future, but in
				// practice it doesn't happen.
				iter = FreeTexture(iter);
				continue;
			}
		}
		else
		{
			// For normal textures, all texture parameters need to match
			if (entry->hash == full_hash && entry->format == full_format && entry->native_levels >= tex_levels &&
				entry->native_width == nativeW && entry->native_height == nativeH)
			{
				entry = DoPartialTextureUpdates(iter);

				return ReturnEntry(stage, entry);
			}
		}

		// Find the texture which hasn't been used for the longest time. Count paletted
		// textures as the same texture here, when the texture itself is the same. This
		// improves the performance a lot in some games that use paletted textures.
		// Example: Sonic the Fighters (inside Sonic Gems Collection)
		// Skip EFB copies here, so they can be used for partial texture updates
		if (entry->frameCount != FRAMECOUNT_INVALID && entry->frameCount < temp_frameCount &&
			!entry->IsEfbCopy() && !(isPaletteTexture && entry->base_hash == base_hash))
		{
			temp_frameCount = entry->frameCount;
			oldest_entry = iter;
		}
		++iter;
	}

	if (unconverted_copy != textures_by_address.end())
	{
		// Perform palette decoding.
		TCacheEntryBase *entry = unconverted_copy->second;

		TCacheEntryConfig config;
		config.rendertarget = true;
		config.width = entry->config.width;
		config.height = entry->config.height;
		config.layers = FramebufferManagerBase::GetEFBLayers();
		TCacheEntryBase *decoded_entry = AllocateTexture(config);

		if (decoded_entry)
		{
			decoded_entry->SetGeneralParameters(address, texture_size, full_format);
			decoded_entry->SetDimensions(entry->native_width, entry->native_height, 1);
			decoded_entry->SetHashes(base_hash, full_hash);
			decoded_entry->frameCount = FRAMECOUNT_INVALID;
			decoded_entry->is_efb_copy = false;

			g_texture_cache->ConvertTexture(decoded_entry, entry, &texMem[tlutaddr], (TlutFormat)tlutfmt);
			textures_by_address.emplace((u64)address, decoded_entry);
			return ReturnEntry(stage, decoded_entry);
		}
	}

	// Search the texture cache for normal textures by hash
	//
	// If the texture was fully hashed, the address does not need to match. Identical duplicate textures cause unnecessary slowdowns
	// Example: Tales of Symphonia (GC) uses over 500 small textures in menus, but only around 70 different ones
	if (g_ActiveConfig.iSafeTextureCache_ColorSamples == 0 ||
		std::max(texture_size, palette_size) <= (u32)g_ActiveConfig.iSafeTextureCache_ColorSamples * 8)
	{
		iter_range = textures_by_hash.equal_range(full_hash);
		iter = iter_range.first;
		while (iter != iter_range.second)
		{
			TCacheEntryBase* entry = iter->second;
			// All parameters, except the address, need to match here
			if (entry->format == full_format && entry->native_levels >= tex_levels &&
				entry->native_width == nativeW && entry->native_height == nativeH)
			{
				entry = DoPartialTextureUpdates(iter);

				return ReturnEntry(stage, entry);
			}
			++iter;
		}
	}

	// If at least one entry was not used for the same frame, overwrite the oldest one
	if (temp_frameCount != 0x7fffffff)
	{
		// pool this texture and make a new one later
		FreeTexture(oldest_entry);
	}

	std::shared_ptr<HiresTexture> hires_tex;
	if (g_ActiveConfig.bHiresTextures)
	{
		hires_tex = HiresTexture::Search(
			src_data, texture_size,
			&texMem[tlutaddr], palette_size,
			width, height,
			texformat, use_mipmaps
		);

		if (hires_tex)
		{
			const auto& level = hires_tex->m_levels[0];
			if (level.width != width || level.height != height)
			{
				width = level.width;
				height = level.height;
			}
			expandedWidth = level .width;
			expandedHeight = level.height;
			CheckTempSize(level.data_size);
			memcpy(temp, level.data.get(), level.data_size);
		}
	}

	// how many levels the allocated texture shall have
	const u32 texLevels = hires_tex ? (u32)hires_tex->m_levels.size() : tex_levels;

	// create the entry/texture
	TCacheEntryConfig config;
	config.width = width;
	config.height = height;
	config.levels = texLevels;

	TCacheEntryBase* entry = AllocateTexture(config);
	GFX_DEBUGGER_PAUSE_AT(NEXT_NEW_TEXTURE, true);

	if (!entry)
		return nullptr;

	if (!hires_tex)
	{
		if (!(texformat == GX_TF_RGBA8 && from_tmem))
		{
			const u8* tlut = &texMem[tlutaddr];
			TexDecoder_Decode(temp, src_data, expandedWidth, expandedHeight, texformat, tlut, (TlutFormat)tlutfmt);
		}
		else
		{
			u8* src_data_gb = &texMem[bpmem.tex[stage / 4].texImage2[stage % 4].tmem_odd * TMEM_LINE_SIZE];
			TexDecoder_DecodeRGBA8FromTmem(temp, src_data, src_data_gb, expandedWidth, expandedHeight);
		}
	}

	iter = textures_by_address.emplace((u64)address, entry);
	if (g_ActiveConfig.iSafeTextureCache_ColorSamples == 0 ||
		std::max(texture_size, palette_size) <= (u32)g_ActiveConfig.iSafeTextureCache_ColorSamples * 8)
	{
		entry->textures_by_hash_iter = textures_by_hash.emplace(full_hash, entry);
	}

	entry->SetGeneralParameters(address, texture_size, full_format);
	entry->SetDimensions(nativeW, nativeH, tex_levels);
	entry->SetHashes(base_hash, full_hash);
	entry->is_efb_copy = false;
	entry->is_custom_tex = hires_tex != nullptr;

	// load texture
	entry->Load(width, height, expandedWidth, 0);

	std::string basename = "";
	if (g_ActiveConfig.bDumpTextures && !hires_tex)
	{
		basename = HiresTexture::GenBaseName(
			src_data, texture_size,
			&texMem[tlutaddr], palette_size,
			width, height,
			texformat, use_mipmaps,
			true
		);
		DumpTexture(entry, basename, 0);
	}

	if (hires_tex)
	{
		for (u32 level_index = 1; level_index != texLevels; ++level_index)
		{
			const auto& level = hires_tex->m_levels[level_index];
			CheckTempSize(level.data_size);
			memcpy(temp, level.data.get(), level.data_size);
			entry->Load(level.width, level.height, level.width, level_index);
		}
	}
	else
	{
		// load mips - TODO: Loading mipmaps from tmem is untested!
		src_data += texture_size;

		const u8* ptr_even = nullptr;
		const u8* ptr_odd = nullptr;
		if (from_tmem)
		{
			ptr_even = &texMem[bpmem.tex[stage / 4].texImage1[stage % 4].tmem_even * TMEM_LINE_SIZE + texture_size];
			ptr_odd = &texMem[bpmem.tex[stage / 4].texImage2[stage % 4].tmem_odd * TMEM_LINE_SIZE];
		}

		for (u32 level = 1; level != texLevels; ++level)
		{
			const u32 mip_width = CalculateLevelSize(width, level);
			const u32 mip_height = CalculateLevelSize(height, level);
			const u32 expanded_mip_width = ROUND_UP(mip_width, bsw);
			const u32 expanded_mip_height = ROUND_UP(mip_height, bsh);

			const u8*& mip_src_data = from_tmem
				? ((level % 2) ? ptr_odd : ptr_even)
				: src_data;
			const u8* tlut = &texMem[tlutaddr];
			TexDecoder_Decode(temp, mip_src_data, expanded_mip_width, expanded_mip_height, texformat, tlut, (TlutFormat)tlutfmt);
			mip_src_data += TexDecoder_GetTextureSizeInBytes(expanded_mip_width, expanded_mip_height, texformat);

			entry->Load(mip_width, mip_height, expanded_mip_width, level);

			if (g_ActiveConfig.bDumpTextures)
				DumpTexture(entry, basename, level);
		}
	}

	INCSTAT(stats.numTexturesUploaded);
	SETSTAT(stats.numTexturesAlive, textures_by_address.size());

	entry = DoPartialTextureUpdates(iter);

	return ReturnEntry(stage, entry);
}

void TextureCacheBase::CopyRenderTargetToTexture(u32 dstAddr, unsigned int dstFormat, u32 dstStride, PEControl::PixelFormat srcFormat,
                                                 const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf)
{
	// Emulation methods:
	//
	// - EFB to RAM:
	//      Encodes the requested EFB data at its native resolution to the emulated RAM using shaders.
	//      Load() decodes the data from there again (using TextureDecoder) if the EFB copy is being used as a texture again.
	//      Advantage: CPU can read data from the EFB copy and we don't lose any important updates to the texture
	//      Disadvantage: Encoding+decoding steps often are redundant because only some games read or modify EFB copies before using them as textures.
	//
	// - EFB to texture:
	//      Copies the requested EFB data to a texture object in VRAM, performing any color conversion using shaders.
	//      Advantage: Works for many games, since in most cases EFB copies aren't read or modified at all before being used as a texture again.
	//                 Since we don't do any further encoding or decoding here, this method is much faster.
	//                 It also allows enhancing the visual quality by doing scaled EFB copies.
	//
	// - Hybrid EFB copies:
	//      1a) Whenever this function gets called, encode the requested EFB data to RAM (like EFB to RAM)
	//      1b) Set type to TCET_EC_DYNAMIC for all texture cache entries in the destination address range.
	//          If EFB copy caching is enabled, further checks will (try to) prevent redundant EFB copies.
	//      2) Check if a texture cache entry for the specified dstAddr already exists (i.e. if an EFB copy was triggered to that address before):
	//      2a) Entry doesn't exist:
	//          - Also copy the requested EFB data to a texture object in VRAM (like EFB to texture)
	//          - Create a texture cache entry for the target (type = TCET_EC_VRAM)
	//          - Store a hash of the encoded RAM data in the texcache entry.
	//      2b) Entry exists AND type is TCET_EC_VRAM:
	//          - Like case 2a, but reuse the old texcache entry instead of creating a new one.
	//      2c) Entry exists AND type is TCET_EC_DYNAMIC:
	//          - Only encode the texture to RAM (like EFB to RAM) and store a hash of the encoded data in the existing texcache entry.
	//          - Do NOT copy the requested EFB data to a VRAM object. Reason: the texture is dynamic, i.e. the CPU is modifying it. Storing a VRAM copy is useless, because we'd always end up deleting it and reloading the data from RAM anyway.
	//      3) If the EFB copy gets used as a texture, compare the source RAM hash with the hash you stored when encoding the EFB data to RAM.
	//      3a) If the two hashes match AND type is TCET_EC_VRAM, reuse the VRAM copy you created
	//      3b) If the two hashes differ AND type is TCET_EC_VRAM, screw your existing VRAM copy. Set type to TCET_EC_DYNAMIC.
	//          Redecode the source RAM data to a VRAM object. The entry basically behaves like a normal texture now.
	//      3c) If type is TCET_EC_DYNAMIC, treat the EFB copy like a normal texture.
	//      Advantage: Non-dynamic EFB copies can be visually enhanced like with EFB to texture.
	//                 Compatibility is as good as EFB to RAM.
	//      Disadvantage: Slower than EFB to texture and often even slower than EFB to RAM.
	//                    EFB copy cache depends on accurate texture hashing being enabled. However, with accurate hashing you end up being as slow as without a copy cache anyway.
	//
	// Disadvantage of all methods: Calling this function requires the GPU to perform a pipeline flush which stalls any further CPU processing.
	//
	// For historical reasons, Dolphin doesn't actually implement "pure" EFB to RAM emulation, but only EFB to texture and hybrid EFB copies.

	float colmat[28] = { 0 };
	float *const fConstAdd = colmat + 16;
	float *const ColorMask = colmat + 20;
	ColorMask[0] = ColorMask[1] = ColorMask[2] = ColorMask[3] = 255.0f;
	ColorMask[4] = ColorMask[5] = ColorMask[6] = ColorMask[7] = 1.0f / 255.0f;
	unsigned int cbufid = -1;
	bool efbHasAlpha = bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;

	if (srcFormat == PEControl::Z24)
	{
		switch (dstFormat)
		{
		case 0: // Z4
			colmat[3] = colmat[7] = colmat[11] = colmat[15] = 1.0f;
			cbufid = 0;
			dstFormat |= _GX_TF_CTF;
			break;
		case 8: // Z8H
			dstFormat |= _GX_TF_CTF;
		case 1: // Z8
			colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1.0f;
			cbufid = 1;
			break;

		case 3: // Z16
			colmat[1] = colmat[5] = colmat[9] = colmat[12] = 1.0f;
			cbufid = 2;
			break;

		case 11: // Z16 (reverse order)
			colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1.0f;
			cbufid = 3;
			dstFormat |= _GX_TF_CTF;
			break;

		case 6: // Z24X8
			colmat[0] = colmat[5] = colmat[10] = 1.0f;
			cbufid = 4;
			break;

		case 9: // Z8M
			colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1.0f;
			cbufid = 5;
			dstFormat |= _GX_TF_CTF;
			break;

		case 10: // Z8L
			colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1.0f;
			cbufid = 6;
			dstFormat |= _GX_TF_CTF;
			break;

		case 12: // Z16L - copy lower 16 depth bits
			// expected to be used as an IA8 texture (upper 8 bits stored as intensity, lower 8 bits stored as alpha)
			// Used e.g. in Zelda: Skyward Sword
			colmat[1] = colmat[5] = colmat[9] = colmat[14] = 1.0f;
			cbufid = 7;
			dstFormat |= _GX_TF_CTF;
			break;

		default:
			ERROR_LOG(VIDEO, "Unknown copy zbuf format: 0x%x", dstFormat);
			colmat[2] = colmat[5] = colmat[8] = 1.0f;
			cbufid = 8;
			break;
		}

		dstFormat |= _GX_TF_ZTF;
	}
	else if (isIntensity)
	{
		fConstAdd[0] = fConstAdd[1] = fConstAdd[2] = 16.0f / 255.0f;
		switch (dstFormat)
		{
		case 0: // I4
		case 1: // I8
		case 2: // IA4
		case 3: // IA8
		case 8: // I8
			// TODO - verify these coefficients
			colmat[0] = 0.257f; colmat[1] = 0.504f; colmat[2] = 0.098f;
			colmat[4] = 0.257f; colmat[5] = 0.504f; colmat[6] = 0.098f;
			colmat[8] = 0.257f; colmat[9] = 0.504f; colmat[10] = 0.098f;

			if (dstFormat < 2 || dstFormat == 8)
			{
				colmat[12] = 0.257f; colmat[13] = 0.504f; colmat[14] = 0.098f;
				fConstAdd[3] = 16.0f / 255.0f;
				if (dstFormat == 0)
				{
					ColorMask[0] = ColorMask[1] = ColorMask[2] = 15.0f;
					ColorMask[4] = ColorMask[5] = ColorMask[6] = 1.0f / 15.0f;
					cbufid = 9;
				}
				else
				{
					cbufid = 10;
				}
			}
			else// alpha
			{
				colmat[15] = 1;
				if (dstFormat == 2)
				{
					ColorMask[0] = ColorMask[1] = ColorMask[2] = ColorMask[3] = 15.0f;
					ColorMask[4] = ColorMask[5] = ColorMask[6] = ColorMask[7] = 1.0f / 15.0f;
					cbufid = 11;
				}
				else
				{
					cbufid = 12;
				}

			}
			break;

		default:
			ERROR_LOG(VIDEO, "Unknown copy intensity format: 0x%x", dstFormat);
			colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1.0f;
			cbufid = 13;
			break;
		}
	}
	else
	{
		switch (dstFormat)
		{
		case 0: // R4
			colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1;
			ColorMask[0] = 15.0f;
			ColorMask[4] = 1.0f / 15.0f;
			cbufid = 14;
			dstFormat |= _GX_TF_CTF;
			break;
		case 1: // R8
		case 8: // R8
			colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1;
			cbufid = 15;
			dstFormat = GX_CTF_R8;
			break;

		case 2: // RA4
			colmat[0] = colmat[4] = colmat[8] = colmat[15] = 1.0f;
			ColorMask[0] = ColorMask[3] = 15.0f;
			ColorMask[4] = ColorMask[7] = 1.0f / 15.0f;

			cbufid = 16;
			if (!efbHasAlpha)
			{
				ColorMask[3] = 0.0f;
				fConstAdd[3] = 1.0f;
				cbufid = 17;
			}
			dstFormat |= _GX_TF_CTF;
			break;
		case 3: // RA8
			colmat[0] = colmat[4] = colmat[8] = colmat[15] = 1.0f;

			cbufid = 18;
			if (!efbHasAlpha)
			{
				ColorMask[3] = 0.0f;
				fConstAdd[3] = 1.0f;
				cbufid = 19;
			}
			dstFormat |= _GX_TF_CTF;
			break;

		case 7: // A8
			colmat[3] = colmat[7] = colmat[11] = colmat[15] = 1.0f;

			cbufid = 20;
			if (!efbHasAlpha)
			{
				ColorMask[3] = 0.0f;
				fConstAdd[0] = 1.0f;
				fConstAdd[1] = 1.0f;
				fConstAdd[2] = 1.0f;
				fConstAdd[3] = 1.0f;
				cbufid = 21;
			}
			dstFormat |= _GX_TF_CTF;
			break;

		case 9: // G8
			colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1.0f;
			cbufid = 22;
			dstFormat |= _GX_TF_CTF;
			break;
		case 10: // B8
			colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1.0f;
			cbufid = 23;
			dstFormat |= _GX_TF_CTF;
			break;

		case 11: // RG8
			colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1.0f;
			cbufid = 24;
			dstFormat |= _GX_TF_CTF;
			break;

		case 12: // GB8
			colmat[1] = colmat[5] = colmat[9] = colmat[14] = 1.0f;
			cbufid = 25;
			dstFormat |= _GX_TF_CTF;
			break;

		case 4: // RGB565
			colmat[0] = colmat[5] = colmat[10] = 1.0f;
			ColorMask[0] = ColorMask[2] = 31.0f;
			ColorMask[4] = ColorMask[6] = 1.0f / 31.0f;
			ColorMask[1] = 63.0f;
			ColorMask[5] = 1.0f / 63.0f;
			fConstAdd[3] = 1.0f; // set alpha to 1
			cbufid = 26;
			break;

		case 5: // RGB5A3
			colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1.0f;
			ColorMask[0] = ColorMask[1] = ColorMask[2] = 31.0f;
			ColorMask[4] = ColorMask[5] = ColorMask[6] = 1.0f / 31.0f;
			ColorMask[3] = 7.0f;
			ColorMask[7] = 1.0f / 7.0f;

			cbufid = 27;
			if (!efbHasAlpha)
			{
				ColorMask[3] = 0.0f;
				fConstAdd[3] = 1.0f;
				cbufid = 28;
			}
			break;
		case 6: // RGBA8
			colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1.0f;

			cbufid = 29;
			if (!efbHasAlpha)
			{
				ColorMask[3] = 0.0f;
				fConstAdd[3] = 1.0f;
				cbufid = 30;
			}
			break;

		default:
			ERROR_LOG(VIDEO, "Unknown copy color format: 0x%x", dstFormat);
			colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1.0f;
			cbufid = 31;
			break;
		}
	}

	u8* dst = Memory::GetPointer(dstAddr);
	if (dst == nullptr)
	{
		ERROR_LOG(VIDEO, "Trying to copy from EFB to invalid address 0x%8x", dstAddr);
		return;
	}

	const unsigned int tex_w = scaleByHalf ? srcRect.GetWidth() / 2 : srcRect.GetWidth();
	const unsigned int tex_h = scaleByHalf ? srcRect.GetHeight() / 2 : srcRect.GetHeight();

	unsigned int scaled_tex_w = g_ActiveConfig.bCopyEFBScaled ? Renderer::EFBToScaledX(tex_w) : tex_w;
	unsigned int scaled_tex_h = g_ActiveConfig.bCopyEFBScaled ? Renderer::EFBToScaledY(tex_h) : tex_h;

	// remove all texture cache entries at dstAddr
	{
		std::pair<TexCache::iterator, TexCache::iterator> iter_range = textures_by_address.equal_range((u64)dstAddr);
		TexCache::iterator iter = iter_range.first;
		while (iter != iter_range.second)
		{
			iter = FreeTexture(iter);
		}
	}

	// Get the base (in memory) format of this efb copy.
	int baseFormat = TexDecoder_GetEfbCopyBaseFormat(dstFormat);

	u32 blockH = TexDecoder_GetBlockHeightInTexels(baseFormat);
	const u32 blockW = TexDecoder_GetBlockWidthInTexels(baseFormat);

	// Round up source height to multiple of block size
	u32 actualHeight = ROUND_UP(tex_h, blockH);
	const u32 actualWidth = ROUND_UP(tex_w, blockW);

	u32 num_blocks_y = actualHeight / blockH;
	const u32 num_blocks_x = actualWidth / blockW;

	// RGBA takes two cache lines per block; all others take one
	const u32 bytes_per_block = baseFormat == GX_TF_RGBA8 ? 64 : 32;

	u32 bytes_per_row = num_blocks_x * bytes_per_block;

	bool copy_to_ram = !g_ActiveConfig.bSkipEFBCopyToRam;
	bool copy_to_vram = true;

	if (copy_to_ram)
	{
		g_texture_cache->CopyEFB(
			dst,
			dstFormat,
			tex_w,
			bytes_per_row,
			num_blocks_y,
			dstStride,
			srcFormat,
			srcRect,
			isIntensity,
			scaleByHalf);
	}
	else
	{
		// Hack: Most games don't actually need the correct texture data in RAM
		//       and we can just keep a copy in VRAM. We zero the memory so we
		//       can check it hasn't changed before using our copy in VRAM.
		u8* ptr = dst;
		for (u32 i = 0; i < num_blocks_y; i++)
		{
			memset(ptr, 0, bytes_per_row);
			ptr += dstStride;
		}
	}

	if (g_bRecordFifoData)
	{
		// Mark the memory behind this efb copy as dynamicly generated for the Fifo log
		u32 address = dstAddr;
		for (u32 i = 0; i < num_blocks_y; i++)
		{
			FifoRecorder::GetInstance().UseMemory(address, bytes_per_row, MemoryUpdate::TEXTURE_MAP, true);
			address += dstStride;
		}
	}

	if (dstStride < bytes_per_row)
	{
		// This kind of efb copy results in a scrambled image.
		// I'm pretty sure no game actually wants to do this, it might be caused by a
		// programming bug in the game, or a CPU/Bounding box emulation issue with dolphin.
		// The copy_to_ram code path above handles this "correctly" and scrambles the image
		// but the copy_to_vram code path just saves and uses unscrambled texture instead.

		// To avoid a "incorrect" result, we simply skip doing the copy_to_vram code path
		// so if the game does try to use the scrambled texture, dolphin will grab the scrambled
		// texture (or black if copy_to_ram is also disabled) out of ram.
		ERROR_LOG(VIDEO, "Memory stride too small (%i < %i)", dstStride, bytes_per_row);
		copy_to_vram = false;
	}

	// Invalidate all textures that overlap the range of our efb copy.
	// Unless our efb copy has a weird stride, then we want avoid invalidating textures which
	// we might be able to do a partial texture update on.
	if (dstStride == bytes_per_row || !copy_to_vram)
	{
		TexCache::iterator iter = textures_by_address.begin();
		while (iter != textures_by_address.end())
		{
			if (iter->second->addr + iter->second->size_in_bytes <= dstAddr || iter->second->addr >= dstAddr + num_blocks_y * dstStride)
				++iter;
			else
				iter = FreeTexture(iter);
		}
	}

	if (copy_to_vram)
	{
		// create the texture
		TCacheEntryConfig config;
		config.rendertarget = true;
		config.width = scaled_tex_w;
		config.height = scaled_tex_h;
		config.layers = FramebufferManagerBase::GetEFBLayers();

		TCacheEntryBase* entry = AllocateTexture(config);

		if (entry)
		{
			entry->SetGeneralParameters(dstAddr, 0, baseFormat);
			entry->SetDimensions(tex_w, tex_h, 1);

			entry->frameCount = FRAMECOUNT_INVALID;
			entry->SetEfbCopy(dstStride);
			entry->is_custom_tex = false;

			entry->FromRenderTarget(dst, srcFormat, srcRect, scaleByHalf, cbufid, colmat);

			u64 hash = entry->CalculateHash();
			entry->SetHashes(hash, hash);

			if (g_ActiveConfig.bDumpEFBTarget)
			{
				static int count = 0;
				entry->Save(StringFromFormat("%sefb_frame_%i.png", File::GetUserPath(D_DUMPTEXTURES_IDX).c_str(),
					count++), 0);
			}

			textures_by_address.emplace((u64)dstAddr, entry);
		}
	}
}

TextureCacheBase::TCacheEntryBase* TextureCacheBase::AllocateTexture(const TCacheEntryConfig& config)
{
	TexPool::iterator iter = texture_pool.find(config);
	TextureCacheBase::TCacheEntryBase* entry;
	if (iter != texture_pool.end())
	{
		entry = iter->second;
		texture_pool.erase(iter);
	}
	else
	{
		entry = g_texture_cache->CreateTexture(config);
		if (!entry)
			return nullptr;

		INCSTAT(stats.numTexturesCreated);
	}

	entry->textures_by_hash_iter = textures_by_hash.end();
	return entry;
}

TextureCacheBase::TexCache::iterator TextureCacheBase::FreeTexture(TexCache::iterator iter)
{
	TCacheEntryBase* entry = iter->second;

	if (entry->textures_by_hash_iter != textures_by_hash.end())
	{
		textures_by_hash.erase(entry->textures_by_hash_iter);
		entry->textures_by_hash_iter = textures_by_hash.end();
	}

	entry->frameCount = FRAMECOUNT_INVALID;
	texture_pool.emplace(entry->config, entry);

	return textures_by_address.erase(iter);
}

u32 TextureCacheBase::TCacheEntryBase::BytesPerRow() const
{
	const u32 blockW = TexDecoder_GetBlockWidthInTexels(format);

	// Round up source height to multiple of block size
	const u32 actualWidth = ROUND_UP(native_width, blockW);

	const u32 numBlocksX = actualWidth / blockW;

	// RGBA takes two cache lines per block; all others take one
	const u32 bytes_per_block = format == GX_TF_RGBA8 ? 64 : 32;

	return numBlocksX * bytes_per_block;
}

u32 TextureCacheBase::TCacheEntryBase::NumBlocksY() const
{
	u32 blockH = TexDecoder_GetBlockHeightInTexels(format);
	// Round up source height to multiple of block size
	u32 actualHeight = ROUND_UP(native_height, blockH);

	return actualHeight / blockH;
}

void TextureCacheBase::TCacheEntryBase::SetEfbCopy(u32 stride)
{
	is_efb_copy = true;
	memory_stride = stride;

	_assert_msg_(VIDEO, memory_stride >= BytesPerRow(), "Memory stride is too small");

	size_in_bytes = memory_stride * NumBlocksY();
}

u64 TextureCacheBase::TCacheEntryBase::CalculateHash() const
{
	u8* ptr = Memory::GetPointer(addr);
	if (memory_stride == BytesPerRow())
	{
		return GetHash64(ptr, size_in_bytes, g_ActiveConfig.iSafeTextureCache_ColorSamples);
	}
	else
	{
		u32 blocks = NumBlocksY();
		u64 temp_hash = size_in_bytes;

		u32 samples_per_row = 0;
		if (g_ActiveConfig.iSafeTextureCache_ColorSamples != 0)
		{
			// Hash at least 4 samples per row to avoid hashing in a bad pattern, like just on the left side of the efb copy
			samples_per_row = std::max(g_ActiveConfig.iSafeTextureCache_ColorSamples / blocks, 4u);
		}

		for (u32 i = 0; i < blocks; i++)
		{
			// Multiply by a prime number to mix the hash up a bit. This prevents identical blocks from canceling each other out
			temp_hash = (temp_hash * 397) ^ GetHash64(ptr, BytesPerRow(), samples_per_row);
			ptr += memory_stride;
		}
		return temp_hash;
	}
}
