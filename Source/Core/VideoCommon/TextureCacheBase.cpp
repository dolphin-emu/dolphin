// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <string>

#include "Common/FileUtil.h"
#include "Common/MemoryUtil.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/HiresTextures.h"
#include "VideoCommon/RenderBase.h"
#include "VideoCommon/Statistics.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VideoConfig.h"

enum
{
	TEXTURE_KILL_THRESHOLD = 200,
	RENDER_TARGET_KILL_THRESHOLD = 3,
};

TextureCache *g_texture_cache;

GC_ALIGNED16(u8 *TextureCache::temp) = nullptr;
unsigned int TextureCache::temp_size;

TextureCache::TexCache TextureCache::textures;
TextureCache::RenderTargetPool TextureCache::render_target_pool;

TextureCache::BackupConfig TextureCache::backup_config;

static bool invalidate_texture_cache_requested;

TextureCache::TCacheEntryBase::~TCacheEntryBase()
{
}

TextureCache::TextureCache()
{
	temp_size = 2048 * 2048 * 4;
	if (!temp)
		temp = (u8*)AllocateAlignedMemory(temp_size, 16);

	TexDecoder_SetTexFmtOverlayOptions(g_ActiveConfig.bTexFmtOverlayEnable, g_ActiveConfig.bTexFmtOverlayCenter);

	if (g_ActiveConfig.bHiresTextures && !g_ActiveConfig.bDumpTextures)
		HiresTextures::Init(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID);

	SetHash64Function(g_ActiveConfig.bHiresTextures || g_ActiveConfig.bDumpTextures);

	invalidate_texture_cache_requested = false;
}

void TextureCache::RequestInvalidateTextureCache()
{
	invalidate_texture_cache_requested = true;
}

void TextureCache::Invalidate()
{
	for (auto& tex : textures)
	{
		delete tex.second;
	}
	textures.clear();

	for (auto& rt : render_target_pool)
	{
		delete rt;
	}
	render_target_pool.clear();
}

TextureCache::~TextureCache()
{
	Invalidate();
	FreeAlignedMemory(temp);
	temp = nullptr;
}

void TextureCache::OnConfigChanged(VideoConfig& config)
{
	if (g_texture_cache)
	{
		// TODO: Invalidating texcache is really stupid in some of these cases
		if (config.iSafeTextureCache_ColorSamples != backup_config.s_colorsamples ||
			config.bTexFmtOverlayEnable != backup_config.s_texfmt_overlay ||
			config.bTexFmtOverlayCenter != backup_config.s_texfmt_overlay_center ||
			config.bHiresTextures != backup_config.s_hires_textures ||
			invalidate_texture_cache_requested)
		{
			g_texture_cache->Invalidate();

			if (g_ActiveConfig.bHiresTextures)
				HiresTextures::Init(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID);

			SetHash64Function(g_ActiveConfig.bHiresTextures || g_ActiveConfig.bDumpTextures);
			TexDecoder_SetTexFmtOverlayOptions(g_ActiveConfig.bTexFmtOverlayEnable, g_ActiveConfig.bTexFmtOverlayCenter);

			invalidate_texture_cache_requested = false;
		}

		// TODO: Probably shouldn't clear all render targets here, just mark them dirty or something.
		if (config.bEFBCopyCacheEnable != backup_config.s_copy_cache_enable || // TODO: not sure if this is needed?
			config.bCopyEFBToTexture != backup_config.s_copy_efb_to_texture ||
			config.bCopyEFBScaled != backup_config.s_copy_efb_scaled ||
			config.bEFBCopyEnable != backup_config.s_copy_efb ||
			config.iEFBScale != backup_config.s_efb_scale)
		{
			g_texture_cache->ClearRenderTargets();
		}
	}

	backup_config.s_colorsamples = config.iSafeTextureCache_ColorSamples;
	backup_config.s_copy_efb_to_texture = config.bCopyEFBToTexture;
	backup_config.s_copy_efb_scaled = config.bCopyEFBScaled;
	backup_config.s_copy_efb = config.bEFBCopyEnable;
	backup_config.s_efb_scale = config.iEFBScale;
	backup_config.s_texfmt_overlay = config.bTexFmtOverlayEnable;
	backup_config.s_texfmt_overlay_center = config.bTexFmtOverlayCenter;
	backup_config.s_hires_textures = config.bHiresTextures;
	backup_config.s_copy_cache_enable = config.bEFBCopyCacheEnable;
}

