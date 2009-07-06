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

#include "TextureConverter.h"
#include "XFB.h"

extern bool s_bHaveFramebufferBlit; // comes from Render.cpp

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

	// Create XFB framebuffer; targets will be created elsewhere.

	glGenFramebuffersEXT(1, &m_xfbFramebuffer);

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
	glObj[2] = m_realXFBSource.texture;
	glDeleteTextures(3, glObj);
	m_resolvedColorTexture = 0;
	m_resolvedDepthTexture = 0;
	m_realXFBSource.texture = 0;

	glObj[0] = m_efbColor;
	glObj[1] = m_efbDepth;
	if (m_msaaSamples <= 1)
		glDeleteTextures(2, glObj);
	else
		glDeleteRenderbuffersEXT(2, glObj);
	m_efbColor = 0;
	m_efbDepth = 0;

	for (VirtualXFBListType::iterator it = m_virtualXFBList.begin(); it != m_virtualXFBList.end(); ++it)
	{
		glDeleteTextures(1, &it->xfbSource.texture);
	}
	m_virtualXFBList.clear();
}

void FramebufferManager::CopyToXFB(u32 xfbAddr, u32 dstWidth, u32 dstHeight, const TRectangle& sourceRc)
{
	if (g_Config.bUseXFB)
		copyToRealXFB(xfbAddr, dstWidth, dstHeight, sourceRc);
	else
		copyToVirtualXFB(xfbAddr, dstWidth, dstHeight, sourceRc);
}

const XFBSource* FramebufferManager::GetXFBSource(u32 xfbAddr, u32 srcWidth, u32 srcHeight)
{
	if (g_Config.bUseXFB)
		return getRealXFBSource(xfbAddr, srcWidth, srcHeight);
	else
		return getVirtualXFBSource(xfbAddr, srcWidth, srcHeight);
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

FramebufferManager::VirtualXFBListType::iterator
FramebufferManager::findVirtualXFB(u32 xfbAddr, u32 width, u32 height)
{
	u32 srcLower = xfbAddr;
	u32 srcUpper = xfbAddr + 2 * width * height;

	VirtualXFBListType::iterator it;
	for (it = m_virtualXFBList.begin(); it != m_virtualXFBList.end(); ++it)
	{
		u32 dstLower = it->xfbAddr;
		u32 dstUpper = it->xfbAddr + 2 * it->xfbWidth * it->xfbHeight;

		if (addrRangesOverlap(srcLower, srcUpper, dstLower, dstUpper))
			return it;
	}

	// That address is not in the Virtual XFB list.
	return m_virtualXFBList.end();
}

void FramebufferManager::copyToRealXFB(u32 xfbAddr, u32 dstWidth, u32 dstHeight, const TRectangle& sourceRc)
{
	u8* pXFB = Memory_GetPtr(xfbAddr);
	if (!pXFB)
	{
		WARN_LOG(VIDEO, "Tried to copy to invalid XFB address");
		return;
	}

	XFB_Write(pXFB, sourceRc, dstWidth, dstHeight);
}

void FramebufferManager::copyToVirtualXFB(u32 xfbAddr, u32 dstWidth, u32 dstHeight, const TRectangle& sourceRc)
{
	GLuint xfbTexture;

	VirtualXFBListType::iterator it = findVirtualXFB(xfbAddr, dstWidth, dstHeight);

	if (it != m_virtualXFBList.end())
	{
		// Overwrite an existing Virtual XFB.

		it->xfbAddr = xfbAddr;
		it->xfbWidth = dstWidth;
		it->xfbHeight = dstHeight;

		it->xfbSource.texWidth = m_targetWidth;
		it->xfbSource.texHeight = m_targetHeight;
		it->xfbSource.sourceRc = sourceRc;

		xfbTexture = it->xfbSource.texture;

		// Move this Virtual XFB to the front of the list.
		m_virtualXFBList.splice(m_virtualXFBList.begin(), m_virtualXFBList, it);
	}
	else
	{
		// Create a new Virtual XFB and place it at the front of the list.

		glGenTextures(1, &xfbTexture);

#if 0 // XXX: Some video drivers don't handle glCopyTexImage2D correctly, so use EXT_framebuffer_blit whenever possible.
		if (m_msaaSamples > 1)
#else
		if (s_bHaveFramebufferBlit)
#endif
		{
			// In MSAA mode, allocate the texture image here. In non-MSAA mode,
			// the image will be allocated by glCopyTexImage2D (later).

			glBindTexture(GL_TEXTURE_RECTANGLE_ARB, xfbTexture);
			glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, m_targetWidth, m_targetHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

			glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
		}

		VirtualXFB newVirt;

		newVirt.xfbAddr = xfbAddr;
		newVirt.xfbWidth = dstWidth;
		newVirt.xfbHeight = dstHeight;

		newVirt.xfbSource.texture = xfbTexture;
		newVirt.xfbSource.texWidth = m_targetWidth;
		newVirt.xfbSource.texHeight = m_targetHeight;
		newVirt.xfbSource.sourceRc = sourceRc;

		// Add the new Virtual XFB to the list

		if (m_virtualXFBList.size() >= MAX_VIRTUAL_XFB)
		{
			// List overflowed; delete the oldest.
			glDeleteTextures(1, &m_virtualXFBList.back().xfbSource.texture);
			m_virtualXFBList.pop_back();
		}

		m_virtualXFBList.push_front(newVirt);
	}

	// Copy EFB to XFB texture

#if 0
	if (m_msaaSamples <= 1)
#else
	if (!s_bHaveFramebufferBlit)
#endif
	{
		// Just copy the EFB directly.

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_efbFramebuffer);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, xfbTexture);
		glCopyTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, 0, 0, m_targetWidth, m_targetHeight, 0);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	}
	else
	{
		// OpenGL cannot copy directly from a multisampled framebuffer, so use
		// EXT_framebuffer_blit.

		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_efbFramebuffer);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_xfbFramebuffer);

		// Bind texture.
		glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, xfbTexture, 0);
		GL_REPORT_FBO_ERROR();

		glBlitFramebufferEXT(
			0, 0, m_targetWidth, m_targetHeight,
			0, 0, m_targetWidth, m_targetHeight,
			GL_COLOR_BUFFER_BIT, GL_NEAREST
			);

		// Unbind texture.
		glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, 0, 0);

		// Return to EFB.
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_efbFramebuffer);
	}
}

