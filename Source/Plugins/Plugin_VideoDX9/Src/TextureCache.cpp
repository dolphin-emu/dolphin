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

#include <d3dx9.h>

#include "Globals.h"
#include "Statistics.h"
#include "MemoryUtil.h"
#include "Hash.h"

#include "CommonPaths.h"
#include "FileUtil.h"

#include "D3DBase.h"
#include "D3DTexture.h"
#include "D3DUtil.h"
#include "FramebufferManager.h"
#include "PixelShaderCache.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "VertexShaderCache.h"

#include "Render.h"

#include "TextureDecoder.h"
#include "TextureCache.h"

#include "debugger/debugger.h"

u8 *TextureCache::temp = NULL;
TextureCache::TexCache TextureCache::textures;

extern int frameCount;

#define TEMP_SIZE (1024*1024*4)
#define TEXTURE_KILL_THRESHOLD 200

void TextureCache::TCacheEntry::Destroy(bool shutdown)
{
	if (texture)
		texture->Release();
	texture = 0;
	if (!isRenderTarget && !shutdown && !g_ActiveConfig.bSafeTextureCache)
	{
		u32 *ptr = (u32*)g_VideoInitialize.pGetMemoryPointer(addr);
		if (ptr && *ptr == hash)
			*ptr = oldpixel;
	}
}

void TextureCache::Init()
{
	temp = (u8*)AllocateMemoryPages(TEMP_SIZE);
	TexDecoder_SetTexFmtOverlayOptions(g_ActiveConfig.bTexFmtOverlayEnable, g_ActiveConfig.bTexFmtOverlayCenter);
}

void TextureCache::Invalidate(bool shutdown)
{
	for (TexCache::iterator iter = textures.begin(); iter != textures.end(); iter++)
		iter->second.Destroy(shutdown);
	textures.clear();
}

void TextureCache::InvalidateRange(u32 start_address, u32 size)
{
	TexCache::iterator iter = textures.begin();
	while (iter != textures.end())
	{
		if (iter->second.IntersectsMemoryRange(start_address, size))
		{
			iter->second.Destroy(false);
			ERASE_THROUGH_ITERATOR(textures, iter);
		}
		else {
			++iter;
		}
	}
}

bool TextureCache::TCacheEntry::IntersectsMemoryRange(u32 range_address, u32 range_size)
{
	if (addr + size_in_bytes < range_address)
		return false;
	if (addr >= range_address + range_size)
		return false;
	return true;
}

void TextureCache::Shutdown()
{
	Invalidate(true);
	FreeMemoryPages(temp, TEMP_SIZE);	
	temp = NULL;
}

void TextureCache::Cleanup()
{
	TexCache::iterator iter = textures.begin();
	while (iter != textures.end())
	{
		if (frameCount > TEXTURE_KILL_THRESHOLD + iter->second.frameCount)
		{
			if (!iter->second.isRenderTarget)
			{
				iter->second.Destroy(false);
				iter = textures.erase(iter);
			}
			else
			{
				// Used to be just iter++
				iter->second.Destroy(false);
				iter = textures.erase(iter);
			}
		}
		else
		{
            iter++;
        }
	}
}

