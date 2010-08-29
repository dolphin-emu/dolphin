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

#include <vector>
#include <cmath>

#include "Globals.h"
#include "CommonPaths.h"
#include "StringUtil.h"
#include <fstream>
#ifdef _WIN32
#define _interlockedbittestandset workaround_ms_header_bug_platform_sdk6_set
#define _interlockedbittestandreset workaround_ms_header_bug_platform_sdk6_reset
#define _interlockedbittestandset64 workaround_ms_header_bug_platform_sdk6_set64
#define _interlockedbittestandreset64 workaround_ms_header_bug_platform_sdk6_reset64
#include <intrin.h>
#undef _interlockedbittestandset
#undef _interlockedbittestandreset
#undef _interlockedbittestandset64
#undef _interlockedbittestandreset64
#endif

#include "VideoConfig.h"
#include "Hash.h"
#include "Statistics.h"
#include "Profiler.h"
#include "ImageWrite.h"

#include "Render.h"

#include "MemoryUtil.h"
#include "BPStructs.h"
#include "TextureDecoder.h"
#include "TextureMngr.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "FramebufferManager.h"
#include "FileUtil.h"
#include "HiresTextures.h"
#include "TextureConverter.h"

u8 *TextureMngr::temp = NULL;
TextureMngr::TexCache TextureMngr::textures;

extern int frameCount;
static u32 s_TempFramebuffer = 0;

#define TEMP_SIZE (1024*1024*4)
#define TEXTURE_KILL_THRESHOLD 200

static const GLint c_MinLinearFilter[8] = {
	GL_NEAREST,
	GL_NEAREST_MIPMAP_NEAREST,
	GL_NEAREST_MIPMAP_LINEAR,
	GL_NEAREST,
	GL_LINEAR,
	GL_LINEAR_MIPMAP_NEAREST,
	GL_LINEAR_MIPMAP_LINEAR,
	GL_LINEAR,
};

static const GLint c_WrapSettings[4] = {
	GL_CLAMP_TO_EDGE,
	GL_REPEAT,
	GL_MIRRORED_REPEAT,
	GL_REPEAT,
};

bool SaveTexture(const char* filename, u32 textarget, u32 tex, int width, int height)
{
	std::vector<u32> data(width * height);
    glBindTexture(textarget, tex);
    glGetTexImage(textarget, 0, GL_BGRA, GL_UNSIGNED_BYTE, &data[0]);
    GLenum err = GL_REPORT_ERROR();
    if (err != GL_NO_ERROR)
	{
		PanicAlert("Can't save texture, GL Error: %s", gluErrorString(err));
        return false;
    }

    return SaveTGA(filename, width, height, &data[0]);
}

int TextureMngr::TCacheEntry::IntersectsMemoryRange(u32 range_address, u32 range_size)
{
	if (addr + size_in_bytes < range_address)
		return -1;
	if (addr >= range_address + range_size)
		return 1;
	return 0;
}

void TextureMngr::TCacheEntry::SetTextureParameters(TexMode0 &newmode,TexMode1 &newmode1)
{
    mode = newmode;
	mode1 = newmode1;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
		            (newmode.mag_filter || g_Config.bForceFiltering) ? GL_LINEAR : GL_NEAREST);

    if (bHaveMipMaps) 
	{
		if (g_ActiveConfig.bForceFiltering && newmode.min_filter < 4)
            mode.min_filter += 4; // take equivalent forced linear
        int filt = newmode.min_filter;            
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, c_MinLinearFilter[filt & 7]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, newmode1.min_lod >> 4);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, newmode1.max_lod >> 4);
		glTexEnvf(GL_TEXTURE_FILTER_CONTROL, GL_TEXTURE_LOD_BIAS, (newmode.lod_bias/32.0f));

    }
    else
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
		                (g_ActiveConfig.bForceFiltering || newmode.min_filter >= 4) ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, c_WrapSettings[newmode.wrap_s]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, c_WrapSettings[newmode.wrap_t]);
    
    if (g_Config.iMaxAnisotropy >= 1)
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)(1 << g_ActiveConfig.iMaxAnisotropy));
}

