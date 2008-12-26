// Copyright (C) 2003-2008 Dolphin Project.

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

#include "Globals.h"

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

#include "Config.h"
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

u8 *TextureMngr::temp = NULL;
TextureMngr::TexCache TextureMngr::textures;
std::map<u32, TextureMngr::DEPTHTARGET> TextureMngr::mapDepthTargets;
int TextureMngr::nTex2DEnabled, TextureMngr::nTexRECTEnabled;

extern int frameCount;
static u32 s_TempFramebuffer = 0;

#define TEMP_SIZE (1024*1024*4)
#define TEXTURE_KILL_THRESHOLD 200

const GLint c_MinLinearFilter[8] = {
	GL_NEAREST,
	GL_NEAREST_MIPMAP_NEAREST,
	GL_NEAREST_MIPMAP_LINEAR,
	GL_NEAREST,
	GL_LINEAR,
	GL_LINEAR_MIPMAP_NEAREST,
	GL_LINEAR_MIPMAP_LINEAR,
	GL_LINEAR
};

const GLint c_WrapSettings[4] = {
	GL_CLAMP_TO_EDGE,
	GL_REPEAT,
	GL_MIRRORED_REPEAT,
	GL_REPEAT
};

bool SaveTexture(const char* filename, u32 textarget, u32 tex, int width, int height)
{
    GL_REPORT_ERRORD();
	std::vector<u32> data(width * height);
    glBindTexture(textarget, tex);
    glGetTexImage(textarget, 0, GL_BGRA, GL_UNSIGNED_BYTE, &data[0]);
    GLenum err;
    GL_REPORT_ERROR();
    if (err != GL_NO_ERROR)
	{
        return false;
    }
    return SaveTGA(filename, width, height, &data[0]);
}

void TextureMngr::TCacheEntry::SetTextureParameters(TexMode0 &newmode)
{
    mode = newmode;
    if (isNonPow2) {
        // very limited!
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER,
			            (newmode.mag_filter || g_Config.bForceFiltering) ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER,
			            (g_Config.bForceFiltering || newmode.min_filter >= 4) ? GL_LINEAR : GL_NEAREST);
		if (newmode.wrap_s == 2 || newmode.wrap_t == 2) {
            DEBUG_LOG("cannot support mirrorred repeat mode\n");
		}
		if (newmode.wrap_s == 1 || newmode.wrap_t == 1) {
            DEBUG_LOG("cannot support repeat mode\n");
		}
    }
    else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
			            (newmode.mag_filter || g_Config.bForceFiltering) ? GL_LINEAR : GL_NEAREST);

        if (bHaveMipMaps) {
            int filt = newmode.min_filter;
            if (g_Config.bForceFiltering && newmode.min_filter < 4)
                newmode.min_filter += 4; // take equivalent forced linear
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, c_MinLinearFilter[filt]);
        }
        else
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
			                (g_Config.bForceFiltering || newmode.min_filter >= 4) ? GL_LINEAR : GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, c_WrapSettings[newmode.wrap_s]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, c_WrapSettings[newmode.wrap_t]);
    }
    
    if (g_Config.iMaxAnisotropy >= 1)
    {
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)(1 << g_Config.iMaxAnisotropy));
    }
}

void TextureMngr::TCacheEntry::Destroy()
{
    if (!texture)
        return;
    glDeleteTextures(1, &texture);
    if (!isRenderTarget) {
        if (!g_Config.bSafeTextureCache) {
            u32 *ptr = (u32*)g_VideoInitialize.pGetMemoryPointer(addr + hashoffset * 4);
            if (*ptr == hash)
                *ptr = oldpixel;
        }
    }
    texture = 0;
} 

void TextureMngr::Init()
{
    temp = (u8*)AllocateMemoryPages(TEMP_SIZE);
    nTex2DEnabled = nTexRECTEnabled = 0;
	TexDecoder_SetTexFmtOverlayOptions(g_Config.bTexFmtOverlayEnable, g_Config.bTexFmtOverlayCenter);
}

void TextureMngr::Invalidate()
{
    TexCache::iterator iter = textures.begin();
    for (; iter!=textures.end(); iter++)
        iter->second.Destroy();
    textures.clear();
	TexDecoder_SetTexFmtOverlayOptions(g_Config.bTexFmtOverlayEnable, g_Config.bTexFmtOverlayCenter);
}

