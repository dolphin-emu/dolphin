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

// OGL
#include "OGL_Render.h"
#include "OGL_XFB.h"
#include "OGL_FramebufferManager.h"
#include "OGL_TextureConverter.h"

namespace OGL
{

extern bool s_bHaveFramebufferBlit; // comes from Render.cpp

int FramebufferManager::m_targetWidth;
int FramebufferManager::m_targetHeight;
int FramebufferManager::m_msaaSamples;
int FramebufferManager::m_msaaCoverageSamples;

GLuint FramebufferManager::m_efbFramebuffer;
GLuint FramebufferManager::m_efbColor; // Renderbuffer in MSAA mode; Texture otherwise
GLuint FramebufferManager::m_efbDepth; // Renderbuffer in MSAA mode; Texture otherwise

// Only used in MSAA mode.
GLuint FramebufferManager::m_resolvedFramebuffer;
GLuint FramebufferManager::m_resolvedColorTexture;
GLuint FramebufferManager::m_resolvedDepthTexture;

GLuint FramebufferManager::m_xfbFramebuffer; // Only used in MSAA mode
XFBSource FramebufferManager::m_realXFBSource; // Only used in Real XFB mode

FramebufferManager::FramebufferManager(int targetWidth, int targetHeight, int msaaSamples, int msaaCoverageSamples)
{
	//	m_efbFramebuffer(0),
	//	m_efbColor(0),
	//	m_efbDepth(0),
	//	m_resolvedFramebuffer(0),
	//	m_resolvedColorTexture(0),
	//	m_resolvedDepthTexture(0),
	//	m_xfbFramebuffer(0)

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

FramebufferManager::~FramebufferManager()
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
}

XFBSourceBase* FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height)
{
	XFBSource* const xfbs = new XFBSource;
	
	glGenTextures(1, &xfbs->texture);

	return xfbs;
}

void XFBSource::CopyEFB(const TargetRectangle& efbSource)
{
	// Copy EFB to XFB texture

#if 0
	if (m_msaaSamples <= 1)
#else
	if (!s_bHaveFramebufferBlit)
#endif
	{
		// Just copy the EFB directly.

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, FramebufferManager::m_efbFramebuffer);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture);
		glCopyTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, 0, 0, texWidth, texHeight, 0);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	}
	else
	{
		// OpenGL cannot copy directly from a multisampled framebuffer, so use
		// EXT_framebuffer_blit.

		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, FramebufferManager::m_efbFramebuffer);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, FramebufferManager::m_xfbFramebuffer);

		// Bind texture.
		glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
			GL_TEXTURE_RECTANGLE_ARB, texture, 0);

		GL_REPORT_FBO_ERROR();

		glBlitFramebufferEXT(0, 0, texWidth, texHeight, 0, 0, texWidth,
			texHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

		// Unbind texture.
		glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, 0, 0);

		// Return to EFB.
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, FramebufferManager::m_efbFramebuffer);
	}
}

GLuint FramebufferManager::GetEFBColorTexture(const EFBRectangle& sourceRc)
{
	if (m_msaaSamples <= 1)
	{
		return m_efbColor;
	}
	else
	{
		// Transfer the EFB to a resolved texture. EXT_framebuffer_blit is
		// required.

		TargetRectangle targetRc = RendererBase::ConvertEFBRectangle(sourceRc);
		targetRc.ClampLL(0, 0, m_targetWidth, m_targetHeight);

		// Resolve.
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_efbFramebuffer);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_resolvedFramebuffer);
		glBlitFramebufferEXT(
			targetRc.left, targetRc.top, targetRc.right, targetRc.bottom,
			targetRc.left, targetRc.top, targetRc.right, targetRc.bottom,
			GL_COLOR_BUFFER_BIT, GL_NEAREST
			);

		// Return to EFB.
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_efbFramebuffer);

		return m_resolvedColorTexture;
	}
}

GLuint FramebufferManager::GetEFBDepthTexture(const EFBRectangle& sourceRc)
{
	if (m_msaaSamples <= 1)
	{
		return m_efbDepth;
	}
	else
	{
		// Transfer the EFB to a resolved texture. EXT_framebuffer_blit is
		// required.

		TargetRectangle targetRc = RendererBase::ConvertEFBRectangle(sourceRc);
		targetRc.ClampLL(0, 0, m_targetWidth, m_targetHeight);

		// Resolve.
		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_efbFramebuffer);
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_resolvedFramebuffer);
		glBlitFramebufferEXT(
			targetRc.left, targetRc.top, targetRc.right, targetRc.bottom,
			targetRc.left, targetRc.top, targetRc.right, targetRc.bottom,
			GL_DEPTH_BUFFER_BIT, GL_NEAREST
			);

		// Return to EFB.
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_efbFramebuffer);

		return m_resolvedDepthTexture;
	}
}

void FramebufferManager::copyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc)
{
	u8* pXFB = Memory_GetPtr(xfbAddr);
	if (!pXFB)
	{
		WARN_LOG(VIDEO, "Tried to copy to invalid XFB address");
		return;
	}

	XFB_Write(pXFB, sourceRc, fbWidth, fbHeight);
}

const XFBSourceBase** FramebufferManager::getRealXFBSource(u32 xfbAddr, u32 fbWidth, u32 fbHeight, u32 &xfbCount)
{
	xfbCount = 1;

	m_realXFBSource.texWidth = MAX_XFB_WIDTH;
	m_realXFBSource.texHeight = MAX_XFB_HEIGHT;

	m_realXFBSource.srcAddr = xfbAddr;
	m_realXFBSource.srcWidth = fbWidth;
	m_realXFBSource.srcHeight = fbHeight;

	// OpenGL texture coordinates originate at the lower left, which is why
	// sourceRc.top = fbHeight and sourceRc.bottom = 0.
	m_realXFBSource.sourceRc.left = 0;
	m_realXFBSource.sourceRc.top = fbHeight;
	m_realXFBSource.sourceRc.right = fbWidth;
	m_realXFBSource.sourceRc.bottom = 0;

	if (!m_realXFBSource.texture)
	{
		glGenTextures(1, &m_realXFBSource.texture);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, m_realXFBSource.texture);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, MAX_XFB_WIDTH, MAX_XFB_HEIGHT, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	}

	// Decode YUYV data from GameCube RAM
	TextureConverter::DecodeToTexture(xfbAddr, fbWidth, fbHeight, m_realXFBSource.texture);

	m_overlappingXFBArray[0] = &m_realXFBSource;

	return &m_overlappingXFBArray[0];
}

void FramebufferManager::SetFramebuffer(GLuint fb)
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fb != 0 ? fb : GetEFBFramebuffer());
}

// Apply AA if enabled
GLuint FramebufferManager::ResolveAndGetRenderTarget(const EFBRectangle &source_rect)
{
	return GetEFBColorTexture(source_rect);
}

GLuint FramebufferManager::ResolveAndGetDepthTarget(const EFBRectangle &source_rect)
{
	return GetEFBDepthTexture(source_rect);
}

}