void TextureMngr::TCacheEntry::Destroy(bool shutdown)
{
    if (!texture)
        return;
    glDeleteTextures(1, &texture);
    if (!isRenderTarget && !shutdown && !g_ActiveConfig.bSafeTextureCache) {
        u32 *ptr = (u32*)g_VideoInitialize.pGetMemoryPointer(addr);
        if (ptr && *ptr == hash)
            *ptr = oldpixel;
    }
    texture = 0;
} 

void TextureMngr::Init()
{
    temp = (u8*)AllocateMemoryPages(TEMP_SIZE);
	TexDecoder_SetTexFmtOverlayOptions(g_ActiveConfig.bTexFmtOverlayEnable, g_ActiveConfig.bTexFmtOverlayCenter);
	HiresTextures::Init(globals->unique_id);
}

void TextureMngr::Invalidate(bool shutdown)
{
    for (TexCache::iterator iter = textures.begin(); iter != textures.end(); ++iter)
        iter->second.Destroy(shutdown);
    textures.clear();
	HiresTextures::Shutdown();
}

void TextureMngr::Shutdown()
{
    Invalidate(true);

    if (s_TempFramebuffer) {
        glDeleteFramebuffersEXT(1, (GLuint *)&s_TempFramebuffer);
        s_TempFramebuffer = 0;
    }

    FreeMemoryPages(temp, TEMP_SIZE);	
    temp = NULL;
}

void TextureMngr::ProgressiveCleanup()
{
	TexCache::iterator iter = textures.begin();
	while (iter != textures.end())
	{
		if (frameCount > TEXTURE_KILL_THRESHOLD + iter->second.frameCount)
		{
			iter->second.Destroy(false);
			textures.erase(iter++);
		}
		else
			++iter;
	}
}

void TextureMngr::InvalidateRange(u32 start_address, u32 size)
{
	TexCache::iterator iter = textures.begin();
	while (iter != textures.end())
	{
		int rangePosition = iter->second.IntersectsMemoryRange(start_address, size);
		if (rangePosition == 0)
		{
			iter->second.Destroy(false);
			textures.erase(iter++);
		}
		else 
		{
			++iter;			
		}
	}
}

void TextureMngr::MakeRangeDynamic(u32 start_address, u32 size)
{
	TexCache::iterator iter = textures.begin();
	while (iter != textures.end())
	{
		int rangePosition = iter->second.IntersectsMemoryRange(start_address, size);
		if ( rangePosition == 0)
		{
			iter->second.hash = 0;
		}
		++iter;
	}
}


