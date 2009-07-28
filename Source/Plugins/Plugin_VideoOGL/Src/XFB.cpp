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


// ----------------------------------------------------------------------------------------------------------
// This file handles the External Frame Buffer (XFB). The XFB is a storage point when the picture is resized
// by the system to the correct display format for output to the TV. In most cases its function can be
// supplemented by the equivalent adjustments in glScissor and glViewport (or their DirectX equivalents). But
// for some homebrew games these functions are necessary because the homebrew game communicate directly with
// them.
// ----------------------------------------------------------------------------------------------------------

#include "Globals.h"
#include "XFB.h"
#include "Render.h"
#include "TextureConverter.h"

void XFB_Write(u8 *xfb_in_ram, const EFBRectangle& sourceRc, u32 dstWd, u32 dstHt)
{
	TargetRectangle targetRc = Renderer::ConvertEFBRectangle(sourceRc);
	TextureConverter::EncodeToRamYUYV(Renderer::ResolveAndGetRenderTarget(sourceRc), targetRc, xfb_in_ram, dstWd, dstHt);
}