const XFBSource* FramebufferManager::getRealXFBSource(u32 xfbAddr, u32 srcWidth, u32 srcHeight)
{
	m_realXFBSource.texWidth = XFB_WIDTH;
	m_realXFBSource.texHeight = XFB_HEIGHT;

	m_realXFBSource.sourceRc.left = 0;
	m_realXFBSource.sourceRc.top = 0;
	m_realXFBSource.sourceRc.right = srcWidth;
	m_realXFBSource.sourceRc.bottom = srcHeight;

	if (!m_realXFBSource.texture)
	{
		glGenTextures(1, &m_realXFBSource.texture);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_realXFBSource.texture);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, XFB_WIDTH, XFB_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	}

	// Decode YUYV data from GameCube RAM
	TextureConverter::DecodeToTexture(xfbAddr, srcWidth, srcHeight, m_realXFBSource.texture);

	return &m_realXFBSource;
}

const XFBSource* FramebufferManager::getVirtualXFBSource(u32 xfbAddr, u32 srcWidth, u32 srcHeight)
{
	if (m_virtualXFBList.size() == 0)
	{
		// No Virtual XFBs available.
		return NULL;
	}

	VirtualXFBListType::iterator it = findVirtualXFB(xfbAddr, srcWidth, srcHeight);
	if (it == m_virtualXFBList.end())
	{
		// Virtual XFB is not in the list, so return the most recently rendered
		// one.
		it = m_virtualXFBList.begin();
	}

	return &it->xfbSource;
}