void TextureMngr::Shutdown()
{
    Invalidate();
    std::map<u32, DEPTHTARGET>::iterator itdepth = mapDepthTargets.begin();
	for (itdepth = mapDepthTargets.begin(); itdepth != mapDepthTargets.end(); ++itdepth) {
		glDeleteRenderbuffersEXT(1, &itdepth->second.targ);
	}
    mapDepthTargets.clear();

    if (s_TempFramebuffer) {
        glDeleteFramebuffersEXT(1, (GLuint *)&s_TempFramebuffer);
        s_TempFramebuffer = 0;
    }

    FreeMemoryPages(temp, TEMP_SIZE);	
    temp = NULL;
}

void TextureMngr::Cleanup()
{
    TexCache::iterator iter = textures.begin();
    while (iter != textures.end()) {
        if (frameCount > TEXTURE_KILL_THRESHOLD + iter->second.frameCount) {
            if (!iter->second.isRenderTarget) {
                iter->second.Destroy();
#ifdef _WIN32
                iter = textures.erase(iter);
#else
				textures.erase(iter++);
#endif
            }
            else {
                iter->second.Destroy();
#ifdef _WIN32
                iter = textures.erase(iter);
#else
				textures.erase(iter++);
#endif
            }
        }
        else
            iter++;
    }

    std::map<u32, DEPTHTARGET>::iterator itdepth = mapDepthTargets.begin();
    while (itdepth != mapDepthTargets.end()) {
        if (frameCount > 20 + itdepth->second.framecount) {
#ifdef _WIN32
            itdepth = mapDepthTargets.erase(itdepth);
#else
            mapDepthTargets.erase(itdepth++);
#endif
        }
        else ++itdepth;
    }
}

