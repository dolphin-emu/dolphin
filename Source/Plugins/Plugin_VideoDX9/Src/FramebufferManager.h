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

#ifndef _FRAMEBUFFERMANAGER_D3D_H_
#define _FRAMEBUFFERMANAGER_D3D_H_

#include <list>
#include "D3DBase.h"

namespace FBManager
{

void Create();
void Destroy();

// To get the EFB in texture form, these functions may have to transfer
// the EFB to a resolved texture first.
LPDIRECT3DTEXTURE9 GetEFBColorTexture(const EFBRectangle& sourceRc);
LPDIRECT3DTEXTURE9 GetEFBDepthTexture(const EFBRectangle& sourceRc);

LPDIRECT3DSURFACE9 GetEFBColorRTSurface();
LPDIRECT3DSURFACE9 GetEFBDepthRTSurface();
LPDIRECT3DSURFACE9 GetEFBColorOffScreenRTSurface();
LPDIRECT3DSURFACE9 GetEFBDepthOffScreenRTSurface();
D3DFORMAT GetEFBDepthRTSurfaceFormat();
D3DFORMAT GetEFBColorRTSurfaceFormat();


/*
// Resolved framebuffer is only used in MSAA mode.
LPDIRECT3DTEXTURE9 GetResolvedFramebuffer() const { return m_resolvedFramebuffer; }

TargetRectangle ConvertEFBRectangle(const EFBRectangle& rc) const;

void SetFramebuffer(LPDIRECT3DSURFACE9 surface);

// If in MSAA mode, this will perform a resolve of the specified rectangle, and return the resolve target as a texture ID.
// Thus, this call may be expensive. Don't repeat it unnecessarily.
// If not in MSAA mode, will just return the render target texture ID.
// After calling this, before you render anything else, you MUST bind the framebuffer you want to draw to.
LPDIRECT3DTEXTURE9 ResolveAndGetRenderTarget(const EFBRectangle &rect);

// Same as above but for the depth Target.
// After calling this, before you render anything else, you MUST bind the framebuffer you want to draw to.
LPDIRECT3DTEXTURE9 ResolveAndGetDepthTarget(const EFBRectangle &rect);
*/

};

#endif
