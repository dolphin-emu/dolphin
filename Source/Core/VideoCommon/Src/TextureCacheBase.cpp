
#include "MemoryUtil.h"

#include "VideoConfig.h"
#include "Statistics.h"
#include "HiresTextures.h"
#include "RenderBase.h"
#include "FileUtil.h"

#include "TextureCacheBase.h"
#include "Debugger.h"
#include "ConfigManager.h"
#include "HW/Memmap.h"

// ugly
extern int frameCount;

enum
{
	TEMP_SIZE = (2048 * 2048 * 4),
	TEXTURE_KILL_THRESHOLD = 200,
};

TextureCache *g_texture_cache;

u8 *TextureCache::temp = NULL;

TextureCache::TexCache TextureCache::textures;
bool TextureCache::DeferredInvalidate;

TextureCache::TCacheEntryBase::~TCacheEntryBase()
{
	if (0 == addr)
		return;

	if (!isRenderTarget && !g_ActiveConfig.bSafeTextureCache)
	{
		u32 *const ptr = (u32*)Memory::GetPointer(addr);
		if (ptr && *ptr == hash)
			*ptr = oldpixel;
	}
}

TextureCache::TextureCache()
{
	if (!temp)
		temp = (u8*)AllocateMemoryPages(TEMP_SIZE);
	TexDecoder_SetTexFmtOverlayOptions(g_ActiveConfig.bTexFmtOverlayEnable, g_ActiveConfig.bTexFmtOverlayCenter);
    if(g_ActiveConfig.bHiresTextures && !g_ActiveConfig.bDumpTextures)
		HiresTextures::Init(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str());
	SetHash64Function(g_ActiveConfig.bHiresTextures || g_ActiveConfig.bDumpTextures);
}

void TextureCache::Invalidate(bool shutdown)
{
	TexCache::iterator
		iter = textures.begin(),
		tcend = textures.end();
	for (; iter != tcend; ++iter)
	{
		if (shutdown)
			iter->second->addr = 0;
		delete iter->second;
	}

	textures.clear();
	if(g_ActiveConfig.bHiresTextures && !g_ActiveConfig.bDumpTextures)
		HiresTextures::Init(SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str());
	SetHash64Function(g_ActiveConfig.bHiresTextures || g_ActiveConfig.bDumpTextures);
	
	DeferredInvalidate = false;
}

void TextureCache::InvalidateDefer()
{
	DeferredInvalidate = true;
}

TextureCache::~TextureCache()
{
	Invalidate(true);
	if (temp)
	{
		FreeMemoryPages(temp, TEMP_SIZE);
		temp = NULL;
	}
}

void TextureCache::Cleanup()
{
	TexCache::iterator
		iter = textures.begin(),
		tcend = textures.end();
	while (iter != tcend)
	{
		if (frameCount > TEXTURE_KILL_THRESHOLD + iter->second->frameCount)
		{
			delete iter->second;
			textures.erase(iter++);
		}
		else
			++iter;
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
			++iter;
	}
}

