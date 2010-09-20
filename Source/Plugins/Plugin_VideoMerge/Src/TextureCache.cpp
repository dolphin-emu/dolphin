
// Common
#include "MemoryUtil.h"

#include "TextureCache.h"

#include "VideoConfig.h"
#include "TextureDecoder.h"
#include "HiresTextures.h"

#include "Statistics.h"

#include "Main.h"

TextureCacheBase::TexCache TextureCacheBase::textures;
u8 *TextureCacheBase::temp;

enum
{
	TEMP_SIZE = (1024 * 1024 * 4),
	TEXTURE_KILL_THRESHOLD = 200,
};

// returns the exponent of the smallest power of two which is greater than val
unsigned int GetPow2(unsigned int val)
{
	unsigned int ret = 0;
	for (; val; val >>= 1)
		++ret;
	return ret;
}

TextureCacheBase::TCacheEntryBase::~TCacheEntryBase()
{
	// TODO: can we just use (addr) and remove the other checks?
	// will need changes to TextureCache::Load and CopyRenderTargetToTexture
	if (addr && false == (isRenderTarget || g_ActiveConfig.bSafeTextureCache))
	{
		u32* ptr = (u32*)g_VideoInitialize.pGetMemoryPointer(addr);
		if (ptr && *ptr == hash)
			*ptr = oldpixel;
	}
}

bool TextureCacheBase::TCacheEntryBase::IntersectsMemoryRange(u32 range_address, u32 range_size) const
{
	if (addr + size_in_bytes < range_address)
		return false;

	if (addr >= range_address + range_size)
		return false;

	return true;
}

TextureCacheBase::TextureCacheBase()
{
	temp = (u8*)AllocateMemoryPages(TEMP_SIZE);
	TexDecoder_SetTexFmtOverlayOptions(g_ActiveConfig.bTexFmtOverlayEnable, g_ActiveConfig.bTexFmtOverlayCenter);
	HiresTextures::Init(g_globals->unique_id);
}

void TextureCacheBase::Cleanup()
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

TextureCacheBase::~TextureCacheBase()
{
	Invalidate(true);
	FreeMemoryPages(temp, TEMP_SIZE);
	temp = NULL;
}

void TextureCacheBase::ClearRenderTargets()
{
	TexCache::iterator
		iter = textures.begin(),
		tcend = textures.end();
	for (; iter!=tcend; ++iter)
        iter->second->isRenderTarget = false;
}

void TextureCacheBase::MakeRangeDynamic(u32 start_address, u32 size)
{
	TexCache::iterator
		iter = textures.begin(),
		tcend = textures.end();
	for (; iter!=tcend; ++iter)
	{
		// TODO: an int ??
		int rangePosition = iter->second->IntersectsMemoryRange(start_address, size);
		if (0 == rangePosition)
			iter->second->hash = 0;
	}
}

void TextureCacheBase::Invalidate(bool shutdown)
{
	TexCache::iterator
		iter = textures.begin(),
		tcend = textures.end();
	for (; iter!=tcend; ++iter)
	{
		// TODO: this could be better
		if (shutdown)
			iter->second->addr = 0; // hax, not even helpin
		delete iter->second;
	}

	textures.clear();
	HiresTextures::Shutdown();
}

void TextureCacheBase::InvalidateRange(u32 start_address, u32 size)
{
	TexCache::iterator
		iter = textures.begin(),
		tcend = textures.end();
	while (iter != tcend)
	{
		if (iter->second->IntersectsMemoryRange(start_address, size))
		{
			delete iter->second;
			textures.erase(iter++);
		}
		else
			++iter;
	}
}