void TextureCache::Cleanup()
{
	TexCache::iterator iter = textures.begin();
	TexCache::iterator tcend = textures.end();
	while (iter != tcend)
	{
		if (frameCount > TEXTURE_KILL_THRESHOLD + iter->second->frameCount &&
            // EFB copies living on the host GPU are unrecoverable and thus shouldn't be deleted
		    !iter->second->IsEfbCopy())
		{
			delete iter->second;
			textures.erase(iter++);
		}
		else
		{
			++iter;
		}
	}

	for (size_t i = 0; i < render_target_pool.size();)
	{
		auto rt = render_target_pool[i];

		if (frameCount > RENDER_TARGET_KILL_THRESHOLD + rt->frameCount)
		{
			delete rt;
			render_target_pool[i] = render_target_pool.back();
			render_target_pool.pop_back();
		}
		else
		{
			++i;
		}
	}
}

void TextureCache::InvalidateRange(u32 start_address, u32 size)
{
	TexCache::iterator
		iter = textures.begin(),
		tcend = textures.end();
	while (iter != tcend)
	{
		const int rangePosition = iter->second->IntersectsMemoryRange(start_address, size);
		if (0 == rangePosition)
		{
			delete iter->second;
			textures.erase(iter++);
		}
		else
		{
			++iter;
		}
	}
}

void TextureCache::MakeRangeDynamic(u32 start_address, u32 size)
{
	TexCache::iterator
		iter = textures.lower_bound(start_address),
		tcend = textures.upper_bound(start_address + size);

	if (iter != textures.begin())
		--iter;

	for (; iter != tcend; ++iter)
	{
		const int rangePosition = iter->second->IntersectsMemoryRange(start_address, size);
		if (0 == rangePosition)
		{
			iter->second->SetHashes(TEXHASH_INVALID);
		}
	}
}

bool TextureCache::Find(u32 start_address, u64 hash)
{
	TexCache::iterator iter = textures.lower_bound(start_address);

	if (iter->second->hash == hash)
		return true;

	return false;
}

int TextureCache::TCacheEntryBase::IntersectsMemoryRange(u32 range_address, u32 range_size) const
{
	if (addr + size_in_bytes < range_address)
		return -1;

	if (addr >= range_address + range_size)
		return 1;

	return 0;
}

void TextureCache::ClearRenderTargets()
{
	TexCache::iterator
		iter = textures.begin(),
		tcend = textures.end();

	while (iter != tcend)
	{
		if (iter->second->type == TCET_EC_VRAM)
		{
			delete iter->second;
			textures.erase(iter++);
		}
		else
		{
			++iter;
		}
	}
}

bool TextureCache::CheckForCustomTextureLODs(u64 tex_hash, int texformat, unsigned int levels)
{
	if (levels == 1)
		return false;

	// Just checking if the necessary files exist, if they can't be loaded or have incorrect dimensions LODs will be black
	std::string texBasePathTemp = StringFromFormat("%s_%08x_%i", SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str(), (u32) (tex_hash & 0x00000000FFFFFFFFLL), texformat);

	for (unsigned int level = 1; level < levels; ++level)
	{
		std::string texPathTemp = StringFromFormat("%s_mip%u", texBasePathTemp.c_str(), level);
		if (!HiresTextures::HiresTexExists(texPathTemp))
		{
			if (level > 1)
				WARN_LOG(VIDEO, "Couldn't find custom texture LOD with index %u (filename: %s), disabling custom LODs for this texture", level, texPathTemp.c_str());

			return false;
		}
	}

	return true;
}

