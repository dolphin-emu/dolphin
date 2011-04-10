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

#include "TextureCache.h"

#include "GLUtil.h"
#include "ImageWrite.h"

namespace OGL
{

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
	
	const GLenum err = GL_REPORT_ERROR();
	if (GL_NO_ERROR != err)
	{
		PanicAlert("Can't save texture, GL Error: %s", gluErrorString(err));
		return false;
	}

    return SaveTGA(filename, width, height, &data[0]);
}

}
