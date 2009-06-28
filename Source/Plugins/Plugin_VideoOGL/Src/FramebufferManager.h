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

#ifndef _FRAMEBUFFERMANAGER_H_
#define _FRAMEBUFFERMANAGER_H_

#include "GLUtil.h"

class FramebufferManager
{

public:

	FramebufferManager() :
		m_efbFramebuffer(0),
		m_efbColor(0),
		m_efbDepth(0),
		m_resolvedFramebuffer(0),
		m_resolvedColorTexture(0),
		m_resolvedDepthTexture(0),
		m_xfbFramebuffer(0)
	{}

	void Init(int targetWidth, int targetHeight, int msaaSamples, int msaaCoverageSamples);
	void Shutdown();

	// To get the EFB in texture form, these functions may have to transfer
	// the EFB to a resolved texture first.
	GLuint GetEFBColorTexture(const TRectangle& sourceRc) const;
	GLuint GetEFBDepthTexture(const TRectangle& sourceRc) const;

	GLuint GetEFBFramebuffer() const { return m_efbFramebuffer; }

private:

	int m_targetWidth;
	int m_targetHeight;
	int m_msaaSamples;
	int m_msaaCoverageSamples;

	GLuint m_efbFramebuffer;
	GLuint m_efbColor; // Renderbuffer in MSAA mode; Texture otherwise
	GLuint m_efbDepth; // Renderbuffer in MSAA mode; Texture otherwise

	// Only used in MSAA mode.
	GLuint m_resolvedFramebuffer;
	GLuint m_resolvedColorTexture;
	GLuint m_resolvedDepthTexture;

	GLuint m_xfbFramebuffer; // Only used in MSAA mode

};

#endif