PC_TexFormat TextureCache::LoadCustomTexture(u64 tex_hash, int texformat, unsigned int level, unsigned int* widthp, unsigned int* heightp)
{
	std::string texPathTemp;
	unsigned int newWidth = 0;
	unsigned int newHeight = 0;
	u32 tex_hash_u32 = tex_hash & 0x00000000FFFFFFFFLL;

	if (level == 0)
		texPathTemp = StringFromFormat("%s_%08x_%i", SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str(), tex_hash_u32, texformat);
	else
		texPathTemp = StringFromFormat("%s_%08x_%i_mip%u", SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str(), tex_hash_u32, texformat, level);

	unsigned int required_size = 0;
	PC_TexFormat ret = HiresTextures::GetHiresTex(texPathTemp, &newWidth, &newHeight, &required_size, texformat, temp_size, temp);
	if (ret == PC_TEX_FMT_NONE && temp_size < required_size)
	{
		// Allocate more memory and try again
		// TODO: Should probably check if newWidth and newHeight are texture dimensions which are actually supported by the current video backend
		temp_size = required_size;
		FreeAlignedMemory(temp);
		temp = (u8*)AllocateAlignedMemory(temp_size, 16);
		ret = HiresTextures::GetHiresTex(texPathTemp, &newWidth, &newHeight, &required_size, texformat, temp_size, temp);
	}

	if (ret != PC_TEX_FMT_NONE)
	{
		unsigned int width = *widthp, height = *heightp;
		if (level > 0 && (newWidth != width || newHeight != height))
			ERROR_LOG(VIDEO, "Invalid custom texture size %dx%d for texture %s. This mipmap layer _must_ be %dx%d.", newWidth, newHeight, texPathTemp.c_str(), width, height);
		if (newWidth * height != newHeight * width)
			ERROR_LOG(VIDEO, "Invalid custom texture size %dx%d for texture %s. The aspect differs from the native size %dx%d.", newWidth, newHeight, texPathTemp.c_str(), width, height);
		if (newWidth % width || newHeight % height)
			WARN_LOG(VIDEO, "Invalid custom texture size %dx%d for texture %s. Please use an integer upscaling factor based on the native size %dx%d.", newWidth, newHeight, texPathTemp.c_str(), width, height);

		*widthp = newWidth;
		*heightp = newHeight;
	}
	return ret;
}

void TextureCache::DumpTexture(TCacheEntryBase* entry, unsigned int level)
{
	std::string filename;
	std::string szDir = File::GetUserPath(D_DUMPTEXTURES_IDX) +
		SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID;

	// make sure that the directory exists
	if (!File::Exists(szDir) || !File::IsDirectory(szDir))
		File::CreateDir(szDir);

	// For compatibility with old texture packs, don't print the LOD index for level 0.
	 // TODO: TLUT format should actually be stored in filename? :/
	if (level == 0)
	{
		filename = StringFromFormat("%s/%s_%08x_%i.png", szDir.c_str(),
			SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str(),
			(u32)(entry->hash & 0x00000000FFFFFFFFLL), entry->format & 0xFFFF);
	}
	else
	{
		filename = StringFromFormat("%s/%s_%08x_%i_mip%i.png", szDir.c_str(),
				SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str(),
				(u32) (entry->hash & 0x00000000FFFFFFFFLL), entry->format & 0xFFFF, level);
	}

	if (!File::Exists(filename))
		entry->Save(filename, level);
}

static u32 CalculateLevelSize(u32 level_0_size, u32 level)
{
	return (level_0_size + ((1 << level) - 1)) >> level;
}

// Used by TextureCache::Load
static TextureCache::TCacheEntryBase* ReturnEntry(unsigned int stage, TextureCache::TCacheEntryBase* entry)
{
	entry->frameCount = frameCount;
	entry->Bind(stage);

	GFX_DEBUGGER_PAUSE_AT(NEXT_TEXTURE_CHANGE, true);

	return entry;
}