TextureMngr::TCacheEntry* TextureMngr::Load(int texstage, u32 address, int width, int height, int format, int tlutaddr, int tlutfmt)
{
    if (address == 0)
        return NULL;

    TexCache::iterator iter = textures.find(address);
    TexMode0 &tm0 = bpmem.tex[texstage > 3].texMode0[texstage & 3];
    u8 *ptr = g_VideoInitialize.pGetMemoryPointer(address);

	// Needed for texture format using tlut.
	// TODO: Slower == Safer. Recalculate tlut lenght for each cases just to be sure.
	u32 hashseed = 0;
	switch (format) {
	case GX_TF_C4:
		hashseed = TexDecoder_GetTlutHash((u16*)(texMem + tlutaddr), 128);
		break;
	case GX_TF_C8:
		hashseed = TexDecoder_GetTlutHash((u16*)(texMem + tlutaddr), 256);
		break;
	case GX_TF_C14X2:
		hashseed = TexDecoder_GetTlutHash((u16*)(texMem + tlutaddr), 384);
		break;
	}

	int palSize = TexDecoder_GetPaletteSize(format);
    u32 palhash = 0xc0debabe;
    
    if (palSize) {
        if (palSize > 32) 
            palSize = 32; //let's not do excessive amount of checking
        u8 *pal = g_VideoInitialize.pGetMemoryPointer(tlutaddr);
        if (pal != 0) {
            for (int i = 0; i < palSize; i++) {
                palhash = _rotl(palhash,13);
                palhash ^= pal[i];
                palhash += 31;
            }
        }
    }

	int bs = TexDecoder_GetBlockWidthInTexels(format) - 1;
    int expandedWidth = (width + bs) & (~bs);

	bool skip_texture_create = false;

    if (iter != textures.end()) {
        TCacheEntry &entry = iter->second;

		u32 hash_value;
		if (g_Config.bSafeTextureCache)
			hash_value = TexDecoder_GetSafeTextureHash(ptr, expandedWidth, height, format, hashseed);
		else
			hash_value = ((u32 *)ptr)[entry.hashoffset];

        if (entry.isRenderTarget || (hash_value == entry.hash && palhash == entry.paletteHash)) {
            entry.frameCount = frameCount;
            //glEnable(entry.isNonPow2?GL_TEXTURE_RECTANGLE_ARB:GL_TEXTURE_2D);
            glBindTexture(entry.isNonPow2 ? GL_TEXTURE_RECTANGLE_ARB:GL_TEXTURE_2D, entry.texture);
            if (entry.mode.hex != tm0.hex)
                entry.SetTextureParameters(tm0);
            return &entry;
        }
        else
        {
            // Let's reload the new texture data into the same texture,
			// instead of destroying it and having to create a new one.
			// Might speed up movie playback very, very slightly.
			if (width == entry.w && height == entry.h && format == entry.fmt)
            {
				glBindTexture(entry.isNonPow2 ? GL_TEXTURE_RECTANGLE_ARB : GL_TEXTURE_2D, entry.texture);
				if (entry.mode.hex != tm0.hex)
					entry.SetTextureParameters(tm0);
				skip_texture_create = true;
            }
            else
            {
                entry.Destroy();
                textures.erase(iter);
            }
        }
    }
    
    PC_TexFormat dfmt = TexDecoder_Decode(temp, ptr, expandedWidth, height, format, tlutaddr, tlutfmt);

    //Make an entry in the table
    TCacheEntry& entry = textures[address];

	entry.hashoffset = 0;
    entry.paletteHash = palhash;
    entry.oldpixel = ((u32 *)ptr)[entry.hashoffset];
	if (g_Config.bSafeTextureCache) {
		entry.hash = TexDecoder_GetSafeTextureHash(ptr, expandedWidth, height, format, hashseed);
	} else {
		entry.hash = (u32)(((double)rand() / RAND_MAX) * 0xFFFFFFFF);
	    ((u32 *)ptr)[entry.hashoffset] = entry.hash;
	}

    entry.addr = address;
    entry.isRenderTarget = false;
    entry.isNonPow2 = ((width & (width - 1)) || (height & (height - 1)));

	GLenum target = entry.isNonPow2 ? GL_TEXTURE_RECTANGLE_ARB : GL_TEXTURE_2D;
	if (!skip_texture_create) {
		glGenTextures(1, (GLuint *)&entry.texture);
		glBindTexture(target, entry.texture);
	}

    if (expandedWidth != width)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, expandedWidth);

	int gl_format;
	int gl_type;
	switch (dfmt) {
	case PC_TEX_FMT_NONE:
		PanicAlert("Invalid PC texture format %i", dfmt); 
	case PC_TEX_FMT_BGRA32:
		gl_format = GL_BGRA;
		gl_type = GL_UNSIGNED_BYTE;
		break;
	}
    if (!entry.isNonPow2 && ((tm0.min_filter & 3) == 1 || (tm0.min_filter & 3) == 2)) {
        gluBuild2DMipmaps(GL_TEXTURE_2D, 4, width, height, gl_format, gl_type, temp);
        entry.bHaveMipMaps = true;
    }
    else
        glTexImage2D(target, 0, 4, width, height, 0, gl_format, gl_type, temp);

    if (expandedWidth != width) // reset
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    entry.frameCount = frameCount;
    entry.w = width;
    entry.h = height;
    entry.fmt = format;
    entry.SetTextureParameters(tm0);

    if (g_Config.bDumpTextures) { // dump texture to file
        static int counter = 0;
        char szTemp[MAX_PATH];
		sprintf(szTemp, "%s/txt_%04i_%i.tga", g_Config.texDumpPath, counter++, format);
        
        SaveTexture(szTemp,target, entry.texture, width, height);
    }

    INCSTAT(stats.numTexturesCreated);
    SETSTAT(stats.numTexturesAlive, textures.size());

    //glEnable(entry.isNonPow2?GL_TEXTURE_RECTANGLE_ARB:GL_TEXTURE_2D);

    //SaveTexture("tex.tga", target, entry.texture, entry.w, entry.h);
    return &entry;
}