void TextureCache::MakeRangeDynamic(u32 start_address, u32 size)
{
	TexCache::iterator
		iter = textures.lower_bound(start_address),
		tcend = textures.upper_bound(start_address + size);

	if (iter != textures.begin())
		iter--;

	for (; iter != tcend; ++iter)
	{
		const int rangePosition = iter->second->IntersectsMemoryRange(start_address, size);
		if (0 == rangePosition)
		{
			iter->second->hash = 0;
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
	for (; iter!=tcend; ++iter)
		iter->second->isRenderTarget = false;
}

TextureCache::TCacheEntryBase* TextureCache::Load(unsigned int stage,
	u32 address, unsigned int width, unsigned int height, int texformat,
	unsigned int tlutaddr, int tlutfmt, bool UseNativeMips, unsigned int maxlevel)
{
	// necessary?
	if (0 == address)
		return NULL;

	u8* ptr = Memory::GetPointer(address);

	// TexelSizeInNibbles(format)*width*height/16;
	const unsigned int bsw = TexDecoder_GetBlockWidthInTexels(texformat) - 1;
	const unsigned int bsh = TexDecoder_GetBlockHeightInTexels(texformat) - 1;

	unsigned int expandedWidth  = (width  + bsw) & (~bsw);
	unsigned int expandedHeight = (height + bsh) & (~bsh);
	const unsigned int nativeW = width;
	const unsigned int nativeH = height;
	bool isPow2;

	u64 hash_value = 0;
	u64 texHash = 0;
	u32 texID = address;
	u32 full_format = texformat;
	const u32 texture_size = TexDecoder_GetTextureSizeInBytes(expandedWidth, expandedHeight, texformat);
	const u32 palette_size = TexDecoder_GetPaletteSize(texformat);
	bool texture_is_dynamic = false;
	unsigned int texLevels;
	PC_TexFormat pcfmt = PC_TEX_FMT_NONE;

	const bool isPaletteTexture = (texformat == GX_TF_C4 || texformat == GX_TF_C8 || texformat == GX_TF_C14X2);

	if (isPaletteTexture)
		full_format = texformat | (tlutfmt << 16);

	// hires texture loading and texture dumping require accurate hashes
	if (g_ActiveConfig.bSafeTextureCache || g_ActiveConfig.bHiresTextures || g_ActiveConfig.bDumpTextures)
	{
		texHash = GetHash64(ptr, texture_size, g_ActiveConfig.iSafeTextureCache_ColorSamples);

		if (isPaletteTexture)
		{
			// WARNING! texID != address now => may break CopyRenderTargetToTexture (cf. TODO up)
			// tlut size can be up to 32768B (GX_TF_C14X2) but Safer == Slower.
			// This trick (to change the texID depending on the TLUT addr) is a trick to get around
			// an issue with metroid prime's fonts, where it has multiple sets of fonts on top of
			// each other stored in a single texture, and uses the palette to make different characters
			// visible or invisible. Thus, unless we want to recreate the textures for every drawn character,
			// we must make sure that texture with different tluts get different IDs.

			const u64 tlutHash = GetHash64(texMem + tlutaddr, palette_size,
				g_ActiveConfig.iSafeTextureCache_ColorSamples);

			texHash ^= tlutHash;

			if (g_ActiveConfig.bSafeTextureCache)
				texID ^= ((u32)tlutHash) ^ (u32)(tlutHash >> 32);
		}

		if (g_ActiveConfig.bSafeTextureCache)
			hash_value = texHash;
	}

	TCacheEntryBase *entry = textures[texID];
	if (entry)
	{
		if (!g_ActiveConfig.bSafeTextureCache)
		{
			if (entry->isRenderTarget || entry->isDynamic)
			{
				if (!g_ActiveConfig.bCopyEFBToTexture)
				{
					hash_value = GetHash64(ptr, texture_size, g_ActiveConfig.iSafeTextureCache_ColorSamples);

					if (isPaletteTexture)
					{
						hash_value ^= GetHash64(&texMem[tlutaddr], palette_size,
							g_ActiveConfig.iSafeTextureCache_ColorSamples);
					}
				}
				else
				{
					hash_value = 0;
				}
			}
			else
			{
				hash_value = *(u32*)ptr;
			}
		}
		else if ((entry->isRenderTarget || entry->isDynamic) && g_ActiveConfig.bCopyEFBToTexture)
		{
			hash_value = 0;
		}

		if (((entry->isRenderTarget || entry->isDynamic) && hash_value == entry->hash && address == entry->addr) 
			|| ((address == entry->addr) && (hash_value == entry->hash) && full_format == entry->format && entry->mipLevels == maxlevel))
		{
			entry->isDynamic = false;
			goto return_entry;
		}
		else
		{
			// Let's reload the new texture data into the same texture,
			// instead of destroying it and having to create a new one.
			// Might speed up movie playback very, very slightly.
			texture_is_dynamic = (entry->isRenderTarget || entry->isDynamic) && !g_ActiveConfig.bCopyEFBToTexture;

			if (!entry->isRenderTarget &&
				((!entry->isDynamic && width == entry->realW && height == entry->realH && full_format == entry->format && entry->mipLevels == maxlevel)
				|| (entry->isDynamic && entry->realW == width && entry->realH == height)))
			{
				// reuse the texture
			}
			else
			{
				// delete the texture and make a new one
				delete entry;
				entry = NULL;
			}
		}
	}
	
	if (g_ActiveConfig.bHiresTextures)
	{
		// Load Custom textures
		char texPathTemp[MAX_PATH];

		unsigned int newWidth = width;
		unsigned int newHeight = height;

		sprintf(texPathTemp, "%s_%08x_%i", SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str(), (u32) (texHash & 0x00000000FFFFFFFFLL), texformat);
		pcfmt = HiresTextures::GetHiresTex(texPathTemp, &newWidth, &newHeight, texformat, temp);

		if (pcfmt != PC_TEX_FMT_NONE)
		{
			expandedWidth = width = newWidth;
			expandedHeight = height = newHeight;

			// TODO: shouldn't generating mips be forced on now?
			// native mips with a custom texture wouldn't make sense
		}
	}

	if (pcfmt == PC_TEX_FMT_NONE)
		pcfmt = TexDecoder_Decode(temp, ptr, expandedWidth,
			expandedHeight, texformat, tlutaddr, tlutfmt, g_ActiveConfig.backend_info.bUseRGBATextures);

	isPow2 = !((width & (width - 1)) || (height & (height - 1)));
	texLevels = (isPow2 && UseNativeMips && maxlevel) ?
		GetPow2(std::max(width, height)) : !isPow2;

	if ((texLevels > (maxlevel + 1)) && maxlevel)
		texLevels = maxlevel + 1;

	// create the entry/texture
	if (NULL == entry) {
		textures[texID] = entry = g_texture_cache->CreateTexture(width, height, expandedWidth, texLevels, pcfmt);

		// Sometimes, we can get around recreating a texture if only the number of mip levels gets changes
		// e.g. if our texture cache entry got too many mipmap levels we can limit the number of used levels by setting the appropriate render states
		// Thus, we don't update this member for every Load, but just whenever the texture gets recreated
		entry->mipLevels = maxlevel;

		GFX_DEBUGGER_PAUSE_AT(NEXT_NEW_TEXTURE, true);
	}

	entry->addr = address;
	entry->format = full_format;
	entry->size_in_bytes = texture_size;

	entry->virtualW = width;
	entry->virtualH = height;

	entry->realW = nativeW;
	entry->realH = nativeH;
		
	entry->isRenderTarget = false;
	entry->isNonPow2 = false;
	entry->isDynamic = texture_is_dynamic;

	entry->oldpixel = *(u32*)ptr;

	if (g_ActiveConfig.bSafeTextureCache || entry->isDynamic)
		entry->hash = hash_value;
	else
		// don't like rand() here
		entry->hash = *(u32*)ptr = (u32)(((double)rand() / RAND_MAX) * 0xFFFFFFFF);

	// load texture
	entry->Load(width, height, expandedWidth, 0, (texLevels == 0));

	// load mips
	if (texLevels > 1 && pcfmt != PC_TEX_FMT_NONE)
	{
		const unsigned int bsdepth = TexDecoder_GetTexelSizeInNibbles(texformat);

		unsigned int level = 1;
		unsigned int mipWidth = (width + 1) >> 1;
		unsigned int mipHeight = (height + 1) >> 1;
		ptr += texture_size;

		while ((mipHeight || mipWidth) && (level < texLevels))
		{
			const unsigned int currentWidth = (mipWidth > 0) ? mipWidth : 1;
			const unsigned int currentHeight = (mipHeight > 0) ? mipHeight : 1;

			expandedWidth  = (currentWidth + bsw)  & (~bsw);
			expandedHeight = (currentHeight + bsh) & (~bsh);

			TexDecoder_Decode(temp, ptr, expandedWidth, expandedHeight, texformat, tlutaddr, tlutfmt, g_ActiveConfig.backend_info.bUseRGBATextures);
			entry->Load(currentWidth, currentHeight, expandedWidth, level);

			ptr += ((std::max(mipWidth, bsw) * std::max(mipHeight, bsh) * bsdepth) >> 1);
			mipWidth >>= 1;
			mipHeight >>= 1;
			++level;
		}
	}

	// TODO: won't this cause loaded hires textures to be dumped as well?
	// dump texture to file
	if (g_ActiveConfig.bDumpTextures)
	{
		char szTemp[MAX_PATH];
		char szDir[MAX_PATH];

		// make sure that the directory exists
		sprintf(szDir, "%s%s", File::GetUserPath(D_DUMPTEXTURES_IDX), SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str());
		if (false == File::Exists(szDir) || false == File::IsDirectory(szDir))
			File::CreateDir(szDir);

		sprintf(szTemp, "%s/%s_%08x_%i.png", szDir, SConfig::GetInstance().m_LocalCoreStartupParameter.m_strUniqueID.c_str(), (u32) (texHash & 0x00000000FFFFFFFFLL), texformat);

		if (false == File::Exists(szTemp))
			entry->Save(szTemp);
	}

	INCSTAT(stats.numTexturesCreated);
	SETSTAT(stats.numTexturesAlive, textures.size());

return_entry:

	entry->frameCount = frameCount;
	entry->Bind(stage);

	GFX_DEBUGGER_PAUSE_AT(NEXT_TEXTURE_CHANGE, true);

	return entry;
}

void TextureCache::CopyRenderTargetToTexture(u32 address, bool bFromZBuffer,
	bool bIsIntensityFmt, u32 copyfmt, bool bScaleByHalf, const EFBRectangle &source_rect)
{
	float colmat[28] = {0};
	float *const fConstAdd = colmat + 16;
	float *const ColorMask = colmat + 20;
	ColorMask[0] = ColorMask[1] = ColorMask[2] = ColorMask[3] = 255.0f; 
	ColorMask[4] = ColorMask[5] = ColorMask[6] = ColorMask[7] = 1.0f / 255.0f;
	unsigned int cbufid = -1;

	if (bFromZBuffer)
	{
		switch (copyfmt)
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
			cbufid = 24;
			break;

		case 11: // Z16 (reverse order)
			colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1.0f;
			cbufid = 2;
			break;

		case 6: // Z24X8
			colmat[0] = colmat[5] = colmat[10] = 1.0f;
			cbufid = 3;
			break;

		case 9: // Z8M
			colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1.0f;
			cbufid = 4;
			break;

		case 10: // Z8L
			colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1.0f;
			cbufid = 5;
			break;

		case 12: // Z16L
			colmat[2] = colmat[6] = colmat[10] = colmat[13] = 1.0f;
			cbufid = 6;
			break;

		default:
			ERROR_LOG(VIDEO, "Unknown copy zbuf format: 0x%x", copyfmt);
			colmat[2] = colmat[5] = colmat[8] = 1.0f;
			cbufid = 7;
			break;
		}
		
	}
	else if (bIsIntensityFmt) 
	{
		fConstAdd[0] = fConstAdd[1] = fConstAdd[2] = 16.0f/255.0f;
		switch (copyfmt) 
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

			if (copyfmt < 2 || copyfmt == 8) 
			{
				colmat[12] = 0.257f; colmat[13] = 0.504f; colmat[14] = 0.098f;
				fConstAdd[3] = 16.0f/255.0f;
				if (copyfmt == 0)
				{
					ColorMask[0] = ColorMask[1] = ColorMask[2] = 15.0f;
					ColorMask[4] = ColorMask[5] = ColorMask[6] = 1.0f / 15.0f;
					cbufid = 8;
				}
				else
				{
					cbufid = 9;	
				}				
			}
			else// alpha
			{
				colmat[15] = 1;
				if (copyfmt == 2)
				{
					ColorMask[0] = ColorMask[1] = ColorMask[2] = ColorMask[3] = 15.0f;
					ColorMask[4] = ColorMask[5] = ColorMask[6] = ColorMask[7] = 1.0f / 15.0f;
					cbufid = 10;
				}
				else
				{
					cbufid = 11;
				}
				
			}
			break;

		default:
			ERROR_LOG(VIDEO, "Unknown copy intensity format: 0x%x", copyfmt);
			colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1.0f;
			cbufid = 23;
			break;
		}
	}
	else
	{
		switch (copyfmt) 
		{
		case 0: // R4
			colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1;
			ColorMask[0] = 15.0f;
			ColorMask[4] = 1.0f / 15.0f;
			cbufid = 12;
			break;
		case 1: // R8
		case 8: // R8
			colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1;
			cbufid = 13;			
			break;

		case 2: // RA4
			colmat[0] = colmat[4] = colmat[8] = colmat[15] = 1.0f;
			ColorMask[0] = ColorMask[3] = 15.0f;
			ColorMask[4] = ColorMask[7] = 1.0f / 15.0f;
			cbufid = 14;
			break;
		case 3: // RA8
			colmat[0] = colmat[4] = colmat[8] = colmat[15] = 1.0f;
			cbufid = 15;
			break;

		case 7: // A8
			colmat[3] = colmat[7] = colmat[11] = colmat[15] = 1.0f;
			cbufid = 16;
			break;

		case 9: // G8
			colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1.0f;
			cbufid = 17;			
			break;
		case 10: // B8
			colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1.0f;
			cbufid = 18;			
			break;

		case 11: // RG8
			colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1.0f;
			cbufid = 19;
			break;

		case 12: // GB8
			colmat[1] = colmat[5] = colmat[9] = colmat[14] = 1.0f;			
			cbufid = 20;
			break;

		case 4: // RGB565
			colmat[0] = colmat[5] = colmat[10] = 1.0f;
			ColorMask[0] = ColorMask[2] = 31.0f;
			ColorMask[4] = ColorMask[6] = 1.0f / 31.0f;
			ColorMask[1] = 63.0f;
			ColorMask[5] = 1.0f / 63.0f;
			fConstAdd[3] = 1.0f; // set alpha to 1
			cbufid = 21;
			break;

		case 5: // RGB5A3
			colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1.0f;
			ColorMask[0] = ColorMask[1] = ColorMask[2] = 31.0f;
			ColorMask[4] = ColorMask[5] = ColorMask[6] = 1.0f / 31.0f;
			ColorMask[3] = 7.0f;
			ColorMask[7] = 1.0f / 7.0f;
			cbufid = 22;
			break;
		case 6: // RGBA8
			colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1.0f;
			cbufid = 23;
			break;

		default:
			ERROR_LOG(VIDEO, "Unknown copy color format: 0x%x", copyfmt);
			colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1.0f;
			cbufid = 23;
			break;
		}
	}

	const unsigned int tex_w = (abs(source_rect.GetWidth()) >> (int)bScaleByHalf);
	const unsigned int tex_h = (abs(source_rect.GetHeight()) >> (int)bScaleByHalf);

	unsigned int scaled_tex_w = g_ActiveConfig.bCopyEFBScaled ? Renderer::EFBToScaledX(tex_w) : tex_w;
	unsigned int scaled_tex_h = g_ActiveConfig.bCopyEFBScaled ? Renderer::EFBToScaledY(tex_h) : tex_h;

	bool texture_is_dynamic = false;

	TCacheEntryBase *entry = textures[address];
	if (entry)
	{
		if ((entry->isRenderTarget && entry->virtualW == scaled_tex_w && entry->virtualH == scaled_tex_h) 
			|| (entry->isDynamic && entry->realW == tex_w && entry->realH == tex_h))
		{
			texture_is_dynamic = entry->isDynamic;
		}
		else
		{
			// remove it and recreate it as a render target
			delete entry;
			entry = NULL;
		}
	}

	if (texture_is_dynamic)
	{
		scaled_tex_w = tex_w;
		scaled_tex_h = tex_h;
	}

	if (NULL == entry)
	{
		// create the texture
		textures[address] = entry = g_texture_cache->CreateRenderTargetTexture(scaled_tex_w, scaled_tex_h);

		entry->addr = address;
		entry->hash = 0;

		entry->realW = tex_w;
		entry->realH = tex_h;

		entry->virtualW = scaled_tex_w;
		entry->virtualH = scaled_tex_h;

		entry->format = copyfmt;
		entry->mipLevels = 0;

		entry->isRenderTarget = true;
		entry->isNonPow2 = true;
		entry->isDynamic = false;
	}

	entry->frameCount = frameCount;

	g_renderer->ResetAPIState(); // reset any game specific settings

	entry->FromRenderTarget(bFromZBuffer, bScaleByHalf, cbufid, colmat, source_rect, bIsIntensityFmt, copyfmt);

	g_renderer->RestoreAPIState();
}
