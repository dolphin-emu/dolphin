// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonFuncs.h"
#include "Core/HW/Memmap.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/Render.h"
#include "VideoBackends/OGL/TextureConverter.h"

#include "VideoCommon/DriverDetails.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/VertexShaderGen.h"

namespace OGL
{

int FramebufferManager::m_targetWidth;
int FramebufferManager::m_targetHeight;
int FramebufferManager::m_msaaSamples;

GLenum FramebufferManager::m_textureType;
int FramebufferManager::m_EFBLayers;

GLuint FramebufferManager::m_efbFramebuffer;
GLuint FramebufferManager::m_xfbFramebuffer;
GLuint FramebufferManager::m_efbColor;
GLuint FramebufferManager::m_efbDepth;
GLuint FramebufferManager::m_efbColorSwap; // for hot swap when reinterpreting EFB pixel formats

// Only used in MSAA mode.
GLuint FramebufferManager::m_resolvedFramebuffer;
GLuint FramebufferManager::m_resolvedColorTexture;
GLuint FramebufferManager::m_resolvedDepthTexture;

// reinterpret pixel format
SHADER FramebufferManager::m_pixel_format_shaders[2];


FramebufferManager::FramebufferManager(int targetWidth, int targetHeight, int msaaSamples)
{
	m_efbFramebuffer = 0;
	m_xfbFramebuffer = 0;
	m_efbColor = 0;
	m_efbDepth = 0;
	m_efbColorSwap = 0;
	m_resolvedFramebuffer = 0;
	m_resolvedColorTexture = 0;
	m_resolvedDepthTexture = 0;

	m_targetWidth = targetWidth;
	m_targetHeight = targetHeight;

	m_msaaSamples = msaaSamples;

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

	glActiveTexture(GL_TEXTURE0 + 9);

	GLuint glObj[3];
	glGenTextures(3, glObj);
	m_efbColor = glObj[0];
	m_efbDepth = glObj[1];
	m_efbColorSwap = glObj[2];

	m_EFBLayers = (g_ActiveConfig.iStereoMode > 0) ? 2 : 1;

	// OpenGL MSAA textures are a different kind of texture type and must be allocated
	// with a different function, so we create them separately.
	if (m_msaaSamples <= 1)
	{
		m_textureType = GL_TEXTURE_2D_ARRAY;

		glBindTexture(m_textureType, m_efbColor);
		glTexParameteri(m_textureType, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(m_textureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(m_textureType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage3D(m_textureType, 0, GL_RGBA, m_targetWidth, m_targetHeight, m_EFBLayers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

		glBindTexture(m_textureType, m_efbDepth);
		glTexParameteri(m_textureType, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(m_textureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(m_textureType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage3D(m_textureType, 0, GL_DEPTH_COMPONENT24, m_targetWidth, m_targetHeight, m_EFBLayers, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

		glBindTexture(m_textureType, m_efbColorSwap);
		glTexParameteri(m_textureType, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(m_textureType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(m_textureType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage3D(m_textureType, 0, GL_RGBA, m_targetWidth, m_targetHeight, m_EFBLayers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	}
	else
	{
		m_textureType = GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
		GLenum resolvedType = GL_TEXTURE_2D_ARRAY;

		glBindTexture(m_textureType, m_efbColor);
		glTexImage3DMultisample(m_textureType, m_msaaSamples, GL_RGBA, m_targetWidth, m_targetHeight, m_EFBLayers, false);

		glBindTexture(m_textureType, m_efbDepth);
		glTexImage3DMultisample(m_textureType, m_msaaSamples, GL_DEPTH_COMPONENT24, m_targetWidth, m_targetHeight, m_EFBLayers, false);

		glBindTexture(m_textureType, m_efbColorSwap);
		glTexImage3DMultisample(m_textureType, m_msaaSamples, GL_RGBA, m_targetWidth, m_targetHeight, m_EFBLayers, false);
		glBindTexture(m_textureType, 0);

		// Although we are able to access the multisampled texture directly, we don't do it everywhere.
		// The old way is to "resolve" this multisampled texture by copying it into a non-sampled texture.
		// This would lead to an unneeded copy of the EFB, so we are going to avoid it.
		// But as this job isn't done right now, we do need that texture for resolving:
		glGenTextures(2, glObj);
		m_resolvedColorTexture = glObj[0];
		m_resolvedDepthTexture = glObj[1];

		glBindTexture(resolvedType, m_resolvedColorTexture);
		glTexParameteri(resolvedType, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(resolvedType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(resolvedType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage3D(resolvedType, 0, GL_RGBA, m_targetWidth, m_targetHeight, m_EFBLayers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

		glBindTexture(resolvedType, m_resolvedDepthTexture);
		glTexParameteri(resolvedType, GL_TEXTURE_MAX_LEVEL, 0);
		glTexParameteri(resolvedType, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(resolvedType, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage3D(resolvedType, 0, GL_DEPTH_COMPONENT24, m_targetWidth, m_targetHeight, m_EFBLayers, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);

		// Bind resolved textures to resolved framebuffer.
		glGenFramebuffers(1, &m_resolvedFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, m_resolvedFramebuffer);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_resolvedColorTexture, 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_resolvedDepthTexture, 0);
	}

	// Create XFB framebuffer; targets will be created elsewhere.
	glGenFramebuffers(1, &m_xfbFramebuffer);

	// Bind target textures to EFB framebuffer.
	glGenFramebuffers(1, &m_efbFramebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, m_efbFramebuffer);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_efbColor, 0);
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_efbDepth, 0);

	// EFB framebuffer is currently bound, make sure to clear its alpha value to 1.f
	glViewport(0, 0, m_targetWidth, m_targetHeight);
	glScissor(0, 0, m_targetWidth, m_targetHeight);
	glClearColor(0.f, 0.f, 0.f, 1.f);
	glClearDepthf(1.0f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

	// reinterpret pixel format
	char vs[] =
		"void main(void) {\n"
		"	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);\n"
		"	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);\n"
		"}\n";

	// The way to sample the EFB is based on the on the current configuration.
	// As we use the same sampling way for both interpreting shaders, the sampling
	// shader are generated first:
	std::string sampler;
	if (m_msaaSamples <= 1)
	{
		// non-msaa, so just fetch the pixel
		sampler =
			"SAMPLER_BINDING(9) uniform sampler2DArray samp9;\n"
			"vec4 sampleEFB(ivec2 pos) {\n"
			"	return texelFetch(samp9, ivec3(pos, 0), 0);\n"
			"}\n";
	}
	else if (g_ogl_config.bSupportSampleShading)
	{
		// msaa + sample shading available, so just fetch the sample
		// This will lead to sample shading, but it's the only way to not loose
		// the values of each sample.
		sampler =
			"SAMPLER_BINDING(9) uniform sampler2DMSArray samp9;\n"
			"vec4 sampleEFB(ivec2 pos) {\n"
			"	return texelFetch(samp9, ivec3(pos, 0), gl_SampleID);\n"
			"}\n";
	}
	else
	{
		// msaa without sample shading: calculate the mean value of the pixel
		std::stringstream samples;
		samples << m_msaaSamples;
		sampler =
			"SAMPLER_BINDING(9) uniform sampler2DMSArray samp9;\n"
			"vec4 sampleEFB(ivec2 pos) {\n"
			"	vec4 color = vec4(0.0, 0.0, 0.0, 0.0);\n"
			"	for(int i=0; i<" + samples.str() + "; i++)\n"
			"		color += texelFetch(samp9, ivec3(pos, 0), i);\n"
			"	return color / " + samples.str() + ";\n"
			"}\n";
	}

	std::string ps_rgba6_to_rgb8 = sampler +
		"out vec4 ocol0;\n"
		"void main()\n"
		"{\n"
		"	ivec4 src6 = ivec4(round(sampleEFB(ivec2(gl_FragCoord.xy)) * 63.f));\n"
		"	ivec4 dst8;\n"
		"	dst8.r = (src6.r << 2) | (src6.g >> 4);\n"
		"	dst8.g = ((src6.g & 0xF) << 4) | (src6.b >> 2);\n"
		"	dst8.b = ((src6.b & 0x3) << 6) | src6.a;\n"
		"	dst8.a = 255;\n"
		"	ocol0 = float4(dst8) / 255.f;\n"
		"}";

	std::string ps_rgb8_to_rgba6 = sampler +
		"out vec4 ocol0;\n"
		"void main()\n"
		"{\n"
		"	ivec4 src8 = ivec4(round(sampleEFB(ivec2(gl_FragCoord.xy)) * 255.f));\n"
		"	ivec4 dst6;\n"
		"	dst6.r = src8.r >> 2;\n"
		"	dst6.g = ((src8.r & 0x3) << 4) | (src8.g >> 4);\n"
		"	dst6.b = ((src8.g & 0xF) << 2) | (src8.b >> 6);\n"
		"	dst6.a = src8.b & 0x3F;\n"
		"	ocol0 = float4(dst6) / 63.f;\n"
		"}";

	ProgramShaderCache::CompileShader(m_pixel_format_shaders[0], vs, ps_rgb8_to_rgba6.c_str());
	ProgramShaderCache::CompileShader(m_pixel_format_shaders[1], vs, ps_rgba6_to_rgb8.c_str());
}

FramebufferManager::~FramebufferManager()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	GLuint glObj[3];

	// Note: OpenGL deletion functions silently ignore parameters of "0".

	glObj[0] = m_efbFramebuffer;
	glObj[1] = m_xfbFramebuffer;
	glObj[2] = m_resolvedFramebuffer;
	glDeleteFramebuffers(3, glObj);
	m_efbFramebuffer = 0;
	m_xfbFramebuffer = 0;
	m_resolvedFramebuffer = 0;

	glObj[0] = m_resolvedColorTexture;
	glObj[1] = m_resolvedDepthTexture;
	glDeleteTextures(2, glObj);
	m_resolvedColorTexture = 0;
	m_resolvedDepthTexture = 0;

	glObj[0] = m_efbColor;
	glObj[1] = m_efbDepth;
	glObj[2] = m_efbColorSwap;
	glDeleteTextures(3, glObj);
	m_efbColor = 0;
	m_efbDepth = 0;
	m_efbColorSwap = 0;

	// reinterpret pixel format
	m_pixel_format_shaders[0].Destroy();
	m_pixel_format_shaders[1].Destroy();
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
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_efbFramebuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_resolvedFramebuffer);
		glBlitFramebuffer(
			targetRc.left, targetRc.top, targetRc.right, targetRc.bottom,
			targetRc.left, targetRc.top, targetRc.right, targetRc.bottom,
			GL_COLOR_BUFFER_BIT, GL_NEAREST
			);

		// Return to EFB.
		glBindFramebuffer(GL_FRAMEBUFFER, m_efbFramebuffer);

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
		// Transfer the EFB to a resolved texture.

		TargetRectangle targetRc = g_renderer->ConvertEFBRectangle(sourceRc);
		targetRc.ClampLL(0, 0, m_targetWidth, m_targetHeight);

		// Resolve.
		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_efbFramebuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_resolvedFramebuffer);
		glBlitFramebuffer(
			targetRc.left, targetRc.top, targetRc.right, targetRc.bottom,
			targetRc.left, targetRc.top, targetRc.right, targetRc.bottom,
			GL_DEPTH_BUFFER_BIT, GL_NEAREST
			);

		// Return to EFB.
		glBindFramebuffer(GL_FRAMEBUFFER, m_efbFramebuffer);

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
	glBindFramebuffer(GL_FRAMEBUFFER, fb != 0 ? fb : GetEFBFramebuffer());
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

void FramebufferManager::ReinterpretPixelData(unsigned int convtype)
{
	g_renderer->ResetAPIState();

	GLuint src_texture = 0;

	// We aren't allowed to render and sample the same texture in one draw call,
	// so we have to create a new texture and overwrite it completely.
	// To not allocate one big texture every time, we've allocated two on
	// initialization and just swap them here:
	src_texture = m_efbColor;
	m_efbColor = m_efbColorSwap;
	m_efbColorSwap = src_texture;
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_efbColor, 0);

	glViewport(0,0, m_targetWidth, m_targetHeight);
	glActiveTexture(GL_TEXTURE0 + 9);
	glBindTexture(m_textureType, src_texture);

	m_pixel_format_shaders[convtype ? 1 : 0].Bind();
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindTexture(m_textureType, 0);

	g_renderer->RestoreAPIState();
}

XFBSource::~XFBSource()
{
	glDeleteTextures(1, &texture);
}

void XFBSource::DecodeToTexture(u32 xfbAddr, u32 fbWidth, u32 fbHeight)
{
	TextureConverter::DecodeToTexture(xfbAddr, fbWidth, fbHeight, texture);
}

void XFBSource::CopyEFB(float Gamma)
{
	g_renderer->ResetAPIState();

	// Copy EFB data to XFB and restore render target again
	glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferManager::GetEFBFramebuffer());
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FramebufferManager::GetXFBFramebuffer());

	// Bind texture.
	glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0);

	glBlitFramebuffer(
		0, 0, texWidth, texHeight,
		0, 0, texWidth, texHeight,
		GL_COLOR_BUFFER_BIT, GL_NEAREST
	);

	// Return to EFB.
	FramebufferManager::SetFramebuffer(0);

	g_renderer->RestoreAPIState();

}

XFBSourceBase* FramebufferManager::CreateXFBSource(unsigned int target_width, unsigned int target_height)
{
	GLuint texture;

	glGenTextures(1, &texture);

	glActiveTexture(GL_TEXTURE0 + 9);
	glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
	glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, target_width, target_height, m_EFBLayers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	return new XFBSource(texture, m_EFBLayers);
}

void FramebufferManager::GetTargetSize(unsigned int *width, unsigned int *height, const EFBRectangle& sourceRc)
{
	*width = m_targetWidth;
	*height = m_targetHeight;
}

}  // namespace OGL