TextureCacheBase::TCacheEntryBase* TextureCacheBase::Load(unsigned int stage,
	u32 address, unsigned int width, unsigned int height, unsigned int tex_format,
	unsigned int tlutaddr, unsigned int tlutfmt, bool UseNativeMips, unsigned int maxlevel)
{
	// necessary?
	if (0 == address)
		return NULL;

	u8* ptr = g_VideoInitialize.pGetMemoryPointer(address);

	// TexelSizeInNibbles(format)*width*height/16;
	const unsigned int bsw = TexDecoder_GetBlockWidthInTexels(tex_format) - 1;
	const unsigned int bsh = TexDecoder_GetBlockHeightInTexels(tex_format) - 1;

	unsigned int expandedWidth  = (width  + bsw) & (~bsw);
	unsigned int expandedHeight = (height + bsh) & (~bsh);

	u64 hash_value = 0;
	u64 texHash = 0;
	u32 texID = address;
	u32 FullFormat = tex_format;
	u32 size_in_bytes = TexDecoder_GetTextureSizeInBytes(expandedWidth, expandedHeight, tex_format);

	switch (tex_format)
	{
	case GX_TF_C4:
	case GX_TF_C8:
	case GX_TF_C14X2:
		FullFormat = tex_format | (tlutfmt << 16);
		break;

	default:
		break;
	}

	// hires textures and texture dumping not supported, yet
	if (g_ActiveConfig.bSafeTextureCache/* || g_ActiveConfig.bHiresTextures || g_ActiveConfig.bDumpTextures*/)
	{
		texHash = GetHash64(ptr, size_in_bytes, g_ActiveConfig.iSafeTextureCache_ColorSamples);
		
		switch (tex_format)
		{
		case GX_TF_C4:
		case GX_TF_C8:
		case GX_TF_C14X2:
			{
			// WARNING! texID != address now => may break CopyRenderTargetToTexture (cf. TODO up)
			// tlut size can be up to 32768B (GX_TF_C14X2) but Safer == Slower.
			// This trick (to change the texID depending on the TLUT addr) is a trick to get around
			// an issue with metroid prime's fonts, where it has multiple sets of fonts on top of
			// each other stored in a single texture, and uses the palette to make different characters
			// visible or invisible. Thus, unless we want to recreate the textures for every drawn character,
			// we must make sure that texture with different tluts get different IDs.
			const u64 tlutHash = GetHash64(texMem + tlutaddr, TexDecoder_GetPaletteSize(tex_format),
				g_ActiveConfig.iSafeTextureCache_ColorSamples);

			texHash ^= tlutHash;

			if (g_ActiveConfig.bSafeTextureCache)
				texID ^= ((u32)tlutHash) ^ (tlutHash >> 32);
			}
			break;

		default:
			break;
		}

		if (g_ActiveConfig.bSafeTextureCache)
			hash_value = texHash;
	}

	bool skip_texture_create = false;

	TCacheEntryBase *entry = textures[texID];
	if (entry)
	{
		if (!g_ActiveConfig.bSafeTextureCache)
			hash_value = *(u32*)ptr;

		// TODO: Is the (entry->MipLevels == maxlevel) check needed?
		if (entry->isRenderTarget ||
			(address == entry->addr && hash_value == entry->hash &&
			FullFormat == entry->fmt && entry->MipLevels == maxlevel))
		{
			goto return_entry;
		}
		else
		{
			// Let's reload the new texture data into the same texture,
			// instead of destroying it and having to create a new one.
			// Might speed up movie playback very, very slightly.

			// TODO: Is the (entry->MipLevels < maxlevel) check needed?
			if (width == entry->w && height==entry->h &&
				FullFormat == entry->fmt && entry->MipLevels < maxlevel)
			{
				goto load_texture;
			}
			else
			{
				delete entry;
			}
		}
	}

	// create the texture

	const bool isPow2 = !((width & (width - 1)) || (height & (height - 1)));
	unsigned int TexLevels = (isPow2 && UseNativeMips && maxlevel) ? GetPow2(std::max(width, height)) : !isPow2;

	// TODO: what is ((maxlevel + 1) && maxlevel) ?
	if (TexLevels > (maxlevel + 1) && maxlevel)
		TexLevels = maxlevel + 1;

	const PC_TexFormat pcfmt = TexDecoder_Decode(temp, ptr, expandedWidth,
		expandedHeight, tex_format, tlutaddr, tlutfmt, true);

	textures[texID] = entry = CreateTexture(width, height, expandedWidth, TexLevels, pcfmt);
	
	entry->oldpixel = *(u32*)ptr;
	entry->addr = address;
	entry->w = width;
	entry->h = height;
	entry->fmt = FullFormat;
	entry->MipLevels = maxlevel;
	entry->size_in_bytes = size_in_bytes;
	
	entry->isRenderTarget = false;
	entry->isNonPow2 = false;

	if (g_ActiveConfig.bSafeTextureCache)
		entry->hash = hash_value;
	else
		// WTF is this rand() doing here?
		entry->hash = *(u32*)ptr = (u32)(((double)rand() / RAND_MAX) * 0xFFFFFFFF);

load_texture:

	entry->Load(width, height, expandedWidth, 0);

	if (TexLevels > 1 && pcfmt != PC_TEX_FMT_NONE)
	{
		const unsigned int bsdepth = TexDecoder_GetTexelSizeInNibbles(tex_format);

		unsigned int level = 1;
		unsigned int mipWidth = (width + 1) >> 1;
		unsigned int mipHeight = (height + 1) >> 1;
		ptr += entry->size_in_bytes;

		while ((mipHeight || mipWidth) && (level < TexLevels))
		{
			const unsigned int currentWidth = (mipWidth > 0) ? mipWidth : 1;
			const unsigned int currentHeight = (mipHeight > 0) ? mipHeight : 1;

			expandedWidth  = (currentWidth + bsw)  & (~bsw);
			expandedHeight = (currentHeight + bsh) & (~bsh);

			TexDecoder_Decode(temp, ptr, expandedWidth, expandedHeight, tex_format, tlutaddr, tlutfmt, true);
			//entry->Load(currentWidth, currentHeight, expandedWidth, level);

			ptr += ((max(mipWidth, bsw) * max(mipHeight, bsh) * bsdepth) >> 1);
			mipWidth >>= 1;
			mipHeight >>= 1;
			++level;
		}
	}

	INCSTAT(stats.numTexturesCreated);
	SETSTAT(stats.numTexturesAlive, (int)textures.size());

return_entry:

	entry->frameCount = frameCount;
	entry->Bind(stage);

	return entry;
}

