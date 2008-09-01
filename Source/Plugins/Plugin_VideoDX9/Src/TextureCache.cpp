#include <d3dx9.h>

#include "Common.h"

#include "D3DBase.h"
#include "D3DTexture.h"

#include "Render.h"

#include "TextureDecoder.h"
#include "TextureCache.h"

#include "Globals.h"
#include "main.h"

u8 *TextureCache::temp = NULL;
TextureCache::TexCache TextureCache::textures;

extern int frameCount;

#define TEMP_SIZE (1024*1024*4)

void TextureCache::TCacheEntry::Destroy()
{
	if (texture)
		texture->Release();
	texture = 0;
	if (!isRenderTarget) {
		u32 *ptr = (u32*)g_VideoInitialize.pGetMemoryPointer(addr + hashoffset*4);
		if (*ptr == hash)
			*ptr = oldpixel;
	}
}

void TextureCache::Init()
{
	temp = (u8*)VirtualAlloc(0,TEMP_SIZE,MEM_COMMIT,PAGE_READWRITE);  	
	TexDecoder_SetTexFmtOverlayOptions(g_Config.bTexFmtOverlayEnable,g_Config.bTexFmtOverlayCenter);
}

void TextureCache::Invalidate()
{
	TexCache::iterator iter = textures.begin();
	for (; iter != textures.end(); iter++)
		iter->second.Destroy();
	textures.clear();
	TexDecoder_SetTexFmtOverlayOptions(g_Config.bTexFmtOverlayEnable, g_Config.bTexFmtOverlayCenter);
}

void TextureCache::Shutdown()
{
	Invalidate();

	VirtualFree(temp, 0, MEM_RELEASE);	
	temp = NULL;
}

void TextureCache::Cleanup()
{
	TexCache::iterator iter=textures.begin();

	while(iter != textures.end())
	{
		if (frameCount>20+iter->second.frameCount)
		{
			if (!iter->second.isRenderTarget)
			{
				iter->second.Destroy();
				iter = textures.erase(iter);
			}
			else
			{
				iter++;
			}
		}
		else
		{
            iter++;
        }
	}
}

void TextureCache::Load(int stage, u32 address, int width, int height, int format, int tlutaddr, int tlutfmt)
{
	if (address == 0)
		return;
	TexCache::iterator iter = textures.find(address);
	
	u8 *ptr = g_VideoInitialize.pGetMemoryPointer(address);

	int palSize = TexDecoder_GetPaletteSize(format);
	u32 palhash = 0xc0debabe;
	if (palSize)
	{
		if (palSize>16) 
			palSize = 16; //let's not do excessive amount of checking
		u8 *pal = g_VideoInitialize.pGetMemoryPointer(tlutaddr);
		if (pal != 0)
		{
			for (int i=0; i<palSize; i++)
			{
				palhash = _rotl(palhash,13);
				palhash ^= pal[i];
				palhash += 31;
			}
		}
	}

	static LPDIRECT3DTEXTURE9 lastTexture[8] = {0,0,0,0,0,0,0,0};

	if (iter != textures.end())
	{
		TCacheEntry &entry = iter->second;

		if (entry.isRenderTarget || (((u32 *)ptr)[entry.hashoffset] == entry.hash && palhash == entry.paletteHash)) //stupid, improve
		{
			iter->second.frameCount = frameCount;
			if (lastTexture[stage] == iter->second.texture)
			{
				return;
			}
			lastTexture[stage] = iter->second.texture;
			
			// D3D::dev->SetTexture(stage,iter->second.texture);
			Renderer::SetTexture( stage, iter->second.texture );

			return;
		}
		else
		{
			TCacheEntry &entry = iter->second;
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
				iter->second.Destroy();
				textures.erase(iter);
			//}
		}
	}
	
	int bs = TexDecoder_GetBlockWidthInTexels(format)-1; //TexelSizeInNibbles(format)*width*height/16;
	int expandedWidth = (width+bs) & (~bs);
	PC_TexFormat pcfmt = TexDecoder_Decode(temp,ptr,expandedWidth,height,format, tlutaddr, tlutfmt);
	D3DFORMAT d3d_fmt;
	switch (pcfmt) {
	case PC_TEX_FMT_BGRA32:
		d3d_fmt = D3DFMT_A8R8G8B8;
		break;
	}

	//Make an entry in the table
	TCacheEntry entry;
	entry.hashoffset = 0; 
	entry.hash = (u32)(((double)rand() / RAND_MAX) * 0xFFFFFFFF);
	entry.paletteHash = palhash;
	entry.oldpixel = ((u32 *)ptr)[entry.hashoffset];
	((u32 *)ptr)[entry.hashoffset] = entry.hash;

	entry.addr = address;
	entry.isRenderTarget=false;
	entry.isNonPow2 = ((width&(width-1)) || (height&(height-1)));
	entry.texture = D3D::CreateTexture2D((BYTE*)temp, width, height, expandedWidth, d3d_fmt);
	entry.frameCount = frameCount;
	entry.w=width;
	entry.h=height;
	entry.fmt=format;
	textures[address] = entry;
	
	if (g_Config.bDumpTextures)
	{ // dump texture to file
		static int counter = 0;
		char szTemp[MAX_PATH];
		sprintf(szTemp, "%s\\txt_%04i_%i.png", g_Config.texDumpPath, counter++, format);
		
	    D3DXSaveTextureToFile(szTemp,D3DXIFF_BMP,entry.texture,0);
	}

	INCSTAT(stats.numTexturesCreated);
	SETSTAT(stats.numTexturesAlive, (int)textures.size());

	//Set the texture!
	// D3D::dev->SetTexture(stage,entry.texture);
	Renderer::SetTexture( stage, entry.texture );

	lastTexture[stage] = entry.texture;
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
			g_VideoInitialize.pLog("Using non-rendertarget texture as render target!!! WTF?", FALSE);
			//TODO: remove it and recreate it as a render target
		}
		tex = iter->second.texture;
		iter->second.frameCount=frameCount;
	}
	else
	{
		TCacheEntry entry;
		entry.isRenderTarget=true;
		entry.hash = 0;
		entry.hashoffset = 0;
		entry.frameCount = frameCount;
		// TODO(ector): infer this size in some sensible way
		D3D::dev->CreateTexture(512,512,1,D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &entry.texture, 0);
		textures[address] = entry;
		tex = entry.texture;
	}
	LPDIRECT3DSURFACE9 srcSurface,destSurface;
	tex->GetSurfaceLevel(0,&destSurface);
	srcSurface = D3D::GetBackBufferSurface();
	D3D::dev->StretchRect(srcSurface,source,destSurface,0,D3DTEXF_NONE);
	destSurface->Release();
}