void TextureMngr::CopyRenderTargetToTexture(u32 address, bool bFromZBuffer, bool bIsIntensityFmt, u32 copyfmt, bool bScaleByHalf, TRectangle *source)
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
    bool bIsInit = textures.find(address) != textures.end();

    PRIM_LOG("copytarg: addr=0x%x, fromz=%d, intfmt=%d, copyfmt=%d\n", address, (int)bFromZBuffer,(int)bIsIntensityFmt,copyfmt);

    TCacheEntry& entry = textures[address];
    entry.isNonPow2 = true;
    entry.hash = 0;
    entry.hashoffset = 0;
    entry.frameCount = frameCount;
    
    int mult = bScaleByHalf?2:1;
	int w = (abs(source->right-source->left)/mult);
    int h = (abs(source->bottom-source->top)/mult);

    GL_REPORT_ERRORD();

    if (!bIsInit) {
        glGenTextures(1, (GLuint *)&entry.texture);
        glBindTexture(GL_TEXTURE_RECTANGLE_ARB, entry.texture);
        glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        GL_REPORT_ERRORD();
    }
    else {
        _assert_(entry.texture);
        bool bReInit = true;

        if (entry.w == w && entry.h == h) {
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, entry.texture);
            // for some reason mario sunshine errors here...
            GLenum err = GL_NO_ERROR;
            GL_REPORT_ERROR();
            if (err == GL_NO_ERROR )
                bReInit = false;
        }

        if (bReInit) {
            // necessary, for some reason opengl gives errors when texture isn't deleted
            glDeleteTextures(1,(GLuint *)&entry.texture);
            glGenTextures(1, (GLuint *)&entry.texture);
            glBindTexture(GL_TEXTURE_RECTANGLE_ARB, entry.texture);
            glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            GL_REPORT_ERRORD();
        }
    }

    if (!bIsInit || !entry.isRenderTarget) {
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (glGetError() != GL_NO_ERROR) {
            glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
            GL_REPORT_ERRORD();
        }
    }
    
    entry.w = w;
    entry.h = h;
    entry.isRenderTarget = true;
    entry.fmt = copyfmt;

    float colmat[16];
    float fConstAdd[4] = {0};
    memset(colmat, 0, sizeof(colmat));

    if (bFromZBuffer) {
        switch(copyfmt) {
            case 0: // Z4
            case 1: // Z8
                colmat[2] = colmat[6] = colmat[10] = colmat[14] = 1;
                break;
            
            case 3: // Z16 //?
            case 11: // Z16
                colmat[1] = colmat[5] = colmat[9] = colmat[14] = 1;
                break;
            case 6: // Z24X8
                colmat[0] = 1;
                colmat[5] = 1;
                colmat[10] = 1;
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
                ERROR_LOG("Unknown copy zbuf format: 0x%x\n", copyfmt);
                colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
                break;
        }
    }
    else if (bIsIntensityFmt) {
        fConstAdd[0] = fConstAdd[1] = fConstAdd[2] = 16.0f/255.0f;
        switch(copyfmt) {
            case 0: // I4
            case 1: // I8
            case 2: // IA4
            case 3: // IA8
				// TODO - verify these coefficients
                colmat[0] = 0.257f; colmat[1] = 0.504f; colmat[2] = 0.098f;
                colmat[4] = 0.257f; colmat[5] = 0.504f; colmat[6] = 0.098f;
                colmat[8] = 0.257f; colmat[9] = 0.504f; colmat[10] = 0.098f;
                if (copyfmt < 2) {
                    fConstAdd[3] = 16.0f / 255.0f;
                    colmat[12] = 0.257f; colmat[13] = 0.504f; colmat[14] = 0.098f;
                }
                else { // alpha
                    colmat[15] = 1;
                }
                break;
            default:
                ERROR_LOG("Unknown copy intensity format: 0x%x\n", copyfmt);
                colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
                break;
        }
    }
    else {
        switch (copyfmt) {
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
                ERROR_LOG("Unknown copy color format: 0x%x\n", copyfmt);
                colmat[0] = colmat[5] = colmat[10] = colmat[15] = 1;
                break;
        }
    }