void TextureCacheBase::CopyRenderTargetToTexture(u32 address, bool bFromZBuffer,
	bool bIsIntensityFmt, u32 copyfmt, bool bScaleByHalf, const EFBRectangle &source_rect)
{
	float colmat[20] = {};
	// last four floats for fConstAdd
	float *const fConstAdd = colmat + 16;
	unsigned int cbufid = -1;

	// TODO: Move this to TextureCache::Init()
	if (bFromZBuffer)
	{
		switch (copyfmt)
		{
		case 0: // Z4
		case 1: // Z8
			colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1.0f;
			cbufid = 12;
			break;

		case 3: // Z16 //?
			colmat[1] = colmat[5] = colmat[9] = colmat[12] = 1.0f;
			cbufid = 13;
			break;

		case 11: // Z16 (reverse order)
			colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1.0f;
			cbufid = 14;
			break;

		case 6: // Z24X8
			colmat[0] = colmat[5] = colmat[10] = 1.0f;
			cbufid = 15;
			break;

		case 9: // Z8M
			colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1.0f;
			cbufid = 16;
			break;

		case 10: // Z8L
			colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1.0f;
			cbufid = 17;
			break;

		case 12: // Z16L
			colmat[2] = colmat[6] = colmat[10] = colmat[13] = 1.0f;
			cbufid = 18;
			break;

		default:
			ERROR_LOG(VIDEO, "Unknown copy zbuf format: 0x%x", copyfmt);
			colmat[2] = colmat[5] = colmat[8] = 1.0f;
			cbufid = 19;
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
			// TODO - verify these coefficients
			colmat[0] = 0.257f; colmat[1] = 0.504f; colmat[2] = 0.098f;
			colmat[4] = 0.257f; colmat[5] = 0.504f; colmat[6] = 0.098f;
			colmat[8] = 0.257f; colmat[9] = 0.504f; colmat[10] = 0.098f;

			if (copyfmt < 2) 
			{
				fConstAdd[3] = 16.0f / 255.0f;
				colmat[12] = 0.257f; colmat[13] = 0.504f; colmat[14] = 0.098f;
				cbufid = 0;
			}
			else// alpha
			{
				colmat[15] = 1;
				cbufid = 1;
			}

			break;

		default:
			ERROR_LOG(VIDEO, "Unknown copy intensity format: 0x%x", copyfmt);
			colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
			break;
		}
	}
	else
	{
		switch (copyfmt) 
		{
		case 0: // R4
		case 8: // R8
			colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1;
			cbufid = 2;
			break;

		case 2: // RA4
		case 3: // RA8
			colmat[0] = colmat[4] = colmat[8] = colmat[15] = 1;
			cbufid = 3;
			break;

		case 7: // A8
			colmat[3] = colmat[7] = colmat[11] = colmat[15] = 1; 
			cbufid = 4;
			break;

		case 9: // G8
			colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1;
			cbufid = 5;
			break;

		case 10: // B8
			colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1;
			cbufid = 6;
			break;

		case 11: // RG8
			colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1;
			cbufid = 7;
			break;

		case 12: // GB8
			colmat[1] = colmat[5] = colmat[9] = colmat[14] = 1;
			cbufid = 8;
			break;

		case 4: // RGB565
			colmat[0] = colmat[5] = colmat[10] = 1;
			fConstAdd[3] = 1; // set alpha to 1
			cbufid = 9;
			break;

		case 5: // RGB5A3
		case 6: // RGBA8
			colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
			cbufid = 10;
			break;

		default:
			ERROR_LOG(VIDEO, "Unknown copy color format: 0x%x", copyfmt);
			colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
			cbufid = 11;
			break;
		}
	}

	const int tex_w = (abs(source_rect.GetWidth()) >> bScaleByHalf);
	const int tex_h = (abs(source_rect.GetHeight()) >> bScaleByHalf);

	const int scaled_tex_w = g_ActiveConfig.bCopyEFBScaled ? (int)(tex_w * g_renderer->GetTargetScaleX()) : tex_w;
	const int scaled_tex_h = g_ActiveConfig.bCopyEFBScaled ? (int)(tex_h * g_renderer->GetTargetScaleY()) : tex_h;

	TCacheEntryBase* entry = NULL;

	const TexCache::iterator iter = textures.find(address);
	if (textures.end() != iter)
	{
		entry = iter->second;

		if (entry->isRenderTarget && entry->Scaledw == scaled_tex_w && entry->Scaledh == scaled_tex_h)
		{
			goto load_texture;
		}
		else
		{
			// remove it and recreate it as a render target
			delete entry;
			textures.erase(iter);
		}
	}

	// create the texture
	textures[address] = entry = CreateRenderTargetTexture(scaled_tex_w, scaled_tex_h);

	if (NULL == entry)
		PanicAlert("CopyRenderTargetToTexture failed to create entry.texture at %s %d\n", __FILE__, __LINE__);

	entry->addr = 0;	// TODO: probably can use this and eliminate isRenderTarget
	entry->hash = 0;
	entry->w = tex_w;
	entry->h = tex_h;
	entry->Scaledw = scaled_tex_w;
	entry->Scaledh = scaled_tex_h;
	entry->fmt = copyfmt;

	entry->isRenderTarget = true;
	entry->isNonPow2 = true;	// TODO: is this used anywhere?

load_texture:

	entry->frameCount = frameCount;

	g_renderer->ResetAPIState(); // reset any game specific settings

	// load the texture
	entry->FromRenderTarget(bFromZBuffer, bScaleByHalf, cbufid, colmat, source_rect);

	g_renderer->RestoreAPIState();
}