TextureCache::TCacheEntry *TextureCache::Load(int stage, u32 address, int width, int height, int tex_format, int tlutaddr, int tlutfmt)
{
	if (address == 0)
		return NULL;

	u8 *ptr = g_VideoInitialize.pGetMemoryPointer(address);
	int bsw = TexDecoder_GetBlockWidthInTexels(tex_format) - 1; //TexelSizeInNibbles(format)*width*height/16;
	int bsh = TexDecoder_GetBlockHeightInTexels(tex_format) - 1; //TexelSizeInNibbles(format)*width*height/16;
	int expandedWidth  = (width + bsw)  & (~bsw);
	int expandedHeight = (height + bsh) & (~bsh);

	u32 hash_value;
	u32 texID = address;
	u32 texHash;

	if (g_ActiveConfig.bSafeTextureCache || g_ActiveConfig.bDumpTextures)
	{
		texHash = TexDecoder_GetSafeTextureHash(ptr, expandedWidth, expandedHeight, tex_format, 0);
		if (g_ActiveConfig.bSafeTextureCache)
			hash_value = texHash;
		if ((tex_format == GX_TF_C4) || (tex_format == GX_TF_C8) || (tex_format == GX_TF_C14X2))
		{
			// WARNING! texID != address now => may break CopyRenderTargetToTexture (cf. TODO up)
			// tlut size can be up to 32768B (GX_TF_C14X2) but Safer == Slower.
			// This trick (to change the texID depending on the TLUT addr) is a trick to get around
			// an issue with metroid prime's fonts, where it has multiple sets of fonts on top of
			// each other stored in a single texture, and uses the palette to make different characters
			// visible or invisible. Thus, unless we want to recreate the textures for every drawn character,
			// we must make sure that texture with different tluts get different IDs.
 			u32 tlutHash = TexDecoder_GetTlutHash(&texMem[tlutaddr], (tex_format == GX_TF_C4) ? 32 : 128);
			texHash ^= tlutHash;
			if (g_ActiveConfig.bSafeTextureCache)
				texID ^= tlutHash;
		}
	}

	bool skip_texture_create = false;
	TexCache::iterator iter = textures.find(texID);

	if (iter != textures.end())
	{
		TCacheEntry &entry = iter->second;

		if (!g_ActiveConfig.bSafeTextureCache)
			hash_value = ((u32 *)ptr)[0];

		if (entry.isRenderTarget || ((address == entry.addr) && (hash_value == entry.hash)))
		{
			entry.frameCount = frameCount;
			D3D::SetTexture(stage, entry.texture);
			return &entry;
		}
		else
		{
            // Let's reload the new texture data into the same texture,
			// instead of destroying it and having to create a new one.
			// Might speed up movie playback very, very slightly.

			if (width == entry.w && height==entry.h && tex_format == entry.fmt)
			{
				skip_texture_create = true;
			}
			else
			{
				entry.Destroy(false);
				textures.erase(iter);
			}
		}
	}
	
	PC_TexFormat pcfmt = TexDecoder_Decode(temp, ptr, expandedWidth, height, tex_format, tlutaddr, tlutfmt);

	D3DFORMAT d3d_fmt;
	switch (pcfmt) {
	case PC_TEX_FMT_BGRA32:
	case PC_TEX_FMT_RGBA32:
		d3d_fmt = D3DFMT_A8R8G8B8;
		break;
	case PC_TEX_FMT_RGB565:
		d3d_fmt = D3DFMT_R5G6B5;
		break;
	case PC_TEX_FMT_IA4_AS_IA8:
		d3d_fmt = D3DFMT_A8L8;
		break;
	case PC_TEX_FMT_I8:
	case PC_TEX_FMT_I4_AS_I8:
		d3d_fmt = D3DFMT_A8P8; // A hack which means the format is a packed
							   // 8-bit intensity texture. It is unpacked
							   // to A8L8 in D3DTexture.cpp
		break;
	case PC_TEX_FMT_IA8:
		d3d_fmt = D3DFMT_A8L8;
		break;
	case PC_TEX_FMT_DXT1:
		d3d_fmt = D3DFMT_DXT1;
		break;
	}
	
	//Make an entry in the table
	TCacheEntry& entry = textures[texID];

	entry.oldpixel = ((u32 *)ptr)[0];
	if (g_ActiveConfig.bSafeTextureCache)
		entry.hash = hash_value;
	else
	{
		entry.hash = (u32)(((double)rand() / RAND_MAX) * 0xFFFFFFFF);
	    ((u32 *)ptr)[0] = entry.hash;
	}

	entry.addr = address;
	entry.size_in_bytes = TexDecoder_GetTextureSizeInBytes(width, height, tex_format);
	entry.isRenderTarget = false;
	entry.isNonPow2 = ((width & (width - 1)) || (height & (height - 1)));
	if (!skip_texture_create) {
		entry.texture = D3D::CreateTexture2D((BYTE*)temp, width, height, expandedWidth, d3d_fmt);
	} else {
		D3D::ReplaceTexture2D(entry.texture, (BYTE*)temp, width, height, expandedWidth, d3d_fmt);
	}
	entry.frameCount = frameCount;
	entry.w = width;
	entry.h = height;
	entry.fmt = tex_format;
	
	if (g_ActiveConfig.bDumpTextures)
	{
		// dump texture to file
		char szTemp[MAX_PATH];
		char szDir[MAX_PATH];
		const char* uniqueId = globals->unique_id;
		bool bCheckedDumpDir = false;
		sprintf(szDir, "%s/%s", FULL_DUMP_TEXTURES_DIR, uniqueId);
		if (!bCheckedDumpDir)
		{
			if (!File::Exists(szDir) || !File::IsDirectory(szDir))
				File::CreateDir(szDir);

			bCheckedDumpDir = true;
		}
		sprintf(szTemp, "%s/%s_%08x_%i.png", szDir, uniqueId, texHash, tex_format);
		//sprintf(szTemp, "%s\\txt_%04i_%i.png", g_Config.texDumpPath.c_str(), counter++, format); <-- Old method
		if (!File::Exists(szTemp))
			D3DXSaveTextureToFileA(szTemp,D3DXIFF_BMP,entry.texture,0);
	}

	INCSTAT(stats.numTexturesCreated);
	SETSTAT(stats.numTexturesAlive, (int)textures.size());

	//Set the texture!
	D3D::SetTexture(stage, entry.texture);

	DEBUGGER_PAUSE_LOG_AT(NEXT_NEW_TEXTURE,true,{printf("A new texture (%d x %d) is loaded", width, height);});
	return &entry;
}
 
