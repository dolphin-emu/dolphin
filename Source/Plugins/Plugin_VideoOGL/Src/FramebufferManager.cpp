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

#include "Globals.h"
#include "FramebufferManager.h"

void FramebufferManager::Init(int targetWidth, int targetHeight, int msaaSamples, int msaaCoverageSamples)
{
	m_targetWidth = targetWidth;
	m_targetHeight = targetHeight;
	m_msaaSamples = msaaSamples;
	m_msaaCoverageSamples = msaaCoverageSamples;

	// The EFB can be set to different pixel formats by the game through the
	// BPMEM_ZCOMPARE register (which should probably have a different name).
	// They are:
	// - 24-bit RGB (8-bit components) with 24-bit Z
	// - 24-bit RGBA (6-bit components) with 24-bit Z
	// - Multisampled 16-bit RGB (5-6-5 format) with 16-bit Z
	// We only use one EFB format here: 32-bit ARGB with 24-bit Z.
	// Multisampling depends on user settings.
	// The distinction becomes important for certain operations, i.e. the
	// alpha channel should be ignored if the EFB does not have one.

	// Create EFB target.

	glGenFramebuffersEXT(1, &m_efbFramebuffer);

	if (m_msaaSamples <= 1)
	{
		// EFB targets will be textures in non-MSAA mode.

		GLuint glObj[2];
		glGenTextures(2, glObj);
		m_efbColor = glObj[0];
		m_efbDepth = glObj[1];

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_efbColor);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8, m_targetWidth, m_targetHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_efbDepth);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_DEPTH_COMPONENT24, m_targetWidth, m_targetHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);

		// Bind target textures to the EFB framebuffer.

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_efbFramebuffer);

		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, m_efbColor, 0);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_RECTANGLE_ARB, m_efbDepth, 0);

		GL_REPORT_FBO_ERROR();
	}
	else
	{
		// EFB targets will be renderbuffers in MSAA mode (required by OpenGL).
		// Resolve targets will be created to transfer EFB to RAM textures.
		// XFB framebuffer will be created to transfer EFB to XFB texture.

		// Create EFB target renderbuffers.

		GLuint glObj[2];
		glGenRenderbuffersEXT(2, glObj);
		m_efbColor = glObj[0];
		m_efbDepth = glObj[1];

		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_efbColor);
		if (m_msaaCoverageSamples)
			glRenderbufferStorageMultisampleCoverageNV(GL_RENDERBUFFER_EXT, m_msaaCoverageSamples, m_msaaSamples, GL_RGBA8, m_targetWidth, m_targetHeight);
		else
			glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, m_msaaSamples, GL_RGBA8, m_targetWidth, m_targetHeight);

		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_efbDepth);
		if (m_msaaCoverageSamples)
			glRenderbufferStorageMultisampleCoverageNV(GL_RENDERBUFFER_EXT, m_msaaCoverageSamples, m_msaaSamples, GL_DEPTH_COMPONENT24, m_targetWidth, m_targetHeight);
		else
			glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, m_msaaSamples, GL_DEPTH_COMPONENT24, m_targetWidth, m_targetHeight);

		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);

		// Bind target renderbuffers to EFB framebuffer.

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_efbFramebuffer);

		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, m_efbColor);
		glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_efbDepth);

		GL_REPORT_FBO_ERROR();

		// Create XFB framebuffer; targets will be created elsewhere.

		glGenFramebuffersEXT(1, &m_xfbFramebuffer);

		// Create resolved targets for transferring multisampled EFB to texture.

		glGenFramebuffersEXT(1, &m_resolvedFramebuffer);

		glGenTextures(2, glObj);
		m_resolvedColorTexture = glObj[0];
		m_resolvedDepthTexture = glObj[1];

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_resolvedColorTexture);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA8, m_targetWidth, m_targetHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_resolvedDepthTexture);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, GL_DEPTH_COMPONENT24, m_targetWidth, m_targetHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);

		// Bind resolved textures to resolved framebuffer.

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_resolvedFramebuffer);

		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, m_resolvedColorTexture, 0);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_RECTANGLE_ARB, m_resolvedDepthTexture, 0);

		GL_REPORT_FBO_ERROR();

		// Return to EFB framebuffer.

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_efbFramebuffer);
	}

	// EFB framebuffer is currently bound.
}

void FramebufferManager::Shutdown()
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	GLuint glObj[3];

	// Note: OpenGL deletion functions silently ignore parameters of "0".

	glObj[0] = m_efbFramebuffer;
	glObj[1] = m_resolvedFramebuffer;
	glObj[2] = m_xfbFramebuffer;
	glDeleteFramebuffersEXT(3, glObj);
	m_efbFramebuffer = 0;
	m_xfbFramebuffer = 0;

	glObj[0] = m_resolvedColorTexture;
	glObj[1] = m_resolvedDepthTexture;
	glDeleteTextures(2, glObj);

	glObj[0] = m_efbColor;
	glObj[1] = m_efbDepth;
	if (m_msaaSamples <= 1)
		glDeleteTextures(2, glObj);
	else
		glDeleteRenderbuffersEXT(2, glObj);
	m_efbColor = 0;
	m_efbDepth = 0;
}

GLuint FramebufferManager::GetEFBColorTexture(const TRectangle& sourceRc) const
{
	if (m_msaaSamples <= 1)
	{
		return m_efbColor;
	}
	else
	{
		// Transfer the EFB to a resolved texture. EXT_framebuffer_blit is
		// required.

		// Flip source rectangle upside-down for OpenGL.
		TRectangle glRect;
		sourceRc.FlipYPosition(m_targetHeight, &glRect);
		glRect.Clamp(0, 0, m_targetWidth, m_targetHeight);

		// Resolve.
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_efbFramebuffer);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_resolvedFramebuffer);
		glBlitFramebufferEXT(
			glRect.left, glRect.top, glRect.right, glRect.bottom,
			glRect.left, glRect.top, glRect.right, glRect.bottom,
			GL_COLOR_BUFFER_BIT, GL_NEAREST
			);

		// Return to EFB.
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_efbFramebuffer);

		return m_resolvedColorTexture;
	}
}

GLuint FramebufferManager::GetEFBDepthTexture(const TRectangle& sourceRc) const
{
	if (m_msaaSamples <= 1)
	{
		return m_efbDepth;
	}
	else
	{
		// Transfer the EFB to a resolved texture. EXT_framebuffer_blit is
		// required.

		// Flip source rectangle upside-down for OpenGL.
		TRectangle glRect;
		sourceRc.FlipYPosition(m_targetHeight, &glRect);
		glRect.Clamp(0, 0, m_targetWidth, m_targetHeight);

		// Resolve.
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_efbFramebuffer);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_resolvedFramebuffer);
		glBlitFramebufferEXT(
			glRect.left, glRect.top, glRect.right, glRect.bottom,
			glRect.left, glRect.top, glRect.right, glRect.bottom,
			GL_DEPTH_BUFFER_BIT, GL_NEAREST
			);

		// Return to EFB.
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_efbFramebuffer);

		return m_resolvedDepthTexture;
	}
}