TextureMngr::TCacheEntry* TextureMngr::Load(int texstage, u32 address, int width, int height, u32 tex_format, int tlutaddr, int tlutfmt)
{
	// notes (about "UNsafe texture cache"):
	//	Have to be removed soon.
	//	But we keep it until the "safe" way became rock solid
	//	pros: it has an unique ID held by the texture data itself (@address) once cached.
	//	cons: it writes this unique ID in the gc RAM <- very dangerous (break MP1) and ugly 

	// notes (about "safe texture cache"):
	//	Metroids text issue (character table):
	//	Same addr, same GX_TF_C4 texture data but different TLUT (hence different outputs).
	//	That's why we have to hash the TLUT too for TLUT tex_format dependent textures (ie. GX_TF_C4, GX_TF_C8, GX_TF_C14X2).
	//	And since the address and tex data don't change, the key index in the cacheEntry map can't be the address but 
	//	have to be a real unique ID. 
	//	DONE but not satifiying yet -> may break copyEFBToTexture sometimes.

	//	Pokemon Colosseum text issue (plain text):
	//	Use a GX_TF_I4 512x512 text-flush-texture at a const address.
	//	The problem here was just the sparse hash on the texture. This texture is partly overwrited (what is needed only) 
	//	so lot's of remaning old text. 	Thin white chars on black bg too.

	// TODO:	- clean this up when ready to kill old "unsafe texture cache"
	//			- fix the key index situation with CopyRenderTargetToTexture. 
	//			Could happen only for GX_TF_C4, GX_TF_C8 and GX_TF_C14X2 fmt.
	//			Wonder if we can't use tex width&height to know if EFB might be copied to it...
	//			raw idea:  TOCHECK if addresses are aligned we have few bits left...

    if (address == 0)
        return NULL;

    TexMode0 &tm0 = bpmem.tex[texstage >> 2].texMode0[texstage & 3];
	TexMode1 &tm1 = bpmem.tex[texstage >> 2].texMode1[texstage & 3];
	int maxlevel = (tm1.max_lod >> 4);
	bool UseNativeMips = (tm0.min_filter & 3) && (tm0.min_filter != 8) && g_ActiveConfig.bUseNativeMips;	

    u8 *ptr = g_VideoInitialize.pGetMemoryPointer(address);
	int bsw = TexDecoder_GetBlockWidthInTexels(tex_format) - 1;
	int bsh = TexDecoder_GetBlockHeightInTexels(tex_format) - 1;
	int bsdepth = TexDecoder_GetTexelSizeInNibbles(tex_format);
    int expandedWidth = (width + bsw) & (~bsw);
	int expandedHeight = (height + bsh) & (~bsh);

	u64 hash_value = 0;
    u32 texID = address;
	u64 texHash = 0;
	u32 FullFormat = tex_format;
	bool TextureisDynamic = false;
	if ((tex_format == GX_TF_C4) || (tex_format == GX_TF_C8) || (tex_format == GX_TF_C14X2))
		FullFormat = (tex_format | (tlutfmt << 16));
	if (g_ActiveConfig.bSafeTextureCache || g_ActiveConfig.bHiresTextures || g_ActiveConfig.bDumpTextures)
	{
		texHash =  GetHash64(ptr,TexDecoder_GetTextureSizeInBytes(expandedWidth, expandedHeight, tex_format),g_ActiveConfig.iSafeTextureCache_ColorSamples);
		if ((tex_format == GX_TF_C4) || (tex_format == GX_TF_C8) || (tex_format == GX_TF_C14X2))
		{
			// WARNING! texID != address now => may break CopyRenderTargetToTexture (cf. TODO up)
			// tlut size can be up to 32768B (GX_TF_C14X2) but Safer == Slower.
			// This trick (to change the texID depending on the TLUT addr) is a trick to get around
			// an issue with metroid prime's fonts, where it has multiple sets of fonts on top of
			// each other stored in a single texture, and uses the palette to make different characters
			// visible or invisible. Thus, unless we want to recreate the textures for every drawn character,
			// we must make sure that texture with different tluts get different IDs.
			u64 tlutHash = GetHash64(&texMem[tlutaddr], TexDecoder_GetPaletteSize(tex_format),g_ActiveConfig.iSafeTextureCache_ColorSamples);
			texHash ^= tlutHash;
			if (g_ActiveConfig.bSafeTextureCache)
			{
				texID = texID ^ ((u32)(tlutHash & 0xFFFFFFFF)) ^ ((u32)((tlutHash >> 32) & 0xFFFFFFFF));
			}
		}
		if (g_ActiveConfig.bSafeTextureCache)
			hash_value = texHash;
	}

	bool skip_texture_create = false;
	TexCache::iterator iter = textures.find(texID);

	if (iter != textures.end())
	{
        TCacheEntry &entry = iter->second;

		if (!g_ActiveConfig.bSafeTextureCache)
		{
			if(entry.isRenderTarget || entry.isDynamic)
			{
				if(!g_ActiveConfig.bCopyEFBToTexture)
				{
					hash_value =  GetHash64(ptr,TexDecoder_GetTextureSizeInBytes(expandedWidth, expandedHeight, tex_format),g_ActiveConfig.iSafeTextureCache_ColorSamples);
					if ((tex_format == GX_TF_C4) || (tex_format == GX_TF_C8) || (tex_format == GX_TF_C14X2))
					{
						hash_value ^= GetHash64(&texMem[tlutaddr], TexDecoder_GetPaletteSize(tex_format),g_ActiveConfig.iSafeTextureCache_ColorSamples);
					}
				}
				else
				{
					hash_value = 0;
				}
			}
			else
			{
				hash_value = ((u32 *)ptr)[0];
			}
		}
		else
		{
			if(entry.isRenderTarget || entry.isDynamic)
			{
				if(g_ActiveConfig.bCopyEFBToTexture)
				{
					hash_value = 0;
				}
			}
		}

        if (((entry.isRenderTarget || entry.isDynamic) && hash_value == entry.hash && address == entry.addr) 
			|| ((address == entry.addr) && (hash_value == entry.hash) && ((int) FullFormat == entry.fmt) && entry.MipLevels >= maxlevel))
		{
            entry.frameCount = frameCount;
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, entry.texture);
			GL_REPORT_ERRORD();
            entry.SetTextureParameters(tm0,tm1);
			entry.isDynamic = false;
			return &entry;
        }
        else
        {
            // Let's reload the new texture data into the same texture,
			// instead of destroying it and having to create a new one.
			// Might speed up movie playback very, very slightly.
			TextureisDynamic = (entry.isRenderTarget || entry.isDynamic) && !g_ActiveConfig.bCopyEFBToTexture;
			if (((!(entry.isRenderTarget || entry.isDynamic)  && width == entry.w && height == entry.h && (int)FullFormat == entry.fmt) ||
				((entry.isRenderTarget || entry.isDynamic)  && entry.w == width && entry.h == height && entry.Scaledw == width && entry.Scaledh == height)))
			{
				glBindTexture(GL_TEXTURE_2D, entry.texture);
				GL_REPORT_ERRORD();
				entry.SetTextureParameters(tm0,tm1);
				skip_texture_create = true;
            }
            else
            {
                entry.Destroy(false);
                textures.erase(iter);
            }
        }
    }

    //Make an entry in the table
	TCacheEntry& entry = textures[texID];
	entry.isDynamic = TextureisDynamic;
	entry.isRenderTarget = false;
	PC_TexFormat dfmt = PC_TEX_FMT_NONE;

	if (g_ActiveConfig.bHiresTextures)
	{
		//Load Custom textures
		char texPathTemp[MAX_PATH];
		int oldWidth = width;
		int oldHeight = height;

		sprintf(texPathTemp, "%s_%08x_%i", globals->unique_id, (unsigned int) texHash, tex_format);
		dfmt = HiresTextures::GetHiresTex(texPathTemp, &width, &height, tex_format, temp);

		if (dfmt != PC_TEX_FMT_NONE)
		{
			expandedWidth = width;
			expandedHeight = height;
		}
	}

	if (dfmt == PC_TEX_FMT_NONE)
		dfmt = TexDecoder_Decode(temp, ptr, expandedWidth, expandedHeight, tex_format, tlutaddr, tlutfmt);

    entry.oldpixel = ((u32 *)ptr)[0];

	if (g_ActiveConfig.bSafeTextureCache || entry.isDynamic)
		entry.hash = hash_value;
	else
	{
		entry.hash = (u32)(((double)rand() / RAND_MAX) * 0xFFFFFFFF);
		((u32 *)ptr)[0] = entry.hash;		
	}

    entry.addr = address;
	entry.size_in_bytes = TexDecoder_GetTextureSizeInBytes(expandedWidth, expandedHeight, tex_format);    

	GLenum target = GL_TEXTURE_2D;
	if (!skip_texture_create) {
		glGenTextures(1, (GLuint *)&entry.texture);
		glBindTexture(target, entry.texture);
	}

	bool isPow2 = !((width & (width - 1)) || (height & (height - 1)));
	int TexLevels = (width > height)?width:height;
	TexLevels =  (isPow2 && UseNativeMips && (maxlevel > 0)) ? (int)(log((double)TexLevels)/log((double)2))+ 1 : (isPow2? 0 : 1);
	if(TexLevels > (maxlevel + 1)  && maxlevel > 0)
		TexLevels = (maxlevel + 1);
	entry.MipLevels = maxlevel;
	bool GenerateMipmaps = TexLevels > 1 || TexLevels == 0;
	entry.bHaveMipMaps = GenerateMipmaps;
	int gl_format = 0;
	int gl_iformat = 0;
	int gl_type = 0;	
	GL_REPORT_ERRORD();
	if (dfmt != PC_TEX_FMT_DXT1)
	{
		switch (dfmt)
		{
		default:
		case PC_TEX_FMT_NONE:
			PanicAlert("Invalid PC texture format %i", dfmt); 
		case PC_TEX_FMT_BGRA32:
			gl_format = GL_BGRA;
			gl_iformat = 4;
			gl_type = GL_UNSIGNED_BYTE;
			break;
		case PC_TEX_FMT_RGBA32:
			gl_format = GL_RGBA;
			gl_iformat = 4;
			gl_type = GL_UNSIGNED_BYTE;
			break;
		case PC_TEX_FMT_I4_AS_I8:
			gl_format = GL_LUMINANCE;
			gl_iformat = GL_INTENSITY4;
			gl_type = GL_UNSIGNED_BYTE;
			break;
		case PC_TEX_FMT_IA4_AS_IA8:
			gl_format = GL_LUMINANCE_ALPHA;
			gl_iformat = GL_LUMINANCE4_ALPHA4;
			gl_type = GL_UNSIGNED_BYTE;
			break;
		case PC_TEX_FMT_I8:
			gl_format = GL_LUMINANCE;
			gl_iformat = GL_INTENSITY8;
			gl_type = GL_UNSIGNED_BYTE;
			break;
		case PC_TEX_FMT_IA8:
			gl_format = GL_LUMINANCE_ALPHA;
			gl_iformat = GL_LUMINANCE8_ALPHA8;
			gl_type = GL_UNSIGNED_BYTE;
			break;
		case PC_TEX_FMT_RGB565:
			gl_format = GL_RGB;
			gl_iformat = GL_RGB;
			gl_type = GL_UNSIGNED_SHORT_5_6_5;
			break;
		}
		if (expandedWidth != width)
			glPixelStorei(GL_UNPACK_ROW_LENGTH, expandedWidth);
		//generate mipmaps even if we use native mips to suport textures with less levels		
		if(skip_texture_create)
		{
			glTexSubImage2D(target, 0,0,0,width, height, gl_format, gl_type, temp);
		}
		else
		{
			if (GenerateMipmaps) 
			{
				if(UseNativeMips)
				{				
					glTexImage2D(target, 0, gl_iformat, width, height, 0, gl_format, gl_type, temp);
				}
				else
				{
					glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
					glTexImage2D(target, 0, gl_iformat, width, height, 0, gl_format, gl_type, temp);
					glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
				}
			}
			else
			{
				glTexImage2D(target, 0, gl_iformat, width, height, 0, gl_format, gl_type, temp);			
			}
		}

		if (expandedWidth != width) // reset
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);		
	}
	else
	{
		if(skip_texture_create)
		{
			glCompressedTexSubImage2D(target, 0,0,0,width, height, 
				GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,expandedWidth*expandedHeight/2, temp);
		}
		else
		{
			glCompressedTexImage2D(target, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,
				width, height, 0, expandedWidth*expandedHeight/2, temp);
		}		
	}
	GL_REPORT_ERRORD();
	if(TexLevels > 1 && dfmt != PC_TEX_FMT_NONE)
	{
		int level = 1;
		int mipWidth = width >> 1;
		int mipHeight = height >> 1;
		ptr +=  entry.size_in_bytes;
		while((mipHeight || mipWidth) && (level < TexLevels))
		{
			u32 currentWidth = (mipWidth > 0)? mipWidth : 1;
			u32 currentHeight = (mipHeight > 0)? mipHeight : 1;
			expandedWidth  = (currentWidth + bsw)  & (~bsw);
			expandedHeight = (currentHeight + bsh) & (~bsh);
			TexDecoder_Decode(temp, ptr, expandedWidth, expandedHeight, tex_format, tlutaddr, tlutfmt);							
			if (dfmt != PC_TEX_FMT_DXT1)
			{
				if (expandedWidth != (int)currentWidth)
					glPixelStorei(GL_UNPACK_ROW_LENGTH, expandedWidth);
				glTexImage2D(target, level, gl_iformat, currentWidth, currentHeight, 0, gl_format, gl_type, temp);													
				if (expandedWidth != (int)currentWidth)
					glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			}
			else
			{
				glCompressedTexImage2D(target, level, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,	currentWidth, currentHeight, 0, expandedWidth*expandedHeight/2, temp);				
			}
			GL_REPORT_ERRORD();
			u32 size = (max(mipWidth, bsw) * max(mipHeight, bsh) * bsdepth) >> 1;
			ptr +=  size;
			mipWidth >>= 1;
			mipHeight >>= 1;
			level++;
		}
	}	
    entry.frameCount = frameCount;
    entry.w = width;
    entry.h = height;
	entry.Scaledw = width;
	entry.Scaledh = height;
    entry.fmt = FullFormat;
    entry.SetTextureParameters(tm0,tm1);
    if (g_ActiveConfig.bDumpTextures) // dump texture to file
	{ 
        char szTemp[MAX_PATH];
		char szDir[MAX_PATH];
		const char* uniqueId = globals->unique_id;
		static bool bCheckedDumpDir = false;

		sprintf(szDir,"%s%s", File::GetUserPath(D_DUMPTEXTURES_IDX), uniqueId);

		if(!bCheckedDumpDir)
		{
			if (!File::Exists(szDir) || !File::IsDirectory(szDir))
				File::CreateDir(szDir);

			bCheckedDumpDir = true;
		}

		sprintf(szTemp, "%s/%s_%08x_%i.tga", szDir, uniqueId, (unsigned int) texHash, tex_format);
		if (!File::Exists(szTemp))
			SaveTexture(szTemp, target, entry.texture, entry.w, entry.h);
    }

    INCSTAT(stats.numTexturesCreated);
    SETSTAT(stats.numTexturesAlive, textures.size());
    return &entry;
}