void TextureCache::CopyRenderTargetToTexture(u32 address, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, int bScaleByHalf, const EFBRectangle &source_rect)
{
	int efb_w = source_rect.GetWidth();
	int efb_h = source_rect.GetHeight();

	int tex_w = (abs(source_rect.GetWidth()) >> bScaleByHalf);
	int tex_h = (abs(source_rect.GetHeight()) >> bScaleByHalf);

	TexCache::iterator iter;
	LPDIRECT3DTEXTURE9 tex;
	iter = textures.find(address);
	if (iter != textures.end())
	{
		if (iter->second.isRenderTarget && iter->second.w == tex_w && iter->second.h == tex_h)
		{
			
			tex = iter->second.texture;
			iter->second.frameCount = frameCount;
			goto have_texture;
		}
		else
		{
			// Remove it and recreate it as a render target
			iter->second.texture->Release();
			iter->second.texture = 0;
			textures.erase(iter);
		}
	}

	{
		TCacheEntry entry;
		entry.isRenderTarget = true;
		entry.hash = 0;
		entry.frameCount = frameCount;
		entry.w = tex_w;
		entry.h = tex_h;
		entry.fmt = copyfmt;
		
		D3D::dev->CreateTexture(tex_w, tex_h, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &entry.texture, 0);
		textures[address] = entry;
		tex = entry.texture;
	}

have_texture:	
	float colmat[16]= {0.0f};
	float fConstAdd[4] = {0.0f};	

    if (bFromZBuffer) 
	{
		switch(copyfmt) 
		{
            case 0: // Z4
            case 1: // Z8
                colmat[0] = colmat[4] = colmat[8] = colmat[12] =1.0f;
                break;
            
            case 3: // Z16 //?
                colmat[1] = colmat[5] = colmat[9] = colmat[12] = 1.0f;
            case 11: // Z16 (reverse order)
                colmat[0] = colmat[4] = colmat[8] = colmat[13] = 1.0f;
                break;
            case 6: // Z24X8
                colmat[0] = colmat[5] = colmat[10] = 1.0f;
                break;
            case 9: // Z8M
                colmat[1] = colmat[5] = colmat[9] = colmat[13] = 1.0f;
                break;
            case 10: // Z8L
                colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1.0f;
                break;
            case 12: // Z16L
                colmat[2] = colmat[6] = colmat[10] = colmat[13] = 1.0f;
                break;
            default:
                ERROR_LOG(VIDEO, "Unknown copy zbuf format: 0x%x", copyfmt);
                colmat[2] = colmat[5] = colmat[8] = 1.0f;
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
                }
                else// alpha
                    colmat[15] = 1;

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
                break;
            case 2: // RA4
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
            case 6: // RGBA8
                colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
                break;

            default:
                ERROR_LOG(VIDEO, "Unknown copy color format: 0x%x", copyfmt);
                colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
                break;
        }
    }
	// Make sure to resolve anything we need to read from.
	LPDIRECT3DTEXTURE9 read_texture = bFromZBuffer ? FBManager::GetEFBDepthTexture(source_rect) : FBManager::GetEFBColorTexture(source_rect);
    
    // We have to run a pixel shader, for color conversion.
    Renderer::ResetAPIState(); // reset any game specific settings
	LPDIRECT3DSURFACE9 Rendersurf = NULL;
	tex->GetSurfaceLevel(0,&Rendersurf);
	D3D::dev->SetDepthStencilSurface(NULL);
	D3D::dev->SetRenderTarget(0, Rendersurf);		
    
	D3DVIEWPORT9 vp;

	// Stretch picture with increased internal resolution
	vp.X = 0;
	vp.Y = 0;
	vp.Width  = tex_w;
	vp.Height = tex_h;
	vp.MinZ = 0.0f;
	vp.MaxZ = 1.0f;
	D3D::dev->SetViewport(&vp);
	RECT destrect;
	destrect.bottom = tex_h;
	destrect.left = 0;
	destrect.right = tex_w;
	destrect.top = 0;
	

	PixelShaderManager::SetColorMatrix(colmat, fConstAdd); // set transformation
	TargetRectangle targetSource = Renderer::ConvertEFBRectangle(source_rect);
	RECT sourcerect;
	sourcerect.bottom = targetSource.bottom;
	sourcerect.left = targetSource.left;
	sourcerect.right = targetSource.right;
	sourcerect.top = targetSource.top;

	if(bScaleByHalf)
		D3D::dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    else
		D3D::dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);

	D3DFORMAT bformat = FBManager::GetEFBDepthRTSurfaceFormat();
	D3D::drawShadedTexQuad(read_texture,&sourcerect, Renderer::GetTargetWidth() , Renderer::GetTargetHeight(),&destrect,((bformat != FOURCC_RAWZ && bformat != D3DFMT_D24X8) && bFromZBuffer)?  PixelShaderCache::GetDepthMatrixProgram(): PixelShaderCache::GetColorMatrixProgram(),VertexShaderCache::GetSimpleVertexShader());	
	
	D3D::RefreshSamplerState(0, D3DSAMP_MINFILTER);

	D3D::dev->SetRenderTarget(0, FBManager::GetEFBColorRTSurface());
	D3D::dev->SetDepthStencilSurface(FBManager::GetEFBDepthRTSurface());	
	Renderer::RestoreAPIState();	
	Rendersurf->Release();
}
