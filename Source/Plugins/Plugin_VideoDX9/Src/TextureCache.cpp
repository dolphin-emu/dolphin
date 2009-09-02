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

#include "Render.h"

#include "TextureDecoder.h"
#include "TextureCache.h"

#include "../../../Core/Core/Src/ConfigManager.h" // FIXME

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
	if (!isRenderTarget && !shutdown) {
		u32 *ptr = (u32*)g_VideoInitialize.pGetMemoryPointer(addr + hashoffset*4);
		if (ptr && *ptr == hash)
			*ptr = oldpixel;
	}
}

void TextureCache::Init()
{
	temp = (u8*)AllocateMemoryPages(TEMP_SIZE);
	TexDecoder_SetTexFmtOverlayOptions(g_Config.bTexFmtOverlayEnable, g_Config.bTexFmtOverlayCenter);
}

void TextureCache::Invalidate(bool shutdown)
{
	for (TexCache::iterator iter = textures.begin(); iter != textures.end(); iter++)
		iter->second.Destroy(shutdown);
	textures.clear();
}

void TextureCache::Shutdown()
{
	Invalidate(true);

	FreeMemoryPages(temp, TEMP_SIZE);	
	temp = NULL;
}

void TextureCache::Cleanup()
{
	TexCache::iterator iter=textures.begin();

	while(iter != textures.end())
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

TextureCache::TCacheEntry *TextureCache::Load(int stage, u32 address, int width, int height, int format, int tlutaddr, int tlutfmt)
{
	if (address == 0)
		return NULL;
	
	TexCache::iterator iter = textures.find(address);
	
	u8 *ptr = g_VideoInitialize.pGetMemoryPointer(address);

	int palSize = TexDecoder_GetPaletteSize(format);
	u32 palhash = 0xc0debabe;
	if (palSize)
	{
		// TODO: Share this code with the GL plugin.
		if (palSize > 32) 
			palSize = 32; //let's not do excessive amount of checking
		u8 *pal = g_VideoInitialize.pGetMemoryPointer(tlutaddr);
		if (pal != 0)
		{
			for (int i = 0; i < palSize; i++)
			{
				palhash = _rotl(palhash,13);
				palhash ^= pal[i];
				palhash += 31;
			}
		}
	}

	static LPDIRECT3DTEXTURE9 lastTexture[8] = {0,0,0,0,0,0,0,0};

	int bs = TexDecoder_GetBlockWidthInTexels(format)-1; //TexelSizeInNibbles(format)*width*height/16;
	int expandedWidth = (width+bs) & (~bs);
	u32 hash_value = TexDecoder_GetSafeTextureHash(ptr, expandedWidth, height, format, 0);

	if (iter != textures.end())
	{
		TCacheEntry &entry = iter->second;

		if (entry.isRenderTarget || ((address == entry.addr) && (hash_value == entry.hash)))
		{
			entry.frameCount = frameCount;
			if (lastTexture[stage] == entry.texture)
			{
				return &entry;
			}
			lastTexture[stage] = entry.texture;
			
			// D3D::dev->SetTexture(stage,iter->second.texture);
			Renderer::SetTexture( stage, entry.texture );

			return &entry;
		}
		else
		{
/*			if (width == iter->second.w && height==entry.h && format==entry.fmt)
			{
				LPDIRECT3DTEXTURE9 tex = entry.texture;
				int bs = TexDecoder_GetBlockWidthInTexels(format)-1; //TexelSizeInNibbles(format)*width*height/16;
				int expandedWidth = (width+bs) & (~bs);
				D3DFORMAT dfmt = TexDecoder_Decode(temp,ptr,expandedWidth,height,format, tlutaddr, tlutfmt);
				D3D::ReplaceTexture2D(tex,temp,width,height,expandedWidth,dfmt);
				D3D::dev->SetTexture(stage,tex);
				return;
			}
			else
			{*/
				entry.Destroy(false);
				textures.erase(iter);
			//}
		}
	}
	
	PC_TexFormat pcfmt = TexDecoder_Decode(temp, ptr, expandedWidth, height, format, tlutaddr, tlutfmt);
	D3DFORMAT d3d_fmt;
	switch (pcfmt) {
	case PC_TEX_FMT_BGRA32:
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
	TCacheEntry& entry = textures[address];

	entry.hashoffset = 0; 
	entry.hash = hash_value;
	//entry.hash = (u32)(((double)rand() / RAND_MAX) * 0xFFFFFFFF);
	entry.paletteHash = palhash;
	entry.oldpixel = ((u32 *)ptr)[entry.hashoffset];
	//((u32 *)ptr)[entry.hashoffset] = entry.hash;

	entry.addr = address;
	entry.isRenderTarget = false;
	entry.isNonPow2 = ((width & (width - 1)) || (height & (height - 1)));
	entry.texture = D3D::CreateTexture2D((BYTE*)temp, width, height, expandedWidth, d3d_fmt);
	entry.frameCount = frameCount;
	entry.w = width;
	entry.h = height;
	entry.fmt = format;
	entry.mode = bpmem.tex[stage > 3].texMode0[stage & 3];
	
	if (g_Config.bDumpTextures)
	{ // dump texture to file
		char szTemp[MAX_PATH];
		char szDir[MAX_PATH];
		bool bCheckedDumpDir = false;
		sprintf(szDir,"%s/%s",FULL_DUMP_TEXTURES_DIR,((struct SConfig *)globals->config)->m_LocalCoreStartupParameter.GetUniqueID().c_str());
		if(!bCheckedDumpDir)
		{
			if (!File::Exists(szDir) || !File::IsDirectory(szDir))
				File::CreateDir(szDir);

			bCheckedDumpDir = true;
		}
		sprintf(szTemp, "%s/%s_%08x_%i.png",szDir, ((struct SConfig *)globals->config)->m_LocalCoreStartupParameter.GetUniqueID().c_str(), hash_value, format);
		//sprintf(szTemp, "%s\\txt_%04i_%i.png", g_Config.texDumpPath.c_str(), counter++, format); <-- Old method
		if (!File::Exists(szTemp))
			D3DXSaveTextureToFileA(szTemp,D3DXIFF_BMP,entry.texture,0);
	}

	INCSTAT(stats.numTexturesCreated);
	SETSTAT(stats.numTexturesAlive, (int)textures.size());

	//Set the texture!
	// D3D::dev->SetTexture(stage,entry.texture);
	Renderer::SetTexture( stage, entry.texture );

	lastTexture[stage] = entry.texture;

	return &entry;
}
 

void TextureCache::CopyEFBToRenderTarget(u32 address, RECT *source)
{
	TexCache::iterator iter;
	LPDIRECT3DTEXTURE9 tex;
	iter = textures.find(address);
	if (iter != textures.end())
	{
		if (!iter->second.isRenderTarget)
		{
			ERROR_LOG(VIDEO, "Using non-rendertarget texture as render target!!! WTF?", FALSE);
			//TODO: remove it and recreate it as a render target
		}
		tex = iter->second.texture;
		iter->second.frameCount = frameCount;
	}
	else
	{
		TCacheEntry entry;
		entry.isRenderTarget = true;
		entry.hash = 0;
		entry.hashoffset = 0;
		entry.frameCount = frameCount;
		// TODO(ector): infer this size in some sensible way
		D3D::dev->CreateTexture(512, 512, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &entry.texture, 0);
		textures[address] = entry;
		tex = entry.texture;
	}

	LPDIRECT3DSURFACE9 srcSurface,destSurface;
	tex->GetSurfaceLevel(0,&destSurface);
	srcSurface = D3D::GetBackBufferSurface();
	D3D::dev->StretchRect(srcSurface,source,destSurface,0,D3DTEXF_NONE);
	destSurface->Release();
}