void TextureMngr::CopyRenderTargetToTexture(u32 address, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, int bScaleByHalf, const EFBRectangle &source_rect)
{
	DVSTARTPROFILE();
	GL_REPORT_ERRORD();

	// for intensity values, use Y of YUV format!
	// for all purposes, treat 4bit equivalents as 8bit (probably just used for compression)
	// RGBA8 - RGBA8
	// RGB565 - RGB565
	// RGB5A3 - RGB5A3
	// I4,R4,Z4 - I4
	// IA4,RA4 - IA4
	// Z8M,G8,I8,A8,Z8,R8,B8,Z8L - I8
	// Z16,GB8,RG8,Z16L,IA8,RA8 - IA8
	float colmat[16];
	float fConstAdd[4] = {0};
	memset(colmat, 0, sizeof(colmat));

    if (bFromZBuffer) 
	{
        switch(copyfmt) 
		{
            case 0: // Z4				
            case 1: // Z8
				colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1;
                break;            
            case 3: // Z16 //?
				colmat[1] = colmat[5] = colmat[9] = colmat[14] = 1;
				break;
            case 11: // Z16 (reverse order)
				colmat[2] = colmat[6] = colmat[10] = colmat[13] = 1;
                break;
            case 6: // Z24X8
				colmat[2] = colmat[5] = colmat[8] = colmat[15] = 1;
                break;
            case 9: // Z8M
				colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1;
                break;
            case 10: // Z8L
				colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1;
                break;
            case 12: // Z16L
				colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1;
                break;
            default:
                ERROR_LOG(VIDEO, "Unknown copy zbuf format: 0x%x", copyfmt);
				colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
                break;
        }
    }
    else if (bIsIntensityFmt) 
	{
			// TODO - verify these coefficients
        fConstAdd[0] = fConstAdd[1] = fConstAdd[2] = 16.0f/255.0f;
		colmat[0] = 0.257f; colmat[1] = 0.504f; colmat[2] = 0.098f;
        colmat[4] = 0.257f; colmat[5] = 0.504f; colmat[6] = 0.098f;
        colmat[8] = 0.257f; colmat[9] = 0.504f; colmat[10] = 0.098f;
		if (copyfmt < 2) 
		{
            fConstAdd[3] = 16.0f / 255.0f;
            colmat[12] = 0.257f; colmat[13] = 0.504f; colmat[14] = 0.098f;
        }
        else// alpha
            colmat[15] = 1;        
    }
    else 
	{
        switch (copyfmt) 
		{
            case 0: // R4
				colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1;
				break;
            case 8: // R8
				colmat[0] = colmat[4] = colmat[8] = colmat[12] = 1;
                break;
            case 2: // RA4
				colmat[0] = colmat[4] = colmat[8] = colmat[15] = 1;
                break;
            case 3: // RA8
				colmat[0] = colmat[4] = colmat[8] = colmat[15] = 1;
                break;
            case 7: // A8
				colmat[3] = colmat[7] = colmat[11] = colmat[15] = 1; 
                break;
            case 9: // G8
				colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1;
                break;
            case 10: // B8
				colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1;
                break;
            case 11: // RG8
				colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1;
                break;
            case 12: // GB8
				colmat[1] = colmat[5] = colmat[9] = colmat[14] = 1;
                break;
            case 4: // RGB565
				colmat[0] = colmat[5] = colmat[10] = 1;
                fConstAdd[3] = 1; // set alpha to 1
                break;
            case 5: // RGB5A3
				colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
                break;
            case 6: // RGBA8
				colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
                break;
            default:
                ERROR_LOG(VIDEO, "Unknown copy color format: 0x%x", copyfmt);
				colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
                break;
        }
    }

	bool bIsInit = textures.find(address) != textures.end();

	PRIM_LOG("copytarg: addr=0x%x, fromz=%d, intfmt=%d, copyfmt=%d", address, (int)bFromZBuffer, (int)bIsIntensityFmt,copyfmt);

	TCacheEntry& entry = textures[address];
	entry.hash = 0;
	entry.frameCount = frameCount;

	int w = (abs(source_rect.GetWidth()) >> bScaleByHalf);
	int h = (abs(source_rect.GetHeight()) >> bScaleByHalf);

	float xScale = Renderer::GetTargetScaleX();
	float yScale = Renderer::GetTargetScaleY();
	
	int Scaledtex_w = (g_ActiveConfig.bCopyEFBScaled)?((int)(xScale * w)) : w;
	int Scaledtex_h = (g_ActiveConfig.bCopyEFBScaled)?((int)(yScale * h)) : h;

	GLenum gl_format = GL_RGBA;
	GLenum gl_iformat = 4;
	GLenum gl_type = GL_UNSIGNED_BYTE;


	GL_REPORT_ERRORD();

	if (!bIsInit) 
	{
		glGenTextures(1, (GLuint *)&entry.texture);
		glBindTexture(GL_TEXTURE_2D, entry.texture);
		GL_REPORT_ERRORD();
		glTexImage2D(GL_TEXTURE_2D, 0, gl_iformat, Scaledtex_w, Scaledtex_h, 0, gl_format, gl_type, NULL);
		GL_REPORT_ERRORD();
		entry.isRenderTarget = true;
		entry.isDynamic = false;
	}
	else 
	{
		_assert_(entry.texture);
		GL_REPORT_ERRORD();
		if(entry.isDynamic)
		{
			Scaledtex_h = h;
			Scaledtex_w = w;
		}
		if (((entry.isRenderTarget || entry.isDynamic)  && entry.Scaledw == Scaledtex_w && entry.Scaledh == Scaledtex_h))
		{
			glBindTexture(GL_TEXTURE_2D, entry.texture);
			// for some reason mario sunshine errors here...
			// Beyond Good and Evil does too, occasionally.
			GL_REPORT_ERRORD();
		} else {
			// Delete existing texture.			
			glDeleteTextures(1,(GLuint *)&entry.texture);
			glGenTextures(1, (GLuint *)&entry.texture);
			glBindTexture(GL_TEXTURE_2D, entry.texture);
			glTexImage2D(GL_TEXTURE_2D, 0, gl_iformat, Scaledtex_w, Scaledtex_h, 0, gl_format, gl_type, NULL);
			GL_REPORT_ERRORD();
			entry.isRenderTarget = !entry.isDynamic;
		}
	}

	if (!bIsInit) 
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (GL_REPORT_ERROR() != GL_NO_ERROR) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
			GL_REPORT_ERRORD();
		}
	}

	entry.addr = address;
	entry.w = w;
	entry.h = h;
	entry.Scaledw = Scaledtex_w;
	entry.Scaledh = Scaledtex_h;
	entry.fmt = copyfmt;	
	entry.hash = 0;

	// Make sure to resolve anything we need to read from.
	GLuint read_texture = bFromZBuffer ? g_framebufferManager.ResolveAndGetDepthTarget(source_rect) : g_framebufferManager.ResolveAndGetRenderTarget(source_rect);

    GL_REPORT_ERRORD();

    // We have to run a pixel shader, for color conversion.
    Renderer::ResetAPIState(); // reset any game specific settings
	if(!entry.isDynamic || g_ActiveConfig.bCopyEFBToTexture)
	{
		if (s_TempFramebuffer == 0)
			glGenFramebuffersEXT(1, (GLuint *)&s_TempFramebuffer);

		g_framebufferManager.SetFramebuffer(s_TempFramebuffer);
		// Bind texture to temporary framebuffer
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, entry.texture, 0);
		GL_REPORT_FBO_ERROR();
		GL_REPORT_ERRORD();
	    
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glActiveTexture(GL_TEXTURE0);
		glEnable(GL_TEXTURE_RECTANGLE_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, read_texture);
	   
		glViewport(0, 0, Scaledtex_w, Scaledtex_h);

		PixelShaderCache::SetCurrentShader(bFromZBuffer ? PixelShaderCache::GetDepthMatrixProgram() : PixelShaderCache::GetColorMatrixProgram());    
		PixelShaderManager::SetColorMatrix(colmat, fConstAdd); // set transformation
		GL_REPORT_ERRORD();

		TargetRectangle targetSource = Renderer::ConvertEFBRectangle(source_rect);

		glBegin(GL_QUADS);
		glTexCoord2f((GLfloat)targetSource.left,  (GLfloat)targetSource.bottom); glVertex2f(-1,  1);
		glTexCoord2f((GLfloat)targetSource.left,  (GLfloat)targetSource.top  ); glVertex2f(-1, -1);
		glTexCoord2f((GLfloat)targetSource.right, (GLfloat)targetSource.top  ); glVertex2f( 1, -1);
		glTexCoord2f((GLfloat)targetSource.right, (GLfloat)targetSource.bottom); glVertex2f( 1,  1);
		glEnd();

		GL_REPORT_ERRORD();

		// Unbind texture from temporary framebuffer
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);
	}

	if(!g_ActiveConfig.bCopyEFBToTexture)
	{
		textures[address].hash = TextureConverter::EncodeToRamFromTexture(
			address,
			read_texture,
			xScale,
			yScale, 
			bFromZBuffer, 
			bIsIntensityFmt, 
			copyfmt, 
			bScaleByHalf, 
			source_rect);
	
	}
	// Return to the EFB.
    g_framebufferManager.SetFramebuffer(0);
    Renderer::RestoreAPIState();
    VertexShaderManager::SetViewportChanged();
    TextureMngr::DisableStage(0);

    GL_REPORT_ERRORD();

	if (g_ActiveConfig.bDumpEFBTarget)
	{
		static int count = 0;
		SaveTexture(StringFromFormat("%sefb_frame_%i.tga", File::GetUserPath(D_DUMPTEXTURES_IDX), count++).c_str(), GL_TEXTURE_2D, entry.texture, entry.w, entry.h);
	}
}

void TextureMngr::DisableStage(int stage)
{
	glActiveTexture(GL_TEXTURE0 + stage);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_RECTANGLE_ARB);
}

void TextureMngr::ClearRenderTargets()
{
    for (TexCache::iterator iter = textures.begin(); iter != textures.end(); ++iter)
        iter->second.isRenderTarget = false;
}
