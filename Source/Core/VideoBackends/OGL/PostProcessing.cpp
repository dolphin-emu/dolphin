// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"

#include "VideoBackends/OGL/FramebufferManager.h"
#include "VideoBackends/OGL/GLUtil.h"
#include "VideoBackends/OGL/PostProcessing.h"
#include "VideoBackends/OGL/ProgramShaderCache.h"

#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"

namespace OGL
{

namespace PostProcessing
{

static std::string s_currentShader;
static SHADER s_shader;
static bool s_enable;

static u32 s_width;
static u32 s_height;
static GLuint s_fbo;
static GLuint s_texture;

static GLuint s_uniform_resolution;

static char s_vertex_shader[] =
	"out vec2 uv0;\n"
	"void main(void) {\n"
	"	vec2 rawpos = vec2(gl_VertexID&1, gl_VertexID&2);\n"
	"	gl_Position = vec4(rawpos*2.0-1.0, 0.0, 1.0);\n"
	"	uv0 = rawpos;\n"
	"}\n";

void Init()
{
	m_enable = false;
	m_width = 0;
	m_height = 0;

	glGenFramebuffers(1, &m_fbo);
	glGenTextures(1, &m_texture);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0); // disable mipmaps
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glBindFramebuffer(GL_FRAMEBUFFER, s_fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_texture, 0);
	FramebufferManager::SetFramebuffer(0);
}

void Shutdown()
{
	m_shader.Destroy();

	glDeleteFramebuffers(1, &s_fbo);
	glDeleteTextures(1, &s_texture);
}

void ReloadShader()
{
	s_currentShader = "";
}

void BindTargetFramebuffer ()
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_enable ? s_fbo : 0);
}

void BlitToScreen()
{
	if (!s_enable) return;

	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glViewport(0, 0, s_width, s_height);

	s_shader.Bind();

	glUniform4f(m_uniform_resolution, (float)m_width, (float)m_height, 1.0f/(float)m_width, 1.0f/(float)m_height);
	glUniform1ui(m_uniform_time, (GLuint)m_timer.GetTimeElapsed());

	glUniform4f(s_uniform_resolution, (float)s_width, (float)s_height, 1.0f/(float)s_width, 1.0f/(float)s_height);

	glActiveTexture(GL_TEXTURE0+9);
	glBindTexture(GL_TEXTURE_2D, s_texture);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

/*	glBindFramebuffer(GL_READ_FRAMEBUFFER, s_fbo);

	glBlitFramebuffer(rc.left, rc.bottom, rc.right, rc.top,
			rc.left, rc.bottom, rc.right, rc.top,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);*/
}

void Update ( u32 width, u32 height )
{
	ApplyShader();

	if (s_enable && (width != s_width || height != s_height)) {
		s_width = width;
		s_height = height;

		// alloc texture for framebuffer
		glActiveTexture(GL_TEXTURE0+9);
		glBindTexture(GL_TEXTURE_2D, s_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	}
}

void ApplyShader()
{
	// shader didn't changed
	if (s_currentShader == g_ActiveConfig.sPostProcessingShader) return;
	s_currentShader = g_ActiveConfig.sPostProcessingShader;
	s_enable = false;
	s_shader.Destroy();

	// shader disabled
	if (g_ActiveConfig.sPostProcessingShader == "") return;

	// so need to compile shader

	// loading shader code
	std::string code;
	std::string path = File::GetUserPath(D_SHADERS_IDX) + g_ActiveConfig.sPostProcessingShader + ".glsl";
	if (!File::Exists(path))
	{
		// Fallback to shared user dir
		path = File::GetSysDirectory() + SHADERS_DIR DIR_SEP + g_ActiveConfig.sPostProcessingShader + ".glsl";
	}
	if (!File::ReadFileToString(path, code)) {
		ERROR_LOG(VIDEO, "Post-processing shader not found: %s", path.c_str());
		return;
	}

	// and compile it
	if (!ProgramShaderCache::CompileShader(s_shader, s_vertex_shader, code.c_str())) {
		ERROR_LOG(VIDEO, "Failed to compile post-processing shader %s", s_currentShader.c_str());
		return;
	}

	// read uniform locations
	m_uniform_resolution = glGetUniformLocation(m_shader.glprogid, "resolution");
	m_uniform_time = glGetUniformLocation(m_shader.glprogid, "time");

	for (const auto& it : m_options)
	{
		std::string glsl_name = "option_" + it.first;
		m_uniform_bindings[it.first] = glGetUniformLocation(m_shader.glprogid, glsl_name.c_str());
	}

	// successful
	m_enable = true;
}

void OpenGLPostProcessing::CreateHeader()
{
	m_glsl_header =
		// Required variables
		// Shouldn't be accessed directly by the PP shader
		// Texture sampler
		"SAMPLER_BINDING(8) uniform sampler2D samp8;\n"
		"SAMPLER_BINDING(9) uniform sampler2D samp9;\n"

		// Output variable
		"out float4 ocol0;\n"
		// Input coordinates
		"in float2 uv0;\n"
		// Resolution
		"uniform float4 resolution;\n"
		// Time
		"uniform uint time;\n"

		// Interfacing functions
		"float4 Sample()\n"
		"{\n"
			"\treturn texture(samp9, uv0);\n"
		"}\n"

		"float4 SampleLocation(float2 location)\n"
		"{\n"
			"\treturn texture(samp9, location);\n"
		"}\n"

		"#define SampleOffset(offset) textureOffset(samp9, uv0, offset)\n"

		"float4 SampleFontLocation(float2 location)\n"
		"{\n"
			"\treturn texture(samp8, location);\n"
		"}\n"

		"float2 GetResolution()\n"
		"{\n"
			"\treturn resolution.xy;\n"
		"}\n"

		"float2 GetInvResolution()\n"
		"{\n"
			"\treturn resolution.zw;\n"
		"}\n"

		"float2 GetCoordinates()\n"
		"{\n"
			"\treturn uv0;\n"
		"}\n"

		"uint GetTime()\n"
		"{\n"
			"\treturn time;\n"
		"}\n"

		"void SetOutput(float4 color)\n"
		"{\n"
			"\tocol0 = color;\n"
		"}\n"

		"#define GetOption(x) (option_#x)\n"
		"#define OptionEnabled(x) (option_#x != 0)\n";
}

}  // namespace

}  // namespace OGL
