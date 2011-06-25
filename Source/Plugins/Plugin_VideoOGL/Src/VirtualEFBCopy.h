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

#ifndef _VIDEOOGL_VIRTUALEFBCOPY_H
#define _VIDEOOGL_VIRTUALEFBCOPY_H

#include "TextureCacheBase.h"
#include "VideoCommon.h"
#include "GLUtil.h"

namespace OGL
{

class VirtualEFBCopy : public VirtualEFBCopyBase
{

public:

	~VirtualEFBCopy();

	void Update(u32 dstAddr, unsigned int dstFormat, unsigned int srcFormat,
		const EFBRectangle& srcRect, bool isIntensity, bool scaleByHalf);
	
	GLuint Virtualize(u32 ramAddr, u32 width, u32 height, u32 levels,
		u32 format, u32 tlutAddr, u32 tlutFormat, bool force);

	unsigned int GetRealWidth() const { return m_realW; }
	unsigned int GetRealHeight() const { return m_realH; }
	unsigned int GetVirtWidth() const { return m_texture.width; }
	unsigned int GetVirtHeight() const { return m_texture.height; }

	bool IsDirty() const { return m_dirty; }
	void ResetDirty() { m_dirty = false; }

private:

	void VirtualizeShade(GLuint texSrc, unsigned int srcFormat,
		bool yuva, bool scale,
		const EFBRectangle& srcRect,
		unsigned int virtualW, unsigned int virtualH,
		const float* colorMatrix, const float* colorAdd);

	void EnsureVirtualTexture(GLsizei width, GLsizei height);
	
	// Properties of the "real" texture: width, height, hash of encoded data, etc.
	unsigned int m_realW;
	unsigned int m_realH;
	unsigned int m_dstFormat;
	bool m_dirty;

	struct Texture
	{
		Texture()
			: tex(0)
		{ }

		GLuint tex;
		GLsizei width;
		GLsizei height;
	} m_texture;

};

};

#endif