//    if (bCopyToTarget) {
//        _assert_( glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT );
//        glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
//        GL_REPORT_ERRORD();
//        glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 0, 0, source->left, source->top, source->right-source->left, source->bottom-source->top);
//        entry.isUpsideDown = true; // note that the copy is upside down!!
//        GL_REPORT_ERRORD(); 
//        return;
//    }
    
    Renderer::SetRenderMode(Renderer::RM_Normal); // set back to normal
    GL_REPORT_ERRORD();

    // have to run a pixel shader

    Renderer::ResetGLState(); // reset any game specific settings

    if (s_TempFramebuffer == 0 )
        glGenFramebuffersEXT( 1, (GLuint *)&s_TempFramebuffer);

    Renderer::SetFramebuffer(s_TempFramebuffer);
    Renderer::SetRenderTarget(entry.texture);
    GL_REPORT_ERRORD();

    // create and attach the render target
    std::map<u32, DEPTHTARGET>::iterator itdepth = mapDepthTargets.find((h << 16) | w);
    
    if (itdepth == mapDepthTargets.end()) {
        DEPTHTARGET& depth = mapDepthTargets[(h << 16) | w];
        depth.framecount = frameCount;
        glGenRenderbuffersEXT(1, &depth.targ);
        glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depth.targ);
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT/*GL_DEPTH24_STENCIL8_EXT*/, w, h);
        GL_REPORT_ERRORD();
        Renderer::SetDepthTarget(depth.targ);
        GL_REPORT_ERRORD();
    }
    else {
        itdepth->second.framecount = frameCount;
        Renderer::SetDepthTarget(itdepth->second.targ);
        GL_REPORT_ERRORD();
    }

    glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, bFromZBuffer?Renderer::GetZBufferTarget():Renderer::GetRenderTarget());    
    TextureMngr::EnableTexRECT(0);
    
    glViewport(0, 0, w, h);

    glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, PixelShaderCache::GetColorMatrixProgram());
    PixelShaderManager::SetColorMatrix(colmat, fConstAdd); // set transformation
    GL_REPORT_ERRORD();
    
    glBegin(GL_QUADS);
    float MValueX = OpenGL_GetXmax();
    float MValueY = OpenGL_GetYmax();
    glTexCoord2f((float)source->left * MValueX, Renderer::GetTargetHeight()-(float)source->bottom * MValueY); glVertex2f(-1,1);
    glTexCoord2f((float)source->left * MValueX, Renderer::GetTargetHeight()-(float)source->top * MValueY); glVertex2f(-1,-1);
    glTexCoord2f((float)source->right * MValueX, Renderer::GetTargetHeight()-(float)source->top * MValueY); glVertex2f(1,-1);
    glTexCoord2f((float)source->right * MValueX, Renderer::GetTargetHeight()-(float)source->bottom * MValueY); glVertex2f(1,1);
    glEnd();

    GL_REPORT_ERRORD();

    Renderer::SetFramebuffer(0);
    Renderer::RestoreGLState();
    VertexShaderManager::SetViewportChanged();

    TextureMngr::DisableStage(0);

    if (bFromZBuffer )
        Renderer::SetZBufferRender(); // notify for future settings

    GL_REPORT_ERRORD();
    //SaveTexture("frame.tga", GL_TEXTURE_RECTANGLE_ARB, entry.texture, entry.w, entry.h);
    //SaveTexture("tex.tga", GL_TEXTURE_RECTANGLE_ARB, Renderer::GetZBufferTarget(), Renderer::GetTargetWidth(), Renderer::GetTargetHeight());
}

void TextureMngr::EnableTex2D(int stage)
{
    if (!(nTex2DEnabled & (1<<stage))) {
        nTex2DEnabled |= (1<<stage);
        glEnable(GL_TEXTURE_2D);
    }
    if (nTexRECTEnabled & (1<<stage)) {
        nTexRECTEnabled &= ~(1<<stage);
        glDisable(GL_TEXTURE_RECTANGLE_ARB);
    }
}

void TextureMngr::EnableTexRECT(int stage)
{
    if ((nTex2DEnabled & (1 << stage))) {
        nTex2DEnabled &= ~(1 << stage);
        glDisable(GL_TEXTURE_2D);
    }
    if (!(nTexRECTEnabled & (1 << stage))) {
        nTexRECTEnabled |= (1 << stage);
        glEnable(GL_TEXTURE_RECTANGLE_ARB);
    }
}

void TextureMngr::DisableStage(int stage)
{
    bool bset = false;
    if (nTex2DEnabled & (1 << stage)) {
        nTex2DEnabled &= ~(1 << stage);
        glActiveTexture(GL_TEXTURE0 + stage);
        glDisable(GL_TEXTURE_2D);
        bset = true;
    }
    if (nTexRECTEnabled & (1<<stage)) {
        nTexRECTEnabled &= ~(1 << stage);
        if (!bset)
			glActiveTexture(GL_TEXTURE0 + stage);
        glDisable(GL_TEXTURE_RECTANGLE_ARB);
    }
}