TextureCache::TCacheEntryBase* TextureCache::Load(unsigned int const stage,
	u32 const address, unsigned int width, unsigned int height, int const texformat,
	unsigned int const tlutaddr, int const tlutfmt, bool const use_mipmaps, unsigned int maxlevel, bool const from_tmem)
{
	if (0 == address)
		return nullptr;

	// TexelSizeInNibbles(format) * width * height / 16;
	const unsigned int bsw = TexDecoder_GetBlockWidthInTexels(texformat) - 1;
	const unsigned int bsh = TexDecoder_GetBlockHeightInTexels(texformat) - 1;

	unsigned int expandedWidth  = (width  + bsw) & (~bsw);
	unsigned int expandedHeight = (height + bsh) & (~bsh);
	const unsigned int nativeW = width;
	const unsigned int nativeH = height;

	u32 texID = address;
	// Hash assigned to texcache entry (also used to generate filenames used for texture dumping and custom texture lookup)
	u64 tex_hash = TEXHASH_INVALID;
	u64 tlut_hash = TEXHASH_INVALID;

	u32 full_format = texformat;
	PC_TexFormat pcfmt = PC_TEX_FMT_NONE;

	const bool isPaletteTexture = (texformat == GX_TF_C4 || texformat == GX_TF_C8 || texformat == GX_TF_C14X2);
	if (isPaletteTexture)
		full_format = texformat | (tlutfmt << 16);

	const u32 texture_size = TexDecoder_GetTextureSizeInBytes(expandedWidth, expandedHeight, texformat);

	const u8* src_data;
	if (from_tmem)
		src_data = &texMem[bpmem.tex[stage / 4].texImage1[stage % 4].tmem_even * TMEM_LINE_SIZE];
	else
		src_data = Memory::GetPointer(address);

	// TODO: This doesn't hash GB tiles for preloaded RGBA8 textures (instead, it's hashing more data from the low tmem bank than it should)
	tex_hash = GetHash64(src_data, texture_size, g_ActiveConfig.iSafeTextureCache_ColorSamples);
	if (isPaletteTexture)
	{
		const u32 palette_size = TexDecoder_GetPaletteSize(texformat);
		tlut_hash = GetHash64(&texMem[tlutaddr], palette_size, g_ActiveConfig.iSafeTextureCache_ColorSamples);

		// NOTE: For non-paletted textures, texID is equal to the texture address.
		//       A paletted texture, however, may have multiple texIDs assigned though depending on the currently used tlut.
		//       This (changing texID depending on the tlut_hash) is a trick to get around
		//       an issue with Metroid Prime's fonts (it has multiple sets of fonts on each other
		//       stored in a single texture and uses the palette to make different characters
		//       visible or invisible. Thus, unless we want to recreate the textures for every drawn character,
		//       we must make sure that a paletted texture gets assigned multiple IDs for each tlut used.
		//
		// TODO: Because texID isn't always the same as the address now, CopyRenderTargetToTexture might be broken now
		texID ^= ((u32)tlut_hash) ^(u32)(tlut_hash >> 32);
		tex_hash ^= tlut_hash;
	}

	// D3D doesn't like when the specified mipmap count would require more than one 1x1-sized LOD in the mipmap chain
	// e.g. 64x64 with 7 LODs would have the mipmap chain 64x64,32x32,16x16,8x8,4x4,2x2,1x1,1x1, so we limit the mipmap count to 6 there
	while (g_ActiveConfig.backend_info.bUseMinimalMipCount && std::max(width, height) >> maxlevel == 0)
		--maxlevel;

	TCacheEntryBase *entry = textures[texID];
	if (entry)
	{
		// 1. Calculate reference hash:
		// calculated from RAM texture data for normal textures. Hashes for paletted textures are modified by tlut_hash. 0 for virtual EFB copies.
		if (g_ActiveConfig.bCopyEFBToTexture && entry->IsEfbCopy())
			tex_hash = TEXHASH_INVALID;

		// 2. a) For EFB copies, only the hash and the texture address need to match
		if (entry->IsEfbCopy() && tex_hash == entry->hash && address == entry->addr)
		{
			entry->type = TCET_EC_VRAM;

			// TODO: Print a warning if the format changes! In this case,
			// we could reinterpret the internal texture object data to the new pixel format
			// (similar to what is already being done in Renderer::ReinterpretPixelFormat())
			return ReturnEntry(stage, entry);
		}

		// 2. b) For normal textures, all texture parameters need to match
		if (address == entry->addr && tex_hash == entry->hash && full_format == entry->format &&
			entry->num_mipmaps > maxlevel && entry->native_width == nativeW && entry->native_height == nativeH)
		{
			return ReturnEntry(stage, entry);
		}

		// 3. If we reach this line, we'll have to upload the new texture data to VRAM.
		//    If we're lucky, the texture parameters didn't change and we can reuse the internal texture object instead of destroying and recreating it.
		//
		// TODO: Don't we need to force texture decoding to RGBA8 for dynamic EFB copies?
		// TODO: Actually, it should be enough if the internal texture format matches...
		if ((entry->type == TCET_NORMAL &&
		     width == entry->virtual_width &&
		     height == entry->virtual_height &&
		     full_format == entry->format &&
		     entry->num_mipmaps > maxlevel) ||
		    (entry->type == TCET_EC_DYNAMIC &&
		     entry->native_width == width &&
		     entry->native_height == height))
		{
			// reuse the texture
		}
		else
		{
			// delete the texture and make a new one
			delete entry;
			entry = nullptr;
		}
	}

	bool using_custom_texture = false;

	if (g_ActiveConfig.bHiresTextures)
	{
		pcfmt = LoadCustomTexture(tex_hash, texformat, 0, &width, &height);
		if (pcfmt != PC_TEX_FMT_NONE)
		{
			if (expandedWidth != width || expandedHeight != height)
			{
				expandedWidth = width;
				expandedHeight = height;

				// If we thought we could reuse the texture before, make sure to pool it now!
				if (entry)
				{
					delete entry;
					entry = nullptr;
				}
			}
			using_custom_texture = true;
		}
	}

	if (!using_custom_texture)
	{
		if (!(texformat == GX_TF_RGBA8 && from_tmem))
		{
			const u8* tlut = &texMem[tlutaddr];
			pcfmt = TexDecoder_Decode(temp, src_data, expandedWidth, expandedHeight, texformat, tlut, (TlutFormat) tlutfmt);
		}
		else
		{
			u8* src_data_gb = &texMem[bpmem.tex[stage/4].texImage2[stage%4].tmem_odd * TMEM_LINE_SIZE];
			pcfmt = TexDecoder_DecodeRGBA8FromTmem(temp, src_data, src_data_gb, expandedWidth, expandedHeight);
		}
	}

	u32 texLevels = use_mipmaps ? (maxlevel + 1) : 1;
	const bool using_custom_lods = using_custom_texture && CheckForCustomTextureLODs(tex_hash, texformat, texLevels);
	// Only load native mips if their dimensions fit to our virtual texture dimensions
	const bool use_native_mips = use_mipmaps && !using_custom_lods && (width == nativeW && height == nativeH);
	texLevels = (use_native_mips || using_custom_lods) ? texLevels : 1; // TODO: Should be forced to 1 for non-pow2 textures (e.g. efb copies with automatically adjusted IR)

	// create the entry/texture
	if (nullptr == entry)
	{
		textures[texID] = entry = g_texture_cache->CreateTexture(width, height, expandedWidth, texLevels, pcfmt);

		// Sometimes, we can get around recreating a texture if only the number of mip levels changes
		// e.g. if our texture cache entry got too many mipmap levels we can limit the number of used levels by setting the appropriate render states
		// Thus, we don't update this member for every Load, but just whenever the texture gets recreated

		// TODO: This is the wrong value. We should be storing the number of levels our actual texture has.
		// But that will currently make the above "existing entry" tests fail as "texLevels" is not calculated until after.
		// Currently, we might try to reuse a texture which appears to have more levels than actual, maybe..
		entry->num_mipmaps = maxlevel + 1;
		entry->type = TCET_NORMAL;

		GFX_DEBUGGER_PAUSE_AT(NEXT_NEW_TEXTURE, true);
	}
	else
	{
		// load texture (CreateTexture also loads level 0)
		entry->Load(width, height, expandedWidth, 0);
	}

	entry->SetGeneralParameters(address, texture_size, full_format, entry->num_mipmaps);
	entry->SetDimensions(nativeW, nativeH, width, height);
	entry->hash = tex_hash;

	if (entry->IsEfbCopy() && !g_ActiveConfig.bCopyEFBToTexture)
		entry->type = TCET_EC_DYNAMIC;
	else
		entry->type = TCET_NORMAL;

	if (g_ActiveConfig.bDumpTextures && !using_custom_texture)
		DumpTexture(entry, 0);

	u32 level = 1;
	// load mips - TODO: Loading mipmaps from tmem is untested!
	if (pcfmt != PC_TEX_FMT_NONE)
	{
		if (use_native_mips)
		{
			src_data += texture_size;

			const u8* ptr_even = nullptr;
			const u8* ptr_odd = nullptr;
			if (from_tmem)
			{
				ptr_even = &texMem[bpmem.tex[stage/4].texImage1[stage%4].tmem_even * TMEM_LINE_SIZE + texture_size];
				ptr_odd = &texMem[bpmem.tex[stage/4].texImage2[stage%4].tmem_odd * TMEM_LINE_SIZE];
			}

			for (; level != texLevels; ++level)
			{
				const u32 mip_width = CalculateLevelSize(width, level);
				const u32 mip_height = CalculateLevelSize(height, level);
				const u32 expanded_mip_width = (mip_width + bsw) & (~bsw);
				const u32 expanded_mip_height = (mip_height + bsh) & (~bsh);

				const u8*& mip_src_data = from_tmem
					? ((level % 2) ? ptr_odd : ptr_even)
					: src_data;
				const u8* tlut = &texMem[tlutaddr];
				TexDecoder_Decode(temp, mip_src_data, expanded_mip_width, expanded_mip_height, texformat, tlut, (TlutFormat) tlutfmt);
				mip_src_data += TexDecoder_GetTextureSizeInBytes(expanded_mip_width, expanded_mip_height, texformat);

				entry->Load(mip_width, mip_height, expanded_mip_width, level);

				if (g_ActiveConfig.bDumpTextures)
					DumpTexture(entry, level);
			}
		}
		else if (using_custom_lods)
		{
			for (; level != texLevels; ++level)
			{
				unsigned int mip_width = CalculateLevelSize(width, level);
				unsigned int mip_height = CalculateLevelSize(height, level);

				LoadCustomTexture(tex_hash, texformat, level, &mip_width, &mip_height);
				entry->Load(mip_width, mip_height, mip_width, level);
			}
		}
	}

	INCSTAT(stats.numTexturesCreated);
	SETSTAT(stats.numTexturesAlive, textures.size());

	return ReturnEntry(stage, entry);
}

