// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _TEXTUREMNGR_H_
#define _TEXTUREMNGR_H_

#include <map>

#include "VideoCommon.h"
#include "GLUtil.h"
#include "BPStructs.h"

class TextureMngr
{
public:
    struct TCacheEntry
    {
        TCacheEntry() : texture(0), addr(0), size_in_bytes(0), hash(0), w(0), h(0), scaleX(1.0f), scaleY(1.0f), isRenderTarget(false), isUpsideDown(false), isNonPow2(true), bHaveMipMaps(false) { mode.hex = 0xFCFCFCFC; }

        GLuint texture;
        u32 addr;
        u32 size_in_bytes;
        u32 hash;
        u32 paletteHash;
        u32 hashoffset;
        u32 oldpixel; // used for simple cleanup
        TexMode0 mode; // current filter and clamp modes that texture is set to

        int frameCount;
        int w, h, fmt;

		float scaleX, scaleY; // Hires texutres need this

        bool isRenderTarget; // if render texture, then rendertex is filled with the direct copy of the render target
                             // later conversions would have to convert properly from rendertexfmt to texfmt
        bool isUpsideDown; 
        bool isNonPow2; // if nonpow2, use GL_TEXTURE_2D, else GL_TEXTURE_RECTANGLE_NV
        bool bHaveMipMaps;

        void SetTextureParameters(TexMode0& newmode);
        void Destroy(bool shutdown);
        void ConvertFromRenderTarget(u32 taddr, int twidth, int theight, int tformat, int tlutaddr, int tlutfmt);
		bool IntersectsMemoryRange(u32 range_address, u32 range_size);
    };

private:
    typedef std::map<u32, TCacheEntry> TexCache;

    static u8 *temp;
    static TexCache textures;

public:
    static void Init();
    static void ProgressiveCleanup();
    static void Shutdown();
    static void Invalidate(bool shutdown);
	static void InvalidateRange(u32 start_address, u32 size);

    static TCacheEntry* Load(int texstage, u32 address, int width, int height, int format, int tlutaddr, int tlutfmt);
    static void CopyRenderTargetToTexture(u32 address, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, bool bScaleByHalf, const EFBRectangle &source);

    static void DisableStage(int stage); // sets active texture

	static void ClearRenderTargets(); // sets render target value of all textures to false
};

bool SaveTexture(const char* filename, u32 textarget, u32 tex, int width, int height);

#endif // _TEXTUREMNGR_H_
