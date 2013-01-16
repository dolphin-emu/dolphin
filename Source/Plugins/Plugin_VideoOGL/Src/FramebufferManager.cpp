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

#include "Globals.h"
#include "FramebufferManager.h"

#include "TextureConverter.h"
#include "Render.h"
#include "HW/Memmap.h"

namespace OGL
{

extern bool s_bHaveFramebufferBlit; // comes from Render.cpp. ugly.

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

FramebufferManager::FramebufferManager(int targetWidth, int targetHeight, int msaaSamples, int msaaCoverageSamples)
{
    m_efbFramebuffer = 0;
    m_efbColor = 0;
    m_efbDepth = 0;
    m_resolvedFramebuffer = 0;
    m_resolvedColorTexture = 0;
    m_resolvedDepthTexture = 0;
    m_xfbFramebuffer = 0;

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

	// EFB framebuffer is currently bound, make sure to clear its alpha value to 1.f
	glViewport(0, 0, m_targetWidth, m_targetHeight);
	glScissor(0, 0, m_targetWidth, m_targetHeight);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClearDepth(1.0);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
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
	glDeleteTextures(2, glObj);
	m_resolvedColorTexture = 0;
	m_resolvedDepthTexture = 0;

	glObj[0] = m_efbColor;
	glObj[1] = m_efbDepth;
	if (m_msaaSamples <= 1)
		glDeleteTextures(2, glObj);
	else
		glDeleteRenderbuffersEXT(2, glObj);
	m_efbColor = 0;
	m_efbDepth = 0;
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

		TargetRectangle targetRc = g_renderer->ConvertEFBRectangle(sourceRc);
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

		TargetRectangle targetRc = g_renderer->ConvertEFBRectangle(sourceRc);
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

void FramebufferManager::CopyToRealXFB(u32 xfbAddr, u32 fbWidth, u32 fbHeight, const EFBRectangle& sourceRc,float Gamma)
{
	u8* xfb_in_ram = Memory::GetPointer(xfbAddr);
	if (!xfb_in_ram)
	{
		WARN_LOG(VIDEO, "Tried to copy to invalid XFB address");
		return;
	}

	TargetRectangle targetRc = g_renderer->ConvertEFBRectangle(sourceRc);
	TextureConverter::EncodeToRamYUYV(ResolveAndGetRenderTarget(sourceRc), targetRc, xfb_in_ram, fbWidth, fbHeight);
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

void XFBSource::Draw(const MathUtil::Rectangle<float> &sourcerc,
		const MathUtil::Rectangle<float> &drawrc, int width, int height) const
{
	// Texture map xfbSource->texture onto the main buffer

	glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture);

	glBegin(GL_QUADS);
	glTexCoord2f(sourcerc.left, sourcerc.bottom);
	glMultiTexCoord2fARB(GL_TEXTURE1, 0, 0);
	glVertex2f(drawrc.left, drawrc.bottom);

	glTexCoord2f(sourcerc.left, sourcerc.top);
	glMultiTexCoord2fARB(GL_TEXTURE1, 0, 1);
	glVertex2f(drawrc.left, drawrc.top);

	glTexCoord2f(sourcerc.right, sourcerc.top);
	glMultiTexCoord2fARB(GL_TEXTURE1, 1, 1);
	glVertex2f(drawrc.right, drawrc.top);

	glTexCoord2f(sourcerc.right, sourcerc.bottom);
	glMultiTexCoord2fARB(GL_TEXTURE1, 1, 0);
	glVertex2f(drawrc.right, drawrc.bottom);
	glEnd();

	GL_REPORT_ERRORD();
}

void XFBSource::DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	TextureConverter::DecodeToTexture(xfbAddr, fbWidth, fbHeight, texture);
}

void XFBSource::CopyEFB(float Gamma)
{
	// Copy EFB data to XFB and restore render target again

#if 0
	if (m_msaaSamples <= 1)
#else
	if (!s_bHaveFramebufferBlit)
#endif
	{
		// Just copy the EFB directly.

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, FramebufferManager::GetEFBFramebuffer());

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture);
		glCopyTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, 0, 0, texWidth, texHeight, 0);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	}
	else
	{
		// OpenGL cannot copy directly from a multisampled framebuffer, so use
		// EXT_framebuffer_blit.

		glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, FramebufferManager::GetEFBFramebuffer());
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, FramebufferManager::GetXFBFramebuffer());

		// Bind texture.
		glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, texture, 0);
		GL_REPORT_FBO_ERROR();

		glBlitFramebufferEXT(
			0, 0, texWidth, texHeight,
			0, 0, texWidth, texHeight,
			GL_COLOR_BUFFER_BIT, GL_NEAREST
			);

		// Unbind texture.
		glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, 0, 0);

		// Return to EFB.
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, FramebufferManager::GetEFBFramebuffer());
	}
}

XFBSourceBase* FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height)
{
	GLuint texture;

	glGenTextures(1, &texture);

#if 0// XXX: Some video drivers don't handle glCopyTexImage2D correctly, so use EXT_framebuffer_blit whenever possible.
	if (m_msaaSamples > 1)
#else
	if (s_bHaveFramebufferBlit)
#endif
	{
		// In MSAA mode, allocate the texture image here. In non-MSAA mode,
		// the image will be allocated by glCopyTexImage2D (later).

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB, 0, 4, target_width, target_height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);

		glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);
	}

	return new XFBSource(texture);
}

void FramebufferManager::GetTargetSize(unsigned int *width, unsigned int *height, const EFBRectangle& sourceRc)
{
	*width = m_targetWidth;
	*height = m_targetHeight;
}

}  // namespace OGL