void TextureCache::CopyRenderTargetToTexture(u32 dstAddr, unsigned int dstFormat, PEControl::PixelFormat srcFormat,
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

	float colmat[28] = {0};
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
			break;
		case 1: // Z8
		case 8: // Z8
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
			break;

		case 6: // Z24X8
			colmat[0] = colmat[5] = colmat[10] = 1.0f;
			cbufid = 4;
			break;

		case 9: // Z8M
			colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1.0f;
			cbufid = 5;
			break;

		case 10: // Z8L
			colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1.0f;
			cbufid = 6;
			break;

		case 12: // Z16L - copy lower 16 depth bits
			// expected to be used as an IA8 texture (upper 8 bits stored as intensity, lower 8 bits stored as alpha)
			// Used e.g. in Zelda: Skyward Sword
			colmat[1] = colmat[5] = colmat[9] = colmat[14] = 1.0f;
			cbufid = 7;
			break;

		default:
			ERROR_LOG(VIDEO, "Unknown copy zbuf format: 0x%x", dstFormat);
			colmat[2] = colmat[5] = colmat[8] = 1.0f;
			cbufid = 8;
			break;
		}
	}
	else if (isIntensity)
	{
		fConstAdd[0] = fConstAdd[1] = fConstAdd[2] = 16.0f/255.0f;
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
				fConstAdd[3] = 16.0f/255.0f;
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
			break;
		case 1: // R8
		case 8: // R8
			colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1;
			cbufid = 15;
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
			break;

		case 9: // G8
			colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1.0f;
			cbufid = 22;
			break;
		case 10: // B8
			colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1.0f;
			cbufid = 23;
			break;

		case 11: // RG8
			colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1.0f;
			cbufid = 24;
			break;

		case 12: // GB8
			colmat[1] = colmat[5] = colmat[9] = colmat[14] = 1.0f;
			cbufid = 25;
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

	const unsigned int tex_w = scaleByHalf ? srcRect.GetWidth()/2 : srcRect.GetWidth();
	const unsigned int tex_h = scaleByHalf ? srcRect.GetHeight()/2 : srcRect.GetHeight();

	unsigned int scaled_tex_w = g_ActiveConfig.bCopyEFBScaled ? Renderer::EFBToScaledX(tex_w) : tex_w;
	unsigned int scaled_tex_h = g_ActiveConfig.bCopyEFBScaled ? Renderer::EFBToScaledY(tex_h) : tex_h;


	TCacheEntryBase *entry = textures[dstAddr];
	if (entry)
	{
		if (entry->type == TCET_EC_DYNAMIC && entry->native_width == tex_w && entry->native_height == tex_h)
		{
			scaled_tex_w = tex_w;
			scaled_tex_h = tex_h;
		}
		else if (!(entry->type == TCET_EC_VRAM && entry->virtual_width == scaled_tex_w && entry->virtual_height == scaled_tex_h))
		{
			if (entry->type == TCET_EC_VRAM)
			{
				// try to re-use this render target later
				FreeRenderTarget(entry);
			}
			else
			{
				// remove it and recreate it as a render target
				delete entry;
			}

			entry = nullptr;
		}
	}

	if (nullptr == entry)
	{
		// create the texture
		textures[dstAddr] = entry = AllocateRenderTarget(scaled_tex_w, scaled_tex_h);

		// TODO: Using the wrong dstFormat, dumb...
		entry->SetGeneralParameters(dstAddr, 0, dstFormat, 1);
		entry->SetDimensions(tex_w, tex_h, scaled_tex_w, scaled_tex_h);
		entry->SetHashes(TEXHASH_INVALID);
		entry->type = TCET_EC_VRAM;
	}

	entry->frameCount = frameCount;

	entry->FromRenderTarget(dstAddr, dstFormat, srcFormat, srcRect, isIntensity, scaleByHalf, cbufid, colmat);
}

TextureCache::TCacheEntryBase* TextureCache::AllocateRenderTarget(unsigned int width, unsigned int height)
{
	for (size_t i = 0; i < render_target_pool.size(); ++i)
	{
		auto rt = render_target_pool[i];

		if (rt->virtual_width != width || rt->virtual_height != height)
			continue;

		render_target_pool[i] = render_target_pool.back();
		render_target_pool.pop_back();

		return rt;
	}

	return g_texture_cache->CreateRenderTargetTexture(width, height);
}

void TextureCache::FreeRenderTarget(TCacheEntryBase* entry)
{
	render_target_pool.push_back(entry);
}
